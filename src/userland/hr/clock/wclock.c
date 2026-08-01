/*
 * wclock.c - an hrgui clock client (GUI.md Phase 1).
 *
 * Face/hand geometry is lifted from the salvaged _graphics/hr clock.c (GUI.md
 * sec 5.2 "reuse the demo content"); everything else is new.  It is a plain
 * process -- no jlib, no coroutines -- that:
 *   - learns its window id and content size from argv (the server created the
 *     window and forked us with HR_EVFD/HR_CMDFD wired up);
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

#ifndef PI
#define PI	3.14159265358979323846
#endif

#define BIG	12		/* minute hand length (in 1/16 of radius) */
#define LITTLE	9		/* hour hand				  */
#define SECOND	13		/* second hand				  */

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

int	paintgen = -2;		/* clip generation of our last full repaint */

repaint()
{
	if ( cl_frozen() )	/* a server menu/overlay is up: don't paint over it */
		return;
	cl_begin();		/* sync clip descriptor + bracket cursor */
	drawface();
	drawhands();
	cl_end();
	paintgen = cl_gen();	/* record what we painted against */
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

	if ( argc < 4 )
		exit(1);
	mywid = atoi(argv[1]);
	cxmax = atoi(argv[2]);
	cymax = atoi(argv[3]);
	cxcen = cxmax / 2;
	cycen = cymax / 2;
	cxrad = cxmax / 2 - 8;
	cyrad = cymax / 2 - 8;
	if ( cxrad < 8 ) cxrad = 8;
	if ( cyrad < 8 ) cyrad = 8;

	cl_init(mywid);			/* direct-render: map VRAM + clip descriptor */
	setpoints();
	repaint();			/* initial draw */

	signal(SIGALRM, tick);
	alarm(1);

	for (;;)
	{
		n = read(HR_EVFD, &e, sizeof(e));
		if ( n == sizeof(e) )
		{
			if ( e.wm_type == E_EXPOSE )
				repaint();
			else if ( e.wm_type == E_QUIT )
				exit(0);
			else if ( e.wm_type == E_RESIZE )
			{
				cxmax = e.wm_arg[0];
				cymax = e.wm_arg[1];
				cxcen = cxmax / 2;  cycen = cymax / 2;
				cxrad = cxmax / 2 - 8;  cyrad = cymax / 2 - 8;
				if ( cxrad < 8 ) cxrad = 8;
				if ( cyrad < 8 ) cyrad = 8;
				setpoints();
				repaint();
			}
		}
		else if ( n == 0 )
			exit(0);		/* server gone */

		if ( tickflag )
		{
			tickflag = 0;
			/* Skip while a menu/overlay is up or we are unmapped (minimised); and
			 * if the server has hidden/shown/raised/resized us since our last full
			 * repaint (clip generation changed), do a FULL repaint rather than an
			 * incremental drawhands over a stale-or-blank face. */
			cl_refresh();
			if ( cl_mapped() && !cl_frozen() )
			{
				if ( cl_gen() != paintgen )
					repaint();
				else
				{
					cl_begin();
					drawhands();
					cl_end();
				}
			}
			alarm(1);
		}
	}
}
