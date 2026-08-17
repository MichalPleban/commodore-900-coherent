/*
 * zfile.c - a ZView file manager.
 *
 * A browser that also opens files in the editor -- it does not RUN programs
 * (launch those from a shell).  One window on one directory:
 *   - a row of BUTTONS: Edit, Copy, Move, Delete, Mkdir, Go,
 *     with the entry count on the right;
 *   - a PATH line naming the directory being shown;
 *   - a scrollable LIST of its entries -- mode, name, size, date, the way
 *     ls -l prints them -- directories first, then files, ".." on top.
 * The list has the common vertical scrollbar (hrsbar) on the LEFT edge; the
 * layout, diff renderer and event loop are zprint's.
 *
 * A click selects; a second click on the selected DIRECTORY (or Enter, or BS
 * for "..") enters it -- the process chdir()s into what it shows, so every
 * operation works on plain names.  Edit hands the selected file to the editor
 * (/usr/hr/bin/zedit <name>), which opens with the command pipe (fd 4) kept
 * open so it gets a window of its own.
 *
 * Copy is done in-process (read/write); Move is link+unlink, so it stays
 * within one file system and says so when it cannot; Delete unlinks a file
 * and runs /bin/rmdir (setuid) for an EMPTY directory; Mkdir runs /bin/mkdir.
 * All of them confirm or complain through the dialog kit, and the directory
 * is polled on a SIGALRM tick (by mtime, one stat) so outside changes show
 * up by themselves.
 *
 * Keys: ^P/^N (the arrows) move the selection, ^Z/^V (PgUp/PgDn; also ^B/^F,
 * space) page the list, ^A/^E (keypad Home/End) and Clear/Home jump to the
 * top/bottom, Enter enters a directory, BS goes up; e/c/m/d/k/g press the six
 * buttons, as do F2..F7 in the same order, and the C900 Help key (F11) or the
 * window-menu Help entry puts up a dialog listing all of this.
 */
#include <stdio.h>
#include <types.h>
#include <stat.h>
#include <dir.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrsbar.h"
#include "hrdlg.h"

extern char	*ctime();
extern char	*strrchr();
extern long	time();

#define	EDITOR	"/usr/hr/bin/zedit"
#define	MKDIRCMD "/bin/mkdir"
#define	RMDIRCMD "/bin/rmdir"

/* View grid ceilings: the biggest full-screen window at the 8x15 cell. */
#define	MAXROWS	52
#define	MAXCOLS	126

/* The entry table. */
#define	MAXF	160
#define	NNAME	15		/* DIRSIZ + NUL                           */
#define	NPATH	120

/* Button-bar metrics (UI font, dialog-button chrome). */
#define	BARH	32		/* button bar height                      */
#define	PATHH	20		/* the path line under it                 */
#define	BTNY	3		/* button top                             */
#define	BTNPAD	10		/* label side padding inside a button     */
#define	BTNGAP	12		/* between buttons                        */
#define	MAXBTN	6

HRAPP	me = { "Files", "file.icn", 0, 0, HRF_STRETCH, 0, 0, HRM_HELP };

int	mywid;
int	cellw, cellh;		/* terminal-font cell (the list)          */
int	fcw, fch;		/* UI-font cell (buttons, bar text)       */
int	xpix;			/* list-grid offset right of the bar      */
int	contw, conth;		/* granted content size, px               */
int	ly0, lrows;		/* list pane: top px, visible rows        */
int	cols;			/* text columns in the list               */

/* ---- the directory shown ---- */
char	cwd[NPATH];		/* always absolute, no trailing slash     */
long	cwdmtime;		/* its mtime as last scanned              */
long	now;			/* time() at the last rescan              */

/* ---- the entry table, rebuilt by rescan() ---- */
struct file {
	char	nm[NNAME];
	long	size;
	long	mtim;
	unsigned short mode;
};
struct file files[MAXF];
int	nfil;			/* entries in the table (incl. "..")      */
int	trunc;			/* 1 = directory had more than MAXF       */
int	self = -1;		/* selected entry, -1 = none              */
int	ltop;			/* first visible list row                 */
char	selname[NNAME];		/* keep the selection across rescans      */

/* the open-on-second-click state */
long	clickt;			/* time() of the last list click          */
int	clickrow = -1;		/* entry it selected                      */

/* ---- rendering (zprint's diff scheme) ---- */
char	disp[MAXROWS][MAXCOLS];	/* what is on screen; 0 = needs paint     */
int	hlon;			/* 1 = list highlight painted             */
int	hlrow;			/* list row it is on                      */
int	chromedirty = 1;	/* 1 = repaint bar + path wholesale       */

HRSBAR	sbl;
int	sblforce = 1;
int	pollflag;		/* SIGALRM: check the directory           */

static char vbuf[MAXCOLS];	/* view-row expansion buffer              */

/* ------------------------------------------------------------------ */
/* the directory scan                                                 */
/* ------------------------------------------------------------------ */

static
atroot()
{
	return cwd[0] == '/' && cwd[1] == 0;
}

/* dirs first, then files, alphabetical within each. */
static
fcmp(a, b)
struct file *a, *b;
{
	register int da, db;

	da = (a->mode & S_IFMT) == S_IFDIR;
	db = (b->mode & S_IFMT) == S_IFDIR;
	if ( da != db )
		return db - da;
	return strcmp(a->nm, b->nm);
}

/* Scan the current directory into the entry table.  ".." is entry 0 except
 * at the root.  Returns 1 when anything shown may have changed. */
static
rescan()
{
	char nm[NNAME];
	struct direct dir;
	struct stat sb;
	register FILE *dirfile;
	register int i;
	int base, onf;

	onf = nfil;
	nfil = 0;
	trunc = 0;
	time(&now);
	if ( stat(".", &sb) == 0 )
		cwdmtime = sb.st_mtime;

	if ( !atroot() )
	{
		strcpy(files[0].nm, "..");
		files[0].size = 0;
		files[0].mtim = 0;
		files[0].mode = S_IFDIR | 0777;
		nfil = 1;
	}
	base = nfil;

	if ( (dirfile = fopen(".", "r")) != 0 )
	{
		while ( fread((char *)&dir, sizeof(dir), 1, dirfile) == 1 )
		{
			if ( dir.d_ino == 0 )
				continue;
			for ( i = 0; i < DIRSIZ; i++ )
				nm[i] = dir.d_name[i];
			nm[DIRSIZ] = 0;
			if ( strcmp(nm, ".") == 0 || strcmp(nm, "..") == 0 )
				continue;
			if ( nfil >= MAXF )
			{
				trunc = 1;
				break;
			}
			strcpy(files[nfil].nm, nm);
			files[nfil].size = 0;
			files[nfil].mtim = 0;
			files[nfil].mode = 0;
			if ( stat(nm, &sb) == 0 )
			{
				files[nfil].size = sb.st_size;
				files[nfil].mtim = sb.st_mtime;
				files[nfil].mode = sb.st_mode;
			}
			nfil++;
		}
		fclose(dirfile);
	}
	qsort((char *)&files[base], nfil - base, sizeof(struct file), fcmp);

	/* keep the selection on the same name if it is still there */
	self = -1;
	for ( i = 0; i < nfil; i++ )
		if ( strcmp(files[i].nm, selname) == 0 )
		{
			self = i;
			break;
		}
	if ( self < 0 && nfil > 0 )
	{
		self = 0;
		strcpy(selname, files[0].nm);
	}
	if ( self < 0 )
		selname[0] = 0;
	chromedirty = 1;
	return nfil != onf || 1;
}

/* ------------------------------------------------------------------ */
/* the view: one text grid over the entry list                        */
/* ------------------------------------------------------------------ */

/* "drwxrwxrwx" from a mode, ls's rendering (setuid/setgid/sticky included). */
static
modestr(m, out)
unsigned m;
char *out;
{
	register int i;

	strcpy(out, "----------");
	switch ( m & S_IFMT )
	{
	case S_IFDIR:	out[0] = 'd';	break;
	case S_IFCHR:	out[0] = 'c';	break;
	case S_IFBLK:	out[0] = 'b';	break;
	case S_IFPIP:	out[0] = 'p';	break;
	}
	for ( i = 0; i < 3; i++ )
	{
		if ( m & (0400 >> (i * 3)) )	out[1 + i * 3] = 'r';
		if ( m & (0200 >> (i * 3)) )	out[2 + i * 3] = 'w';
		if ( m & (0100 >> (i * 3)) )	out[3 + i * 3] = 'x';
	}
	if ( m & S_ISUID )	out[3] = 's';
	if ( m & S_ISGID )	out[6] = 's';
	if ( m & S_ISVTX )	out[9] = 't';
	return 0;
}

/* "Mmm dd hh:mm" for a recent time, "Mmm dd  yyyy" for an old one (ls). */
static
datestr(t, out)
long t;
char *out;
{
	register char *p;

	if ( t == 0 )
	{
		strcpy(out, "            ");
		return 0;
	}
	p = ctime(&t);
	if ( now - t > 180L * 24 * 3600 || t > now )
	{
		sprintf(out, "%.6s  %.4s", p + 4, p + 20);
		return 0;
	}
	sprintf(out, "%.12s", p + 4);
	return 0;
}

/* Text of view row r (cols cells, blank-padded). */
static char *
vrow(r)
{
	register char *p;
	register int i;
	int li;
	char mb[12], db[16], nb[NNAME + 2], zb[10];
	char lbuf[MAXCOLS + 16];

	for ( i = 0; i < cols; i++ )
		vbuf[i] = ' ';
	li = ltop + r;
	if ( li < 0 || li >= nfil )
	{
		if ( li == 0 && nfil == 0 )
		{
			p = "(empty directory)";
			for ( i = 0; p[i] && i < cols; i++ )
				vbuf[i] = p[i];
		}
		return vbuf;
	}
	modestr(files[li].mode, mb);
	datestr(files[li].mtim, db);
	strcpy(nb, files[li].nm);
	if ( (files[li].mode & S_IFMT) == S_IFDIR )
		strcat(nb, "/");
	if ( (files[li].mode & S_IFMT) == S_IFDIR )
		strcpy(zb, "        ");
	else
		sprintf(zb, "%8ld", files[li].size);
	sprintf(lbuf, "%s  %-15.15s %s  %s", mb, nb, zb, db);
	for ( i = 0; lbuf[i] && i < cols; i++ )
		vbuf[i] = lbuf[i];
	return vbuf;
}

static
invalidate()
{
	register int r, c;

	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;
	hlon = 0;
	sblforce = 1;
	chromedirty = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the list highlight (XOR, zprint's scheme)                          */
/* ------------------------------------------------------------------ */

static
hldraw()
{
	int r;

	if ( self < 0 )
		return 0;
	r = self - ltop;
	if ( r < 0 || r >= lrows )
		return 0;
	cl_fillrect(xpix, ly0 + r * cellh, contw, ly0 + (r + 1) * cellh, 2);
	hlon = 1;  hlrow = r;
	return 0;
}

static
hlerase()
{
	if ( hlon )
	{
		cl_fillrect(xpix, ly0 + hlrow * cellh,
			    contw, ly0 + (hlrow + 1) * cellh, 2);
		hlon = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* the button bar + path line                                         */
/* ------------------------------------------------------------------ */

int	nbtn;
int	bx[MAXBTN], bw[MAXBTN];
char	*blab[MAXBTN];
int	barmed = -1;		/* armed (pressed) button, -1 = none      */
int	barin;			/* 1 = pointer currently inside it        */

/* Draw one button: white face, 1px border, 2px drop shadow, centred label
 * (the 9x16 UI glyphs sit 1px high-left in their cell, so centre +1,+1). */
static
drawbtn(i)
{
	int x, w, tx, ty;

	x = bx[i];  w = bw[i];
	cl_fillrect(x, BTNY, x + w, BTNY + DLG_BTNH, 1);
	cl_fillrect(x, BTNY, x + w, BTNY + 1, 0);
	cl_fillrect(x, BTNY + DLG_BTNH - 1, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x, BTNY, x + 1, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + w - 1, BTNY, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + 2, BTNY + DLG_BTNH, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	cl_fillrect(x + w, BTNY + 2, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	tx = x + (w - strlen(blab[i]) * fcw) / 2 + 1;
	ty = BTNY + (DLG_BTNH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, tx, ty, blab[i]);
	if ( i == barmed && barin )
		cl_fillrect(x + 1, BTNY + 1, x + w - 1, BTNY + DLG_BTNH - 1, 2);
	return 0;
}

/* The bar, then the path line under it, then the rule over the list. */
static
drawbar()
{
	static char *btns[] = { "Edit", "Copy", "Move", "Delete",
				"Mkdir", "Go" };
	char sbuf[32], pbuf[NPATH + 8];
	register int i;
	int x, n, avail;

	cl_fillrect(0, 0, contw, ly0 - 1, 1);
	nbtn = MAXBTN;
	x = 6;
	for ( i = 0; i < nbtn; i++ )
	{
		blab[i] = btns[i];
		bx[i] = x;
		bw[i] = strlen(btns[i]) * fcw + 2 * BTNPAD;
		drawbtn(i);
		x += bw[i] + BTNGAP;
	}
	if ( trunc )
		sprintf(sbuf, "%d+ entries", nfil);
	else
		sprintf(sbuf, "%d entr%s", nfil, nfil == 1 ? "y" : "ies");
	i = contw - 8 - strlen(sbuf) * fcw;
	if ( i > x )
		cl_ptext(SHM_FUI, i, BTNY + (DLG_BTNH - fch) / 2 + 1, sbuf);

	/* the path line: the tail of a path too long for the window */
	avail = (contw - 12) / fcw - 6;
	n = strlen(cwd);
	if ( n > avail && avail > 3 )
		sprintf(pbuf, "Path: ...%s", cwd + (n - avail + 3));
	else
		sprintf(pbuf, "Path: %s", cwd);
	cl_ptext(SHM_FUI, 6, BARH + (PATHH - fch) / 2 + 1, pbuf);
	cl_fillrect(0, ly0 - 2, contw, ly0 - 1, 0);
	return 0;
}

/* Which button is under content pixel (x,y)?  -1 = none. */
static
btnhit(x, y)
{
	register int i;

	if ( y < BTNY || y >= BTNY + DLG_BTNH )
		return -1;
	for ( i = 0; i < nbtn; i++ )
		if ( x >= bx[i] && x < bx[i] + bw[i] )
			return i;
	return -1;
}

/* ------------------------------------------------------------------ */
/* painting                                                           */
/* ------------------------------------------------------------------ */

static
drawrun(r, c0, c1, vp)
char *vp;
{
	char buf[MAXCOLS + 1];
	int s, e, i, n, y;

	y = ly0 + r * cellh;
	cl_fillrect(xpix + c0 * cellw, y,
		    xpix + c1 * cellw, y + cellh, 1);
	for ( s = c0; s < c1; s = e )
	{
		while ( s < c1 && vp[s] == ' ' )
			s++;
		if ( s >= c1 )
			break;
		for ( e = s; e < c1 && vp[e] != ' '; e++ )
			;
		n = 0;
		for ( i = s; i < e; i++ )
			buf[n++] = vp[i];
		buf[n] = 0;
		cl_ptext(SHM_FTERM, xpix + s * cellw, y, buf);
	}
	return 0;
}

static
clamptop()
{
	if ( ltop > nfil - lrows )
		ltop = nfil - lrows;
	if ( ltop < 0 )
		ltop = 0;
	return 0;
}

static
flush()
{
	register int r, c;
	register char *vp;
	int c0;

	clamptop();
	cl_begin();
	hlerase();
	if ( chromedirty )
	{
		drawbar();
		chromedirty = 0;
	}
	for ( r = 0; r < lrows; r++ )
	{
		vp = vrow(r);
		c = 0;
		while ( c < cols )
		{
			if ( vp[c] == disp[r][c] )
			{
				c++;
				continue;
			}
			c0 = c;
			while ( c < cols && vp[c] != disp[r][c] )
			{
				disp[r][c] = vp[c];
				c++;
			}
			drawrun(r, c0, c, vp);
		}
	}
	hldraw();
	sbl.sb_x = 0;
	sbl.sb_y = ly0;
	sbl.sb_h = lrows * cellh;
	sbl.sb_total = nfil;
	sbl.sb_page = lrows;
	sbl.sb_pos = ltop;
	hr_sbdraw(&sbl, sblforce);
	sblforce = 0;
	cl_end();
	return 0;
}

static
layout()
{
	ly0 = BARH + PATHH + 2;
	lrows = (conth - ly0) / cellh;
	cols = (contw - xpix) / cellw;
	if ( lrows < 1 ) lrows = 1;
	if ( lrows > MAXROWS ) lrows = MAXROWS;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( cols < 1 ) cols = 1;
	return 0;
}

static
select(i)
{
	if ( i < 0 || i >= nfil )
		return 0;
	self = i;
	strcpy(selname, files[i].nm);
	if ( self < ltop )
		ltop = self;
	if ( self >= ltop + lrows )
		ltop = self - lrows + 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* dialogs (zprint's notice/confirm + one text-field dialog)          */
/* ------------------------------------------------------------------ */

char	dmsg[44];

HRWIDGET mwg[] = {
    { DW_LABEL,   12,  16,   0,  0, dmsg },
    { DW_BUTTON, 150,  52,  70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NMWG	(sizeof(mwg) / sizeof(mwg[0]))

/* A one-line notice with an OK button. */
static
notice(msg)
char *msg;
{
	int w, h, r;

	strncpy(dmsg, msg, sizeof(dmsg) - 1);
	dmsg[sizeof(dmsg) - 1] = 0;
	w = strlen(dmsg) * 9 + 2 * DLG_MARG;
	if ( w < 240 ) w = 240;
	if ( w > 420 ) w = 420;
	h = 96;
	mwg[1].dw_x = (w - 70) / 2;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(mwg, NMWG);
	r = hr_dlgrun(mwg, NMWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return 0;
}

char	cfmsg[40];

HRWIDGET cwg[] = {
    { DW_LABEL,   12,  16,   0,  0, cfmsg },
    { DW_BUTTON,  40,  56,  90, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  56,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NCWG	(sizeof(cwg) / sizeof(cwg[0]))

static
confirm(msg)
char *msg;
{
	int w, h, r;

	strcpy(cfmsg, msg);
	w = 300;
	h = 100;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(cwg, NCWG);
	r = hr_dlgrun(cwg, NCWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return r == 1;
}

/* The one text-field dialog, relabelled per use. */
char	tlab[44];
char	tbuf[80];

HRWIDGET twg[] = {
    { DW_LABEL,   12,  16,   0,  0, tlab },
    { DW_TEXT,    12,  40, 356, 22, (char *)0, 0, 0, tbuf, sizeof(tbuf) },
    { DW_BUTTON, 100,  76,  90, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 230,  76,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NTWG	(sizeof(twg) / sizeof(twg[0]))

/* Put the label up, run it; 1 = OK with a non-empty tbuf. */
static
askname(label, initial)
char *label, *initial;
{
	int w, h, r;

	strncpy(tlab, label, sizeof(tlab) - 1);
	tlab[sizeof(tlab) - 1] = 0;
	strncpy(tbuf, initial, sizeof(tbuf) - 1);
	tbuf[sizeof(tbuf) - 1] = 0;
	w = 380;
	h = 116;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(twg, NTWG);
	r = hr_dlgrun(twg, NTWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return r == 2 && tbuf[0] != 0;
}

/* ---- the Help dialog (the C900 Help key, F11) ----------------------- *
 * One card listing the whole key set (zedit's dohelp pattern) -- the C900
 * keyboard has a Help key, so it should do something helpful. */

HRWIDGET hwg[] = {
    { DW_LABEL, 12,  12, 0, 0, "Move: arrows / ^P ^N     select an entry" },
    { DW_LABEL, 12,  32, 0, 0, "PgUp PgDn (^Z ^V, space) page the list" },
    { DW_LABEL, 12,  52, 0, 0, "Home End (^A ^E)  top / bottom of the list" },
    { DW_LABEL, 12,  72, 0, 0, "Enter  enter the selected directory" },
    { DW_LABEL, 12,  92, 0, 0, "BS     up to the parent directory" },
    { DW_LABEL, 12, 116, 0, 0, "F2 e  Edit      F3 c  Copy     F4 m  Move" },
    { DW_LABEL, 12, 136, 0, 0, "F5 d  Delete    F6 k  Mkdir    F7 g  Go" },
    { DW_LABEL, 12, 160, 0, 0, "Click a name to select it; click the" },
    { DW_LABEL, 12, 180, 0, 0, "selected directory again to enter it." },
    { DW_BUTTON, 170, 212, 70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NHWG	(sizeof(hwg) / sizeof(hwg[0]))

static
dohelp()
{
	int w, h, r;

	w = 410;
	h = 256;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(hwg, NHWG);
	r = hr_dlgrun(hwg, NHWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return 0;
}

/* ------------------------------------------------------------------ */
/* the operations                                                     */
/* ------------------------------------------------------------------ */

/* Run a program and forget it: fork twice so init reaps the worker and
 * zfile never carries a zombie.  fds 0-4 are KEPT -- 4 is the shared
 * command pipe (wire.h HR_CMDFD), which is what lets the launched editor
 * connect and get a window of its own. */
static
launch(cmd, arg)
char *cmd, *arg;
{
	register int fd;
	int pid, st;

	if ( (pid = fork()) == 0 )
	{
		if ( fork() == 0 )
		{
			for ( fd = 5; fd < 20; fd++ )
				close(fd);
			execl(cmd, cmd, arg, (char *)0);
			_exit(1);
		}
		exit(0);
	}
	if ( pid > 0 )
		while ( wait(&st) >= 0 )
			;
	return 0;
}

/* Run a program and WAIT for it -- mkdir/rmdir, whose result the rescan
 * right after must be able to see. */
static
runwait(cmd, arg)
char *cmd, *arg;
{
	register int fd;
	int pid, st, w;

	if ( (pid = fork()) == 0 )
	{
		for ( fd = 5; fd < 20; fd++ )
			close(fd);
		execl(cmd, cmd, arg, (char *)0);
		_exit(1);
	}
	if ( pid > 0 )
		while ( (w = wait(&st)) != pid && w >= 0 )
			;
	return 0;
}

/* Enter a directory: chdir, then fold the step into the absolute path. */
static
enterdir(nm)
char *nm;
{
	register char *p;

	if ( chdir(nm) < 0 )
	{
		notice("Cannot enter that directory");
		return 0;
	}
	if ( strcmp(nm, "..") == 0 )
	{
		if ( (p = strrchr(cwd, '/')) != 0 )
			*p = 0;
		if ( cwd[0] == 0 )
			strcpy(cwd, "/");
	}
	else
	{
		if ( !atroot() )
			strcat(cwd, "/");
		if ( strlen(cwd) + strlen(nm) < NPATH )
			strcat(cwd, nm);
	}
	selname[0] = 0;
	ltop = 0;
	rescan();
	return 0;
}

/* "Open" the selected entry: a directory is entered, a file does nothing
 * (opening a file is the Edit button's job, not a double-click's). */
static
doopen()
{
	if ( self < 0 )
		return 0;
	if ( (files[self].mode & S_IFMT) == S_IFDIR )
		return enterdir(files[self].nm);
	return 0;
}

/* Edit the selected file: hand it to the editor.  Only an ordinary file --
 * a directory is navigated, not edited. */
static
doedit()
{
	if ( self < 0 )
		return 0;
	if ( (files[self].mode & S_IFMT) != S_IFREG )
	{
		notice("Select a file to edit");
		return 0;
	}
	launch(EDITOR, files[self].nm);
	return 0;
}

static
docopy()
{
	char lab[44], dest[NPATH];
	char iobuf[2048];
	struct stat sb;
	register int ifd, ofd, n;
	int ok;

	if ( self < 0 || strcmp(files[self].nm, "..") == 0 )
		return 0;
	if ( (files[self].mode & S_IFMT) != S_IFREG )
	{
		notice("Can only copy an ordinary file");
		return 0;
	}
	sprintf(lab, "Copy %.14s to:", files[self].nm);
	if ( !askname(lab, "") )
		return 0;
	strcpy(dest, tbuf);
	if ( stat(dest, &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFDIR )
	{
		if ( strlen(dest) + strlen(files[self].nm) + 2 >= NPATH )
		{
			notice("Name too long");
			return 0;
		}
		strcat(dest, "/");
		strcat(dest, files[self].nm);
	}
	if ( stat(dest, &sb) == 0 )
	{
		if ( !confirm("Overwrite the existing file?") )
			return 0;
	}
	if ( (ifd = open(files[self].nm, 0)) < 0 )
	{
		notice("Cannot read that file");
		return 0;
	}
	if ( (ofd = creat(dest, files[self].mode & 0777)) < 0 )
	{
		close(ifd);
		notice("Cannot create the destination");
		return 0;
	}
	ok = 1;
	while ( (n = read(ifd, iobuf, sizeof(iobuf))) > 0 )
		if ( write(ofd, iobuf, n) != n )
		{
			ok = 0;
			break;
		}
	if ( n < 0 )
		ok = 0;
	close(ifd);
	close(ofd);
	if ( !ok )
		notice("Copy failed (disk full?)");
	rescan();
	return 0;
}

static
domove()
{
	char lab[44], dest[NPATH];
	struct stat sb;

	if ( self < 0 || strcmp(files[self].nm, "..") == 0 )
		return 0;
	if ( (files[self].mode & S_IFMT) == S_IFDIR )
	{
		notice("Cannot move a directory");
		return 0;
	}
	sprintf(lab, "Move %.14s to:", files[self].nm);
	if ( !askname(lab, "") )
		return 0;
	strcpy(dest, tbuf);
	if ( stat(dest, &sb) == 0 && (sb.st_mode & S_IFMT) == S_IFDIR )
	{
		if ( strlen(dest) + strlen(files[self].nm) + 2 >= NPATH )
		{
			notice("Name too long");
			return 0;
		}
		strcat(dest, "/");
		strcat(dest, files[self].nm);
	}
	if ( stat(dest, &sb) == 0 )
	{
		if ( !confirm("Overwrite the existing file?") )
			return 0;
		unlink(dest);
	}
	if ( link(files[self].nm, dest) < 0 )
	{
		notice("Cannot move there (other file system?)");
		return 0;
	}
	unlink(files[self].nm);
	rescan();
	return 0;
}

/* An empty directory has just "." and "..". */
static
dirempty(nm)
char *nm;
{
	struct direct dir;
	register FILE *dirfile;
	register int n;

	if ( (dirfile = fopen(nm, "r")) == 0 )
		return 0;
	n = 0;
	while ( fread((char *)&dir, sizeof(dir), 1, dirfile) == 1 )
		if ( dir.d_ino != 0 )
			n++;
	fclose(dirfile);
	return n <= 2;
}

static
dodelete()
{
	char msg[44];
	register struct file *f;

	if ( self < 0 || strcmp(files[self].nm, "..") == 0 )
		return 0;
	f = &files[self];
	if ( (f->mode & S_IFMT) == S_IFDIR )
	{
		if ( !dirempty(f->nm) )
		{
			notice("That directory is not empty");
			return 0;
		}
		sprintf(msg, "Remove directory %.14s?", f->nm);
		if ( !confirm(msg) )
			return 0;
		runwait(RMDIRCMD, f->nm);
	}
	else
	{
		sprintf(msg, "Delete %.14s?", f->nm);
		if ( !confirm(msg) )
			return 0;
		if ( unlink(f->nm) < 0 )
			notice("Cannot delete it");
	}
	rescan();
	return 0;
}

static
domkdir()
{
	if ( !askname("New directory:", "") )
		return 0;
	runwait(MKDIRCMD, tbuf);
	rescan();
	return 0;
}

static
dogo()
{
	char dest[NPATH];
	register int n;

	if ( !askname("Go to directory:", cwd) )
		return 0;
	if ( tbuf[0] != '/' )
	{
		notice("Give an absolute path (starting /)");
		return 0;
	}
	strcpy(dest, tbuf);
	n = strlen(dest);
	while ( n > 1 && dest[n - 1] == '/' )
		dest[--n] = 0;
	if ( chdir(dest) < 0 )
	{
		notice("Cannot enter that directory");
		return 0;
	}
	strcpy(cwd, dest);
	selname[0] = 0;
	ltop = 0;
	rescan();
	return 0;
}

static
dobutton(i)
{
	switch ( i )
	{
	case 0:	doedit();	break;
	case 1:	docopy();	break;
	case 2:	domove();	break;
	case 3:	dodelete();	break;
	case 4:	domkdir();	break;
	case 5:	dogo();		break;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static
pgmove(d)
{
	ltop += (lrows > 1 ? lrows - 1 : 1) * d;
	clamptop();
	return 0;
}

static
dokey(c)
{
	c &= 0xff;
	switch ( c )
	{
	case 'P'-0x40:	select(self - 1);	break;
	case 'N'-0x40:	select(self + 1);	break;
	case 'Z'-0x40:				/* PgUp arrives as ^Z (zvpump) */
	case 'B'-0x40:	pgmove(-1);		break;
	case 'V'-0x40:				/* PgDn arrives as ^V */
	case 'F'-0x40:
	case ' ':	pgmove(1);		break;
	case 'A'-0x40:				/* keypad Home arrives as ^A */
	case HRK_CLRHOME:
			select(0);		break;
	case 'E'-0x40:				/* keypad End arrives as ^E */
			select(nfil - 1);	break;
	case '\r':
	case '\n':	doopen();		break;
	case 0x08:
		if ( !atroot() )
			enterdir("..");
		break;
	case HRK_HELP:	dohelp();		break;
	case 'e':
	case HRK_F2:	doedit();		break;
	case 'c':
	case HRK_F3:	docopy();		break;
	case 'm':
	case HRK_F4:	domove();		break;
	case 'd':
	case HRK_F5:	dodelete();		break;
	case 'k':
	case HRK_F6:	domkdir();		break;
	case 'g':
	case HRK_F7:	dogo();			break;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

tick()
{
	pollflag = 1;
	signal(SIGALRM, tick);
	alarm(5);
}

main(argc, argv)
char **argv;
{
	WMSG e;
	struct stat sb;
	int need, i;
	long t;

	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	fcw = hr_font(SHM_FUI)->cellw;
	fch = hr_font(SHM_FUI)->cellh;
	if ( fcw <= 0 ) fcw = 9;
	if ( fch <= 0 ) fch = 16;
	xpix = ((HRSB_W + cellw - 1) / cellw) * cellw;
	me.ha_w = xpix + 72 * cellw;	/* wide enough for the six-button bar */
	me.ha_h = BARH + PATHH + 2 + 18 * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	layout();

	/* start in / (or in a directory named on the command line) */
	strcpy(cwd, "/");
	if ( argc > 1 && argv[1][0] == '/' && chdir(argv[1]) == 0 )
	{
		strncpy(cwd, argv[1], NPATH - 1);
		i = strlen(cwd);
		while ( i > 1 && cwd[i - 1] == '/' )
			cwd[--i] = 0;
	}
	else
		chdir("/");
	rescan();

	invalidate();
	need = 1;			/* flushed below, or by the first loop
					 * pass if a server overlay is up now */
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		flush();
		need = 0;
	}

	signal(SIGALRM, tick);
	alarm(5);
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
				invalidate();
				need = 1;
				break;

			case E_RESIZE:
				contw = e.wm_arg[0];
				conth = e.wm_arg[1];
				layout();
				clamptop();
				invalidate();
				need = 1;
				break;

			case E_KEY:
				dokey(e.wm_arg[0]);
				need = 1;
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					if ( (i = btnhit(e.wm_arg[0],
							 e.wm_arg[1])) >= 0 )
					{
						barmed = i;
						barin = 1;
						cl_begin();
						cl_fillrect(bx[i] + 1, BTNY + 1,
						    bx[i] + bw[i] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= ly0 )
					{
						if ( hr_sbpress(&sbl, e.wm_arg[1]) )
						{
							ltop = sbl.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[1] >= ly0 )
					{
						i = ltop + (e.wm_arg[1] - ly0)
							   / cellh;
						if ( i >= 0 && i < nfil )
						{
							time(&t);
							if ( i == self &&
							     i == clickrow &&
							     t - clickt <= 1 )
							{
								clickrow = -1;
								doopen();
							}
							else
							{
								select(i);
								clickrow = i;
								clickt = t;
							}
							need = 1;
						}
					}
				}
				else				/* release */
				{
					if ( barmed >= 0 )
					{
						i = barmed;
						barmed = -1;
						if ( barin )
						{
							cl_begin();
							cl_fillrect(bx[i] + 1,
							    BTNY + 1,
							    bx[i] + bw[i] - 1,
							    BTNY + DLG_BTNH - 1, 2);
							cl_end();
							dobutton(i);
							need = 1;
						}
					}
					else if ( sbl.sb_drag )
						hr_sbrelease(&sbl);
				}
				break;

			case E_MOTION:
				if ( barmed >= 0 )
				{
					i = (btnhit(e.wm_arg[0], e.wm_arg[1])
					     == barmed);
					if ( i != barin )
					{
						barin = i;
						cl_begin();
						cl_fillrect(bx[barmed] + 1,
						    BTNY + 1,
						    bx[barmed] + bw[barmed] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
				}
				else if ( sbl.sb_drag )
				{
					if ( hr_sbmotion(&sbl, e.wm_arg[1]) )
					{
						ltop = sbl.sb_pos;
						need = 1;
					}
				}
				break;

			case E_MENU:
				if ( e.wm_arg[0] == HRM_HELP )
				{
					dohelp();
					need = 1;
				}
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )		/* fell behind: assume the worst */
		{
			invalidate();
			need = 1;
		}
		if ( pollflag )			/* did the directory change? */
		{
			pollflag = 0;
			if ( stat(".", &sb) == 0 && sb.st_mtime != cwdmtime )
			{
				rescan();
				invalidate();
				need = 1;
			}
		}
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )	/* a draw was lost against a freeze */
			{
				invalidate();
				need = 1;
			}
			if ( need )
			{
				flush();
				need = 0;
			}
		}
	}
}
