/*
 * veldlgm.c - Vellum's DIALOG HELPER (/usr/vellum/lib/veldlg): every
 * modal dialog of the editor, run in a spawned process on the EDITOR'S
 * window -- the velxport pattern applied to the dialogs, which is what
 * keeps the editor inside its 64 K text segment (VELLUM.md sec. 21's
 * escape hatch, cashed in when v2 outgrew the wall).
 *
 * The editor forks, points fd 1 at a pipe, and execs
 *	veldlg WID KIND [ARGS...]
 * with the command pipe (fd HR_CMDFD) inherited.  This process adopts
 * the editor's window with hr_attach(WID) -- no server handshake; the
 * editor owns the window and sits BLOCKED on the pipe while its event
 * ring is borrowed -- runs one modal dialog through the ordinary
 * hrdlg kit (the dialog itself is a server overlay, so nothing here
 * draws on the editor's content), prints ONE result line to fd 1 and
 * exits.  Result conventions:
 *	""   (no output)  cancelled / refused
 *	q                 E_QUIT arrived: the window is gone, the editor
 *	                  must clean up and exit
 *	=... the payload  (fields separated by blanks; a free-text tail
 *	                  rides after a TAB so it may contain blanks)
 *
 * KINDs and their argv / result payloads:
 *	confirm MSG                     -> y
 *	text    INIT                    -> =STR
 *	file    SAVE INIT MSG           -> =NAME       (SAVE 0 open, 1 save;
 *	                                   MSG "" or an error to show)
 *	prop    NAME VAL                -> =NAME<TAB>VAL   (NAME "-" = empty)
 *	help                            -> y           (just dismissed)
 *	lib     MSG                     -> =PATH
 *	set     GRID SHW UNUM UNAME VVVV PPP
 *	                                -> =GRID SHW UNUM UNAME VVVV PPP F
 *	                                   (F 1 = the Frame button)
 *	style   FLAGS LAYER SIZE TEXT   -> =FLAGS LAYER SIZE<TAB>TEXT
 */
#include <stdio.h>
#include <types.h>
#include <dir.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"
#include "hrsbar.h"

#define	NAMEL	8		/* the editor's DOBJ field sizes (vellum.h; */
#define	VALL	16		/* not included -- the helper has no model) */
#define	FNLEN	40
#define	UNAMEL	8
#define	USERLIB	"/usr/vellum/etc/symbols"

/* One result line; the editor reads to EOF, so exit right after. */
static
say(s)
char *s;
{
	printf("%s\n", s);
	exit(0);
}

static
sayquit()
{
	say("q");
}

static
saycancel()
{
	say("");
}

/* ------------------------------------------------------------------ */
/* confirm                                                            */
/* ------------------------------------------------------------------ */

char	cfmsg[40];

HRWIDGET cwg[] = {
    { DW_LABEL,   12,  16,   0,  0, cfmsg },
    { DW_BUTTON,  40,  56,  90, DLG_BTNH, "Discard", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  56,  80, DLG_BTNH, "Cancel",  0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NCWG	(sizeof(cwg) / sizeof(cwg[0]))

static
d_confirm(msg)
char *msg;
{
	int w, h, r;

	strncpy(cfmsg, msg, sizeof(cfmsg) - 1);
	cfmsg[sizeof(cfmsg) - 1] = 0;
	w = 300;
	h = 100;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	hr_dlgdraw(cwg, NCWG);
	r = hr_dlgrun(cwg, NCWG);
	hr_dlgclose();
	if ( r == -1 )
		sayquit();
	say(r == 1 ? "y" : "");
}

/* ------------------------------------------------------------------ */
/* one text line                                                      */
/* ------------------------------------------------------------------ */

char	vbuf[VALL];

HRWIDGET twg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Text:" },
    { DW_TEXT,    70,  12, 180, 22, (char *)0, 0, 0, vbuf, sizeof(vbuf) },
    { DW_BUTTON,  60,  46,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  46,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NTWG	(sizeof(twg) / sizeof(twg[0]))
#define	TW_OK	2

static
d_text(init)
char *init;
{
	char out[VALL + 2];
	int w, h, r;

	strncpy(vbuf, init, VALL - 1);
	vbuf[VALL - 1] = 0;
	w = 264;
	h = 90;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	hr_dlgdraw(twg, NTWG);
	r = hr_dlgrun(twg, NTWG);
	hr_dlgclose();
	if ( r == -1 )
		sayquit();
	if ( r != TW_OK || vbuf[0] == 0 )
		saycancel();
	sprintf(out, "=%s", vbuf);
	say(out);
}

/* ------------------------------------------------------------------ */
/* file name (Open / Save)                                            */
/* ------------------------------------------------------------------ */

char	fnbuf[FNLEN];
char	dmsg[36];

HRWIDGET fwg[] = {
    { DW_LABEL,   12,  12,   0,  0, "File name:" },
    { DW_TEXT,    12,  32, 256, 22, (char *)0, 0, 0, fnbuf, sizeof(fnbuf) },
    { DW_LABEL,   12,  62,   0,  0, dmsg },
    { DW_BUTTON,  60,  88,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  88,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NFWG	(sizeof(fwg) / sizeof(fwg[0]))
#define	FW_MSG	2
#define	FW_OK	3

/* The editor validates the actual open/save (it owns the model); a failed
 * attempt respawns this dialog with the error text in MSG.  The only
 * in-dialog retry left is the empty name. */
static
d_file(init, msg)
char *init, *msg;
{
	char out[FNLEN + 2];
	int w, h, r;

	strncpy(fnbuf, init, FNLEN - 1);
	fnbuf[FNLEN - 1] = 0;
	strncpy(dmsg, msg, sizeof(dmsg) - 1);
	dmsg[sizeof(dmsg) - 1] = 0;
	w = 280;
	h = 132;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	for (;;)
	{
		cl_fillrect(fwg[FW_MSG].dw_x, fwg[FW_MSG].dw_y, w,
			    fwg[FW_MSG].dw_y + hr_font(SHM_FUI)->cellh, 1);
		hr_dlgdraw(fwg, NFWG);
		r = hr_dlgrun(fwg, NFWG);
		if ( r == -1 )
		{
			hr_dlgclose();
			sayquit();
		}
		if ( r != FW_OK )
		{
			hr_dlgclose();
			saycancel();
		}
		if ( fnbuf[0] )
			break;
		strcpy(dmsg, "Enter a file name");
	}
	hr_dlgclose();
	sprintf(out, "=%s", fnbuf);
	say(out);
}

/* ------------------------------------------------------------------ */
/* symbol name / value                                                */
/* ------------------------------------------------------------------ */

char	nbuf[NAMEL];

HRWIDGET nwg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Name:" },
    { DW_TEXT,    78,  12,  90, 22, (char *)0, 0, 0, nbuf, sizeof(nbuf) },
    { DW_LABEL,   12,  46,   0,  0, "Value:" },
    { DW_TEXT,    78,  42, 160, 22, (char *)0, 0, 0, vbuf, sizeof(vbuf) },
    { DW_BUTTON,  60,  76,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  76,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NNWG	(sizeof(nwg) / sizeof(nwg[0]))
#define	NW_OK	4

static
d_prop(name, val)
char *name, *val;
{
	char out[NAMEL + VALL + 4];
	int w, h, r;

	if ( strcmp(name, "-") != 0 )
	{
		strncpy(nbuf, name, NAMEL - 1);
		nbuf[NAMEL - 1] = 0;
	}
	if ( strcmp(val, "-") != 0 )
	{
		strncpy(vbuf, val, VALL - 1);
		vbuf[VALL - 1] = 0;
	}
	w = 264;
	h = 120;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	hr_dlgdraw(nwg, NNWG);
	r = hr_dlgrun(nwg, NNWG);
	hr_dlgclose();
	if ( r == -1 )
		sayquit();
	if ( r != NW_OK )
		saycancel();
	sprintf(out, "=%s\t%s", nbuf, vbuf);
	say(out);
}

/* ------------------------------------------------------------------ */
/* help                                                               */
/* ------------------------------------------------------------------ */

HRWIDGET hwg[] = {
    { DW_LABEL,  12,  10, 0, 0, "Palette header cycles groups: the" },
    { DW_LABEL,  12,  28, 0, 0, "libraries, then 'shapes' (stretch" },
    { DW_LABEL,  12,  46, 0, 0, "shapes, connectors, arc, net, dim)." },
    { DW_LABEL,  12,  72, 0, 0, "Wire/conn/dim: drag or click ends;" },
    { DW_LABEL,  12,  90, 0, 0, "Line chains into a polyline.  ESC or" },
    { DW_LABEL,  12, 108, 0, 0, "an anchor-click ends a run." },
    { DW_LABEL,  12, 134, 0, 0, "Sel: click picks (a group whole)," },
    { DW_LABEL,  12, 152, 0, 0, "drag moves; corner/end grips drag." },
    { DW_LABEL,  12, 178, 0, 0, "u undo  v fit  f front  j/J group/un" },
    { DW_LABEL,  12, 196, 0, 0, "1-6 align; r/m turn/flip selection." },
    { DW_LABEL,  12, 222, 0, 0, "Name: style/fill/layer dialog." },
    { DW_LABEL,  12, 240, 0, 0, "Middle drag pans, click pastes;" },
    { DW_LABEL,  12, 258, 0, 0, "< > keys or cells switch sheets." },
    { DW_LABEL,  12, 284, 0, 0, "F2 Save  F3 Open  F4 New  F8 Set" },
    { DW_LABEL,  12, 302, 0, 0, "F5/F6/F7 Cut/Copy/Paste  F11 Help" },
    { DW_BUTTON, 150, 332, 70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NHWG	(sizeof(hwg) / sizeof(hwg[0]))

static
d_help()
{
	int w, h, r;

	w = 360;
	h = 368;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	hr_dlgdraw(hwg, NHWG);
	r = hr_dlgrun(hwg, NHWG);
	hr_dlgclose();
	if ( r == -1 )
		sayquit();
	say("y");
}

/* ------------------------------------------------------------------ */
/* the scrollable library chooser                                     */
/* ------------------------------------------------------------------ */

#define	LIBDIR	"/usr/vellum/lib"
#define	NDLGL	16		/* list capacity                          */
#define	LVIS	6		/* rows visible at once                   */
#define	LROWH	16
#define	LLX	30		/* rows: right of the scrollbar           */
#define	LLX2	268
#define	LLY	28
#define	LMSGY	(LLY + LVIS * LROWH + 8)
#define	LBTNY	(LMSGY + 22)

char	lbmsg[36];
char	dlname[NDLGL][16];	/* list labels: the file names            */
char	dlpath[NDLGL][44];	/* their full paths                       */
int	nls, lsel;
HRSBAR	lsb;

static
lrow(i)
{
	int y;

	if ( i < lsb.sb_pos || i >= lsb.sb_pos + LVIS || i >= nls )
		return 0;
	y = LLY + (i - lsb.sb_pos) * LROWH;
	cl_fillrect(LLX, y, LLX2, y + LROWH, 1);
	cl_ptext(SHM_FUI, LLX + 4, y, dlname[i]);
	if ( i == lsel )
		cl_fillrect(LLX, y, LLX2, y + LROWH, 2);
	return 0;
}

static
lrows()
{
	register int i;
	int y;

	for ( i = lsb.sb_pos; i < lsb.sb_pos + LVIS; i++ )
		if ( i < nls )
			lrow(i);
		else
		{
			y = LLY + (i - lsb.sb_pos) * LROWH;
			cl_fillrect(LLX, y, LLX2, y + LROWH, 1);
		}
	return 0;
}

static
lmsg()
{
	cl_fillrect(12, LMSGY, LLX2, LMSGY + 16, 1);
	cl_ptext(SHM_FUI, 12, LMSGY, lbmsg);
	return 0;
}

/* A dialog-kit-look button (border + mini drop shadow + centred label). */
static
lbtn(x, w, label)
char *label;
{
	cl_fillrect(x, LBTNY, x + w, LBTNY + DLG_BTNH, 1);
	cl_line(x, LBTNY, x + w - 1, LBTNY, 0);
	cl_line(x + w - 1, LBTNY, x + w - 1, LBTNY + DLG_BTNH - 1, 0);
	cl_line(x + w - 1, LBTNY + DLG_BTNH - 1, x, LBTNY + DLG_BTNH - 1, 0);
	cl_line(x, LBTNY + DLG_BTNH - 1, x, LBTNY, 0);
	cl_fillrect(x + DLG_BSHAD, LBTNY + DLG_BTNH,
		    x + w + DLG_BSHAD, LBTNY + DLG_BTNH + DLG_BSHAD, 0);
	cl_fillrect(x + w, LBTNY + DLG_BSHAD,
		    x + w + DLG_BSHAD, LBTNY + DLG_BTNH + DLG_BSHAD, 0);
	cl_ptext(SHM_FUI, x + (w - strlen(label) * 9) / 2 + 1,
		 LBTNY + (DLG_BTNH - 16) / 2 + 1, label);
	return 0;
}

/* Scroll the selection into view; 1 if the view moved. */
static
lshow()
{
	if ( lsel < lsb.sb_pos )
	{
		lsb.sb_pos = lsel;
		return 1;
	}
	if ( lsel >= lsb.sb_pos + LVIS )
	{
		lsb.sb_pos = lsel - LVIS + 1;
		return 1;
	}
	return 0;
}

/* Accept the selected entry if the file at least OPENS (the editor does
 * the real loadlib and respawns with a message if that still fails). */
static
lpick()
{
	register FILE *fp;
	char out[48];

	if ( (fp = fopen(dlpath[lsel], "r")) == (FILE *)0 )
	{
		strcpy(lbmsg, "Cannot load that library");
		lmsg();
		return 0;
	}
	fclose(fp);
	hr_dlgclose();
	sprintf(out, "=%s", dlpath[lsel]);
	say(out);
	return 1;	/* not reached */
}

static
d_lib(msg)
char *msg;
{
	register FILE *dirfile;
	struct direct dent;
	char nm[DIRSIZ + 1];
	WMSG e;
	int w, h, r, i, x, y;

	nls = 0;
	if ( (dirfile = fopen(LIBDIR, "r")) != (FILE *)0 )
	{
		while ( nls < NDLGL - 1 &&
			fread((char *)&dent, sizeof(dent), 1, dirfile) == 1 )
		{
			if ( dent.d_ino == 0 )
				continue;
			for ( i = 0; i < DIRSIZ; i++ )
				nm[i] = dent.d_name[i];
			nm[DIRSIZ] = 0;
			if ( nm[0] == '.' || strlen(nm) > 14 )
				continue;
			strcpy(dlname[nls], nm);
			sprintf(dlpath[nls], "%s/%s", LIBDIR, nm);
			nls++;
		}
		fclose(dirfile);
	}
	strcpy(dlname[nls], "symbols (user)");
	strcpy(dlpath[nls], USERLIB);
	nls++;
	lsel = 0;
	strncpy(lbmsg, msg, sizeof(lbmsg) - 1);
	lbmsg[sizeof(lbmsg) - 1] = 0;

	w = 280;
	h = LBTNY + DLG_BTNH + DLG_BSHAD + 10;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();

	lsb.sb_x = 10;
	lsb.sb_y = LLY;
	lsb.sb_h = LVIS * LROWH;
	lsb.sb_total = nls;
	lsb.sb_page = LVIS;
	lsb.sb_pos = 0;
	lsb.sb_drag = 0;

	cl_ptext(SHM_FUI, 12, 6, "Load library:");
	cl_line(8, LLY - 2, LLX2 + 2, LLY - 2, 0);	/* the list frame */
	cl_line(LLX2 + 2, LLY - 2, LLX2 + 2, LLY + LVIS * LROWH + 1, 0);
	cl_line(LLX2 + 2, LLY + LVIS * LROWH + 1, 8, LLY + LVIS * LROWH + 1, 0);
	cl_line(8, LLY + LVIS * LROWH + 1, 8, LLY - 2, 0);
	lrows();
	lmsg();
	hr_sbdraw(&lsb, 1);
	lbtn(60, 70, "OK");
	lbtn(160, 80, "Cancel");

	for (;;)
	{
		hr_evwait(hr_wid());
		while ( hr_evget(hr_wid(), (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_DBUTTON:
				x = e.wm_arg[0];
				y = e.wm_arg[1];
				if ( !(e.wm_arg[2] & EB_LEFT) )
				{
					if ( lsb.sb_drag )
						hr_sbrelease(&lsb);
					break;
				}
				if ( hr_sbhit(&lsb, x, y) )
				{
					if ( hr_sbpress(&lsb, y) )
					{
						lrows();
						hr_sbdraw(&lsb, 0);
					}
				}
				else if ( x >= LLX && x < LLX2 && y >= LLY &&
					  y < LLY + LVIS * LROWH )
				{
					i = lsb.sb_pos + (y - LLY) / LROWH;
					if ( i < nls && i != lsel )
					{
						r = lsel;
						lsel = i;
						lrow(r);
						lrow(i);
					}
					else if ( i == lsel )
						lpick();	/* 2nd click */
				}
				else if ( y >= LBTNY &&
					  y < LBTNY + DLG_BTNH )
				{
					if ( x >= 60 && x < 130 )
						lpick();
					else if ( x >= 160 && x < 240 )
					{
						hr_dlgclose();
						saycancel();
					}
				}
				break;

			case E_DMOTION:
				if ( lsb.sb_drag &&
				     hr_sbmotion(&lsb, e.wm_arg[1]) )
				{
					lrows();
					hr_sbdraw(&lsb, 0);
				}
				break;

			case E_DKEY:
				i = e.wm_arg[0] & 0xff;
				if ( i == 0x0e && lsel < nls - 1 )
					lsel++;
				else if ( i == 0x10 && lsel > 0 )
					lsel--;
				else if ( i == '\r' || i == '\n' )
				{
					lpick();
					break;
				}
				else if ( i == 0x1b )
				{
					hr_dlgclose();
					saycancel();
				}
				else
					break;
				if ( lshow() )
				{
					lrows();
					hr_sbdraw(&lsb, 0);
				}
				else
				{
					lrow(lsel == 0 ? 1 : lsel - 1);
					lrow(lsel + 1);
					lrow(lsel);
				}
				break;

			case E_QUIT:
				sayquit();
			}
		}
	}
}

/* ------------------------------------------------------------------ */
/* Settings: grid pitch, sheet preset, units, layer vis/print         */
/* ------------------------------------------------------------------ */

char	unbuf[6];
char	unmbuf[UNAMEL];

HRWIDGET swg[] = {
    { DW_LABEL,   12,  10,   0,  0, "Grid:" },
    { DW_RADIO,   90,  10,   0,  0, "1",       0, 1 },
    { DW_RADIO,  150,  10,   0,  0, "2",       0, 1 },
    { DW_LABEL,   12,  36,   0,  0, "Sheet:" },
    { DW_RADIO,   90,  36,   0,  0, "160x120", 0, 2 },
    { DW_RADIO,  210,  36,   0,  0, "A4",      0, 2 },
    { DW_LABEL,   12,  62,   0,  0, "Units:" },
    { DW_TEXT,    90,  58,  60, 22, (char *)0, 0, 0, unbuf, sizeof(unbuf) },
    { DW_TEXT,   170,  58,  90, 22, (char *)0, 0, 0, unmbuf,
      sizeof(unmbuf) },
    { DW_LABEL,  130,  88,   0,  0, "Vis Prn" },
    { DW_LABEL,   12, 110,   0,  0, "drawing" },
    { DW_CHECK,  130, 110,   0,  0, "",        0, 0 },
    { DW_CHECK,  180, 110,   0,  0, "",        0, 0 },
    { DW_LABEL,   12, 132,   0,  0, "annotation" },
    { DW_CHECK,  130, 132,   0,  0, "",        0, 0 },
    { DW_CHECK,  180, 132,   0,  0, "",        0, 0 },
    { DW_LABEL,   12, 154,   0,  0, "frame" },
    { DW_CHECK,  130, 154,   0,  0, "",        0, 0 },
    { DW_CHECK,  180, 154,   0,  0, "",        0, 0 },
    { DW_LABEL,   12, 176,   0,  0, "construction" },
    { DW_CHECK,  130, 176,   0,  0, "",        0, 0 },
    { DW_BUTTON,  12, 208, 100, DLG_BTNH, "Frame",  0, 0, (char *)0, 0,
      DWF_END },
    { DW_BUTTON, 130, 208,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 220, 208,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NSWG		(sizeof(swg) / sizeof(swg[0]))
#define	SW_G1		1
#define	SW_G2		2
#define	SW_S160		4
#define	SW_SA4		5
#define	SW_UN		7
#define	SW_UNM		8
#define	SW_V0		11
#define	SW_P0		12
#define	SW_V1		14
#define	SW_P1		15
#define	SW_V2		17
#define	SW_P2		18
#define	SW_V3		20
#define	SW_FRAME	21
#define	SW_OK		22

/* argv: GRID SHW UNUM UNAME VVVV PPP -- UNAME "-" = none, VVVV/PPP are
 * '0'/'1' digit strings for the four vis / three prn checkboxes. */
static
d_set(av)
char **av;
{
	char out[64];
	int w, h, r, i;
	char *vv, *pp;

	swg[SW_G1].dw_val = atoi(av[0]) == 1;
	swg[SW_G2].dw_val = atoi(av[0]) == 2;
	swg[SW_S160].dw_val = atoi(av[1]) == 160;
	swg[SW_SA4].dw_val = atoi(av[1]) != 160;
	strncpy(unbuf, av[2], sizeof(unbuf) - 1);
	unbuf[sizeof(unbuf) - 1] = 0;
	if ( strcmp(av[3], "-") != 0 )
	{
		strncpy(unmbuf, av[3], UNAMEL - 1);
		unmbuf[UNAMEL - 1] = 0;
	}
	vv = av[4];
	pp = av[5];
	swg[SW_V0].dw_val = vv[0] == '1';
	swg[SW_V1].dw_val = vv[1] == '1';
	swg[SW_V2].dw_val = vv[2] == '1';
	swg[SW_V3].dw_val = vv[3] == '1';
	swg[SW_P0].dw_val = pp[0] == '1';
	swg[SW_P1].dw_val = pp[1] == '1';
	swg[SW_P2].dw_val = pp[2] == '1';
	w = 320;
	h = 208 + DLG_BTNH + DLG_BSHAD + 10;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	hr_dlgdraw(swg, NSWG);
	r = hr_dlgrun(swg, NSWG);
	hr_dlgclose();
	if ( r == -1 )
		sayquit();
	if ( r != SW_OK && r != SW_FRAME )
		saycancel();
	i = atoi(unbuf);
	if ( i < 1 )
		i = 1;
	sprintf(out, "=%d %d %d %s %c%c%c%c %c%c%c %d",
		swg[SW_G2].dw_val ? 2 : 1,
		swg[SW_SA4].dw_val ? 120 : 160,
		i, unmbuf[0] ? unmbuf : "-",
		'0' + swg[SW_V0].dw_val, '0' + swg[SW_V1].dw_val,
		'0' + swg[SW_V2].dw_val, '0' + swg[SW_V3].dw_val,
		'0' + swg[SW_P0].dw_val, '0' + swg[SW_P1].dw_val,
		'0' + swg[SW_P2].dw_val,
		r == SW_FRAME);
	say(out);
}

/* ------------------------------------------------------------------ */
/* Style: line style / fill / bold / hatch / layer / text / size /    */
/* vertical                                                           */
/* ------------------------------------------------------------------ */

char	stbuf[VALL];

HRWIDGET ywg[] = {
    { DW_LABEL,   12,  10,   0,  0, "Style:" },
    { DW_RADIO,   90,  10,   0,  0, "Solid",  0, 1 },
    { DW_RADIO,  180,  10,   0,  0, "Dash",   0, 1 },
    { DW_RADIO,  264,  10,   0,  0, "Dot",    0, 1 },
    { DW_LABEL,   12,  36,   0,  0, "Fill:" },
    { DW_RADIO,   90,  36,   0,  0, "None",   0, 2 },
    { DW_RADIO,  180,  36,   0,  0, "White",  0, 2 },
    { DW_RADIO,  264,  36,   0,  0, "Gray",   0, 2 },
    { DW_RADIO,  336,  36,   0,  0, "Black",  0, 2 },
    { DW_CHECK,   12,  62,   0,  0, "Bold" },
    { DW_CHECK,  100,  62,   0,  0, "Hatch" },
    { DW_LABEL,  200,  62,   0,  0, "Layer:" },
    { DW_RADIO,  270,  62,   0,  0, "0",      0, 3 },
    { DW_RADIO,  310,  62,   0,  0, "1",      0, 3 },
    { DW_RADIO,  350,  62,   0,  0, "2",      0, 3 },
    { DW_RADIO,  390,  62,   0,  0, "3",      0, 3 },
    { DW_LABEL,   12,  90,   0,  0, "Text:" },
    { DW_TEXT,    90,  86, 200, 22, (char *)0, 0, 0, stbuf, sizeof(stbuf) },
    { DW_LABEL,   12, 118,   0,  0, "Size:" },
    { DW_RADIO,   90, 118,   0,  0, "S",      0, 4 },
    { DW_RADIO,  140, 118,   0,  0, "M",      0, 4 },
    { DW_RADIO,  190, 118,   0,  0, "L",      0, 4 },
    { DW_CHECK,  250, 118,   0,  0, "Vert" },
    { DW_CHECK,  330, 118,   0,  0, "Smooth" },
    { DW_BUTTON,  90, 148,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 200, 148,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NYWG	(sizeof(ywg) / sizeof(ywg[0]))
#define	YW_SOL	1
#define	YW_DSH	2
#define	YW_DOT	3
#define	YW_FN	5
#define	YW_FW	6
#define	YW_FG	7
#define	YW_FB	8
#define	YW_BOLD	9
#define	YW_HAT	10
#define	YW_L0	12
#define	YW_TXT	17
#define	YW_SZS	19
#define	YW_SZM	20
#define	YW_SZL	21
#define	YW_VERT	22
#define	YW_SMOO	23
#define	YW_OK	24

/* o_flags bits (vellum.h; kept in step by hand -- the helper carries no
 * model header on purpose) */
#define	F_STYLE	0x03
#define	F_DASH	0x01
#define	F_DOT	0x02
#define	F_FILL	0x0c
#define	F_FILLW	0x04
#define	F_FILLG	0x08
#define	F_FILLB	0x0c
#define	F_BOLD	0x10
#define	F_HATCH	0x20
#define	F_SMOOTH 0x40
#define	F_VERT	0x80

/* argv: FLAGS LAYER SIZE TEXT ("-" = the object has no text) */
static
d_style(av)
char **av;
{
	char out[VALL + 24];
	int w, h, r, f, hastext;

	f = atoi(av[0]);
	ywg[YW_SOL].dw_val = (f & F_STYLE) == 0;
	ywg[YW_DSH].dw_val = (f & F_STYLE) == F_DASH;
	ywg[YW_DOT].dw_val = (f & F_STYLE) == F_DOT;
	ywg[YW_FN].dw_val = (f & F_FILL) == 0;
	ywg[YW_FW].dw_val = (f & F_FILL) == F_FILLW;
	ywg[YW_FG].dw_val = (f & F_FILL) == F_FILLG;
	ywg[YW_FB].dw_val = (f & F_FILL) == F_FILLB;
	ywg[YW_BOLD].dw_val = (f & F_BOLD) != 0;
	ywg[YW_HAT].dw_val = (f & F_HATCH) != 0;
	ywg[YW_VERT].dw_val = (f & F_VERT) != 0;
	ywg[YW_SMOO].dw_val = (f & F_SMOOTH) != 0;
	for ( r = 0; r < 4; r++ )
		ywg[YW_L0 + r].dw_val = atoi(av[1]) == r;
	ywg[YW_SZS].dw_val = atoi(av[2]) == 0;
	ywg[YW_SZM].dw_val = atoi(av[2]) == 1;
	ywg[YW_SZL].dw_val = atoi(av[2]) >= 2;
	hastext = strcmp(av[3], "-") != 0;
	if ( hastext )
	{
		strncpy(stbuf, av[3], VALL - 1);
		stbuf[VALL - 1] = 0;
	}
	w = 430;
	h = 148 + DLG_BTNH + DLG_BSHAD + 10;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		sayquit();
	if ( r < 0 )
		saycancel();
	hr_dlgdraw(ywg, NYWG);
	r = hr_dlgrun(ywg, NYWG);
	hr_dlgclose();
	if ( r == -1 )
		sayquit();
	if ( r != YW_OK )
		saycancel();
	f = 0;
	if ( ywg[YW_DSH].dw_val )	f |= F_DASH;
	if ( ywg[YW_DOT].dw_val )	f |= F_DOT;
	if ( ywg[YW_FW].dw_val )	f |= F_FILLW;
	if ( ywg[YW_FG].dw_val )	f |= F_FILLG;
	if ( ywg[YW_FB].dw_val )	f |= F_FILLB;
	if ( ywg[YW_BOLD].dw_val )	f |= F_BOLD;
	if ( ywg[YW_HAT].dw_val )	f |= F_HATCH;
	if ( ywg[YW_VERT].dw_val )	f |= F_VERT;
	if ( ywg[YW_SMOO].dw_val )	f |= F_SMOOTH;
	sprintf(out, "=%d %d %d\t%s", f,
		ywg[YW_L0+1].dw_val ? 1 : ywg[YW_L0+2].dw_val ? 2 :
		ywg[YW_L0+3].dw_val ? 3 : 0,
		ywg[YW_SZS].dw_val ? 0 : ywg[YW_SZM].dw_val ? 1 : 2,
		stbuf);
	say(out);
}

/* ------------------------------------------------------------------ */
/* entry                                                              */
/* ------------------------------------------------------------------ */

main(argc, argv)
char **argv;
{
	register char *k;

	if ( argc < 3 )
		exit(1);
	hr_attach(atoi(argv[1]));
	k = argv[2];
	if ( strcmp(k, "confirm") == 0 && argc >= 4 )
		d_confirm(argv[3]);
	else if ( strcmp(k, "text") == 0 && argc >= 4 )
		d_text(argv[3]);
	else if ( strcmp(k, "file") == 0 && argc >= 6 )
		d_file(argv[4], argv[5]);
	else if ( strcmp(k, "prop") == 0 && argc >= 5 )
		d_prop(argv[3], argv[4]);
	else if ( strcmp(k, "help") == 0 )
		d_help();
	else if ( strcmp(k, "lib") == 0 && argc >= 4 )
		d_lib(argv[3]);
	else if ( strcmp(k, "set") == 0 && argc >= 9 )
		d_set(&argv[3]);
	else if ( strcmp(k, "style") == 0 && argc >= 7 )
		d_style(&argv[3]);
	exit(1);
}
