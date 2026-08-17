/*
 * zclock.c - a ZView clock client (GUI.md Phase 1).
 *
 * Face/hand geometry is lifted from the salvaged _graphics/hr clock.c (GUI.md
 * sec 5.2 "reuse the demo content"); everything else is new.  It is a plain
 * process -- no jlib, no coroutines -- that:
 *   - declares the window it wants (title, size, icon, resizable) to the server
 *     with hr_open() and is told its window id and granted content size back;
 *   - draws by writing window-relative command records to the server;
 *   - auto-updates once a second via SIGALRM (differential hand redraw);
 *   - repaints the whole face on an E_EXPOSE event (redraw-on-expose), which is
 *     what makes it correct when uncovered after being partially covered.
 */
#include <math.h>
#include <types.h>
#include <timeb.h>
#include <time.h>
#include <signal.h>
#include "smgr.h"		/* for the L_* logical ops + FP_* patterns */
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"		/* the Settings dialog (root: set date/time) */

#ifndef PI
#define PI	3.14159265358979323846
#endif

#define BIG	12		/* minute hand length (in 1/16 of radius) */
#define LITTLE	9		/* hour hand				  */
#define SECOND	13		/* second hand				  */

/* What we ask the server for: a square face, our own icon, and permission to be
 * stretched (the face is drawn to whatever content size we end up with). */
HRAPP	me = { "Clock", "clock.icn", 240, 240, HRF_STRETCH };

int	mywid;
int	cxmax, cymax;		/* content size			*/
int	cxrad, cyrad;		/* clock radii			*/
int	cxcen, cycen;		/* clock centre			*/
int	tickflag;

struct HAND { POINT left, center, right; };
struct HAND lastbig, lastlittle, lastsecond;
short	handfl;

POINT	cframe[2][32];
POINT	cdig[12][4];

/* Direct-render (GUI.md Model A): draw straight to VRAM via clgfx.  drawmode is
 * the clgfx line mode: 0 = black (face), 2 = invert/XOR (hands, so a redraw
 * erases -- the differential-update trick the original did with L_NDST). */
int	drawmode;
#define clrface()	cl_fillrect(0, 0, cxmax, cymax, 1)	/* white content */
#define cwline(x0, y0, x1, y1)	cl_line((x0), (y0), (x1), (y1), drawmode)

/* ---- geometry (from clock.c) ---- */
setpoints()
{
	register uint i, j;
	double s, cc, s2, c2;

	for ( i = 0; i < 32; ++i )
	{
		s = sin((2*PI*i)/31);
		cc = cos((2*PI*i)/31);
		for ( j = 0; j <= 1; j++ )
		{
			cframe[j][i].x = (uint)((cxrad + 5*j) * cc + cxcen);
			cframe[j][i].y = (uint)((cyrad + 5*j) * s + cycen);
		}
	}
	j = 0;
	for ( i = 0; i < 360; i += (360/12) )
	{
		s = sin((2*PI*i)/360);
		cc = cos((2*PI*i)/360);
		s2 = sin((2*PI*(i+1))/360);
		c2 = cos((2*PI*(i+1))/360);
		cdig[j][0].x = (uint)((15*cxrad)/16 * cc + cxcen);
		cdig[j][0].y = (uint)((15*cyrad)/16 * s + cycen);
		cdig[j][1].x = (uint)((14*cxrad)/16 * c2 + cxcen);
		cdig[j][1].y = (uint)((14*cyrad)/16 * s2 + cycen);
		cdig[j][2].x = (uint)((12*cxrad)/16 * cc + cxcen);
		cdig[j][2].y = (uint)((12*cyrad)/16 * s + cycen);
		cdig[j][3].x = (uint)((14*cxrad)/16 * cos((2*PI*((i+359)%360))/360) + cxcen);
		cdig[j][3].y = (uint)((14*cyrad)/16 * sin((2*PI*((i+359)%360))/360) + cycen);
		j++;
	}
}

different(a, b)
struct HAND *a, *b;
{
	return ( a->right.x != b->right.x || a->right.y != b->right.y ||
		 a->center.x != b->center.x || a->center.y != b->center.y ||
		 a->left.x != b->left.x || a->left.y != b->left.y );
}

clrhand(hp)
struct HAND *hp;
{
	hp->left.x = hp->left.y = 0;
	hp->center.x = hp->center.y = 0;
	hp->right.x = hp->right.y = 0;
}

ghand(hs, atm, r)
struct HAND *hs;
struct tm *atm;
int r;
{
	uint i;

	if ( r == BIG )
		i = (atm->tm_min * 360) / 60;
	else if ( r == LITTLE )
		i = ((atm->tm_hour % 12) * 360) / 12 + (atm->tm_min * 30) / 60;
	else
		i = (atm->tm_sec * 360) / 60;
	i = (i + 270) % 360;
	hs->center.x = (uint)((cxrad * r) / 16 * cos((2*PI*i)/360) + cxcen);
	hs->center.y = (uint)((cyrad * r) / 16 * sin((2*PI*i)/360) + cycen);
	if ( r == BIG )
	{
		hs->left.x  = (uint)((cxrad*(r-2))/16 * cos((2*PI*((i+358)%360))/360) + cxcen);
		hs->left.y  = (uint)((cyrad*(r-2))/16 * sin((2*PI*((i+358)%360))/360) + cycen);
		hs->right.x = (uint)((cxrad*(r-2))/16 * cos((2*PI*((i+2)%360))/360) + cxcen);
		hs->right.y = (uint)((cyrad*(r-2))/16 * sin((2*PI*((i+2)%360))/360) + cycen);
	}
	else if ( r == LITTLE )
	{
		hs->left.x  = (uint)((cxrad*(r-2))/16 * cos((2*PI*((i+357)%360))/360) + cxcen);
		hs->left.y  = (uint)((cyrad*(r-2))/16 * sin((2*PI*((i+357)%360))/360) + cycen);
		hs->right.x = (uint)((cxrad*(r-2))/16 * cos((2*PI*((i+3)%360))/360) + cxcen);
		hs->right.y = (uint)((cyrad*(r-2))/16 * sin((2*PI*((i+3)%360))/360) + cycen);
	}
}

hand(h, hf)
struct HAND *h;
short hf;
{
	if ( hf == SECOND )
		cwline(cxcen, cycen, h->center.x, h->center.y);
	else
	{
		cwline(cxcen, cycen, h->left.x, h->left.y);
		cwline(h->left.x, h->left.y, h->center.x, h->center.y);
		cwline(h->center.x, h->center.y, h->right.x, h->right.y);
		cwline(h->right.x, h->right.y, cxcen, cycen);
	}
}

drawface()
{
	register uint i, j;

	clrface();
	drawmode = 0;			/* black face lines */
	for ( i = 0; i < 31; ++i )
		for ( j = 0; j <= 1; j++ )
			cwline(cframe[j][i].x, cframe[j][i].y,
			       cframe[j][i+1].x, cframe[j][i+1].y);
	for ( i = 0; i < 12; i++ )
	{
		cwline(cdig[i][0].x, cdig[i][0].y, cdig[i][1].x, cdig[i][1].y);
		cwline(cdig[i][1].x, cdig[i][1].y, cdig[i][2].x, cdig[i][2].y);
		cwline(cdig[i][2].x, cdig[i][2].y, cdig[i][3].x, cdig[i][3].y);
		cwline(cdig[i][3].x, cdig[i][3].y, cdig[i][0].x, cdig[i][0].y);
	}
	clrhand(&lastbig);
	clrhand(&lastlittle);
	handfl = 0;
	drawmode = 2;			/* hands invert/XOR (draw == erase) */
}

drawhands()
{
	struct HAND big, little, second;
	struct tm *atm;
	struct timeb tb;

	ftime(&tb);
	atm = localtime(&tb.time);
	ghand(&big, atm, BIG);
	ghand(&little, atm, LITTLE);
	ghand(&second, atm, SECOND);
	if ( handfl )
	{
		if ( different(&lastbig, &big) )     hand(&lastbig, BIG);
		if ( different(&lastlittle, &little) ) hand(&lastlittle, LITTLE);
		hand(&lastsecond, SECOND);
	}
	if ( different(&lastbig, &big) )     hand(&big, BIG);
	if ( different(&lastlittle, &little) ) hand(&little, LITTLE);
	hand(&second, SECOND);
	lastbig = big;
	lastlittle = little;
	lastsecond = second;
	handfl = 1;
}

/* ---- the Settings dialog: set the system date and time (root only) ------- *
 * Declared (ha_menu |= HRM_SETTINGS) only when getuid() == 0, because only
 * root's stime() succeeds -- anyone else simply has no Settings entry.
 * Two text fields, prefilled from localtime; OK parses, validates and sets.
 *
 * Rejected input keeps the dialog up with what was typed still in the fields
 * (so a single wrong digit is one keystroke to fix, not a retype) and SAYS SO
 * on the message line -- silently doing nothing would just read as a dead OK
 * button.  A DW_LABEL over a buffer is the whole mechanism: set the text and
 * redraw that one widget.
 *
 * Field rows and the button row are laid out on the same DLG_MARG margin the
 * chrome uses, so the fields clear the top edge and sit a normal gap above
 * the buttons rather than drifting apart. */

char	datebuf[12];		/* YYYY-MM-DD */
char	timebuf[10];		/* HH:MM:SS   */
char	msgbuf[32];		/* "" or why the last OK was refused */

/* A DW_TEXT draws its content at dw_y + (h - cellh)/2 + 1, so a DW_LABEL
 * beside a 22px field is aligned by sitting 4px lower than the field. */
HRWIDGET swg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Date:" },
    { DW_TEXT,    70,  12, 120, 22, (char *)0, 0, 0, datebuf, sizeof(datebuf) },
    { DW_LABEL,   12,  48,   0,  0, "Time:" },
    { DW_TEXT,    70,  44, 120, 22, (char *)0, 0, 0, timebuf, sizeof(timebuf) },
    { DW_LABEL,   12,  72,   0,  0, msgbuf },
    { DW_BUTTON,  60, 104,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170, 104,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define NSWG	(sizeof(swg) / sizeof(swg[0]))
#define SW_MSG	4		/* the message line   */
#define SW_OK	5		/* the OK button      */
#define SW_W	300		/* interior size that fits the layout above */
#define SW_H	146		/* (buttons end at 104+24+3 ring+2 shadow)  */
/* The message line sits just under the fields (6px) and well clear of the
 * buttons (13px to the default button's ring): it says what is wrong with
 * what was TYPED, so it belongs with the fields, not with the buttons. */

static int	mdays[12] = { 31,28,31,30,31,30,31,31,30,31,30,31 };

static
leapy(y)
{
	return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}

/* Days from 1970-01-01 to civil date y-m-d (the libc has no mktime). */
static long
civdays(y, m, d)
{
	long dd;
	int i;

	dd = 0;
	for ( i = 1970; i < y; i++ )
		dd += leapy(i) ? 366 : 365;
	for ( i = 1; i < m; i++ )
	{
		dd += mdays[i - 1];
		if ( i == 2 && leapy(y) )
			dd++;
	}
	return dd + d - 1;
}

/* Parse the two fields, validate, convert LOCAL time to a UTC time_t and set
 * the system clock.  Returns 0 on success, or -1 having put the reason in
 * msgbuf for the caller to show.
 *
 * The local->UTC conversion starts from "as if the fields were UTC" and then
 * REFINES through localtime() itself -- a few iterations of "how far is
 * localtime(t) from the target" -- so the TZ/DST arithmetic stays inside
 * ctime.c where it already lives, instead of being duplicated here. */
static
parseset()
{
	int y, mo, d, hh, mi, ss, i, ml;
	long t, ds;
	struct tm *tp;

	if ( sscanf(datebuf, "%d-%d-%d", &y, &mo, &d) != 3 ||
	     sscanf(timebuf, "%d:%d:%d", &hh, &mi, &ss) != 3 )
	{
		strcpy(msgbuf, "Use YYYY-MM-DD and HH:MM:SS");
		return -1;
	}
	if ( y < 1970 || y > 2037 )	/* 2038: signed 32-bit time_t */
	{
		strcpy(msgbuf, "Year must be 1970-2037");
		return -1;
	}
	ml = 0;
	if ( mo >= 1 && mo <= 12 )
	{
		ml = mdays[mo - 1];
		if ( mo == 2 && leapy(y) )
			ml = 29;
	}
	if ( mo < 1 || mo > 12 || d < 1 || d > ml )
	{
		strcpy(msgbuf, "No such date");
		return -1;
	}
	if ( hh < 0 || hh > 23 || mi < 0 || mi > 59 || ss < 0 || ss > 59 )
	{
		strcpy(msgbuf, "No such time");
		return -1;
	}
	t = civdays(y, mo, d) * 86400L +
	    hh * 3600L + mi * 60L + (long)ss;
	for ( i = 0; i < 3; i++ )
	{
		tp = localtime(&t);
		ds = (civdays(y, mo, d) -
		      civdays(tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday))
			 * 86400L +
		     (hh - tp->tm_hour) * 3600L +
		     (mi - tp->tm_min) * 60L + (long)(ss - tp->tm_sec);
		if ( ds == 0 )
			break;
		t += ds;
	}
	if ( stime(&t) < 0 )
	{
		strcpy(msgbuf, "Cannot set the clock");
		return -1;			/* not root after all */
	}
	return 0;
}

dosettings()
{
	int w, h, r;
	long t;
	struct tm *tp;

	time(&t);
	tp = localtime(&t);
	sprintf(datebuf, "%04d-%02d-%02d",
		tp->tm_year + 1900, tp->tm_mon + 1, tp->tm_mday);
	sprintf(timebuf, "%02d:%02d:%02d",
		tp->tm_hour, tp->tm_min, tp->tm_sec);
	msgbuf[0] = 0;			/* nothing to complain about yet */
	w = SW_W;
	h = SW_H;
	r = hr_dlgopen(&w, &h);
	if ( r < 0 )
	{
		if ( r == -2 )			/* E_QUIT while waiting */
			exit(0);
		return;
	}
	hr_dlgdraw(swg, NSWG);
	for (;;)
	{
		r = hr_dlgrun(swg, NSWG);
		if ( r == -1 )			/* window died mid-dialog */
		{
			hr_dlgclose();
			exit(0);
		}
		if ( r != SW_OK )		/* Cancel: change nothing */
			break;
		if ( parseset() == 0 )		/* OK: set the clock */
			break;
		/* Refused: parseset said why.  Keep the dialog up with the
		 * fields as typed and show the reason -- clearing the message
		 * line first, since a label draws no background of its own and
		 * the previous message may have been the longer one. */
		cl_fillrect(swg[SW_MSG].dw_x, swg[SW_MSG].dw_y, w,
			    swg[SW_MSG].dw_y + hr_font(SHM_FUI)->cellh, 1);
		hr_dlgdraw(swg, NSWG);
	}
	hr_dlgclose();
}

repaint()
{
	if ( cl_frozen() )	/* a server menu/overlay is up: don't paint over it */
		return;
	cl_begin();		/* sync clip descriptor + bracket cursor */
	drawface();
	drawhands();
	cl_end();
	cl_snapclip();		/* record what this full repaint covered */
}

tick()
{
	tickflag = 1;
	signal(SIGALRM, tick);
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int n;
	int needfull, ticked;

	if ( getuid() == 0 )		/* only root's stime() works, so only */
		me.ha_menu = HRM_SETTINGS;	/* root gets the menu entry   */
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* no window server (hr_open does cl_init) */
	cxmax = me.ha_w;		/* the size we were GRANTED */
	cymax = me.ha_h;
	cxcen = cxmax / 2;
	cycen = cymax / 2;
	cxrad = cxmax / 2 - 8;
	cyrad = cymax / 2 - 8;
	if ( cxrad < 8 ) cxrad = 8;
	if ( cyrad < 8 ) cyrad = 8;

	setpoints();
	repaint();			/* initial draw */

	signal(SIGALRM, tick);
	alarm(1);

	needfull = 0;
	ticked = 0;
	for (;;)
	{
		/* Block on our event ring (shmem.h SHM_EVQ) -- no pipe, and no system
		 * call at all when an event is already queued.  SIGALRM breaks the
		 * wait, which is what keeps the second hand ticking. */
		hr_evwait(hr_wid());
		while ( hr_evget(hr_wid(), (short *)&e) )
		{
			if ( e.wm_type == E_EXPOSE )
				needfull = 1;
			else if ( e.wm_type == E_QUIT )
				exit(0);
			else if ( e.wm_type == E_MENU &&
				  e.wm_arg[0] == HRM_SETTINGS )
				dosettings();
			else if ( e.wm_type == E_RESIZE )
			{
				cxmax = e.wm_arg[0];
				cymax = e.wm_arg[1];
				cxcen = cxmax / 2;  cycen = cymax / 2;
				cxrad = cxmax / 2 - 8;  cyrad = cymax / 2 - 8;
				if ( cxrad < 8 ) cxrad = 8;
				if ( cyrad < 8 ) cyrad = 8;
				setpoints();
				needfull = 1;
			}
		}
		if ( hr_evover(hr_wid()) )	/* we fell behind: assume the worst */
			needfull = 1;

		if ( tickflag )
		{
			tickflag = 0;
			ticked = 1;
			alarm(1);
		}

		/* Draw or defer (while a menu/overlay is up or we are minimised,
		 * everything keeps until we are drawable again).  A FULL repaint
		 * happens only when damage arrived (expose/ring overflow), when a
		 * draw of ours was dropped against a freeze (cl_dropped), or when
		 * the clip UNCOVERED area our last full repaint did not cover
		 * (cl_uncovered) -- the incremental hands may have been clipped
		 * out there, so the face cannot be patched.  A raise elsewhere
		 * that merely covers us MORE repaints nothing: the tighter clip
		 * alone is enough (the old test repainted on ANY clip-generation
		 * change, which made the clock flash on every restack anywhere).
		 * Otherwise a tick just moves the hands. */
		cl_refresh();
		if ( cl_mapped() && !cl_frozen() )
		{
			if ( needfull || cl_dropped() || cl_uncovered() )
			{
				repaint();
				needfull = 0;
				ticked = 0;	/* repaint drew the hands too */
			}
			else if ( ticked )
			{
				cl_begin();
				drawhands();
				cl_end();
				ticked = 0;
			}
		}
	}
}
