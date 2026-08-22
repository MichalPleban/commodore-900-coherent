/*
 * symedit.c - SymEdit, the symbol editor for Vellum libraries.
 *
 * The "draw it once, reuse it everywhere" half of the schematic editor:
 * zdraw's palette appends every symbol found in /usr/vellum/etc/symbols, and
 * this program is how those symbols are made.  It edits ONE symbol at a
 * time on a magnified quarter-grid canvas (10 px per quarter unit, so one
 * schematic grid unit is a 40 px square); the whole library is held in
 * memory and written back by Save.
 *
 * Layout: a tool column down the left edge -- Seg, Circ, Arc, Pin,
 * Char, Move, Del, with a TRUE-SCALE preview of the symbol under them
 * (2 px per quarter unit, exactly what Vellum will draw) -- and the
 * edit canvas, dotted at every quarter unit with heavier marks on whole
 * grid units and a cross at the ORIGIN, the point Vellum places and
 * rotates the symbol around.
 *
 * Tools (keys s c a p t v e; ESC = back to Seg):
 *   Seg   drag draws a segment, snapped to quarter units
 *   Circ  drag from the centre, radius snapped to quarter units
 *   Arc   drag centre to the start point (radius + start angle), then
 *         drag again to sweep out the end angle (5-degree snaps)
 *   Pin   click places a connection pin, snapped to WHOLE grid units --
 *         pins are where Vellum's wires snap; a second click on an
 *         existing pin asks for its NAME (netlists print Q1.B)
 *   Char  click asks for one character (small font) and places it
 *   Move  drag an element; a drag on empty canvas marquee-selects and
 *         the set drags/deletes ('x') as one
 *   Del   click removes the nearest element
 * Keys: r / m rotate / mirror the WHOLE buffer around the origin;
 * f pulls a symbol from ANOTHER library in as the starting point.
 *
 * Menu: New (code + designator prefix), Open (by code; the message line
 * lists what the library holds), Save (writes the whole library), Help.
 * Switching symbols keeps edits in memory; only Save touches the file.
 *
 * The file format is the plain-text one Vellum reads (see velfile.c):
 *	symbol CODE PFX
 *	s x0 y0 x1 y1  /  c cx cy r  /  a cx cy r a0 a1  /  t x y C
 *	p x y [NAME]
 *	end
 * Coordinates are quarter-grid units; keep bodies within about -20..40 x
 * and -20..20 y so they fit Vellum's palette cells and canvas sensibly.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"

#define	SYMFILE	"/usr/vellum/etc/symbols"	/* default: the user scratch lib */

char	libfile[44] = SYMFILE;	/* the library FILE being edited: argv[1]
				 * overrides (zdraw's Edit button passes the
				 * current library's path) */
char	*libbase = libfile;	/* its basename, for the status line     */

/* ---- geometry ---- */
#define	SC	10		/* px per quarter unit on the edit canvas */
#define	TW	48		/* tool column width (3 VRAM words)       */
#define	STH	18		/* status bar                             */
#define	QX0	(-8)		/* visible q range                        */
#define	QX1	48
#define	QY0	(-16)
#define	QY1	24
#define	ORGX	(TW + (0 - QX0) * SC)	/* canvas px of q (0,0)           */
#define	ORGY	((0 - QY0) * SC)
#define	TCH	20		/* tool cell height                       */
#define	PREVY	146		/* preview box top (below 7 cells + rule) */
#define	PREVH	64

/* ---- the op list (vellum's symbol encoding) ---- */
#define	SEND	0
#define	SE	1
#define	SC_	2		/* circle (SC is taken by the scale)      */
#define	ST	3
#define	SA_	4		/* arc: cx cy r a0 a1 (degrees, y up)     */

#define	MAXEOP	400		/* edit buffer, shorts                    */
#define	MAXPIN	8
#define	MAXCUST	12
#define	SLOTOPS	320

/* ---- tools ---- */
#define	T_SEG	0
#define	T_CIRC	1
#define	T_ARC	2
#define	T_PIN	3
#define	T_CHR	4
#define	T_MOVE	5
#define	T_DEL	6
#define	NTOOL	7

char	*toolnm[] = { "Seg", "Circ", "Arc", "Pin", "Char", "Move", "Del" };

HRAPP	me = { "SymEdit", "vellum.icn", 0, 0, HRF_CONFIRM, 0, 0,
	       HRM_NEW | HRM_OPEN | HRM_SAVE | HRM_HELP };

int	mywid;
int	contw, conth;
int	tool	= T_SEG;
int	lastqx, lastqy;
int	statdirty;
int	edited;			/* library differs from the file          */

/* ---- the edit buffer (current symbol) ---- */
short	eops[MAXEOP];		/* op tuples, NO trailing SEND            */
int	neop;			/* shorts used                            */
char	ecode[8], epfx[4];

short	epin[2 * MAXPIN];
char	epname[MAXPIN][8];	/* pin NAMES (netlists say Q1.B)          */
int	npin;

/* ---- the in-memory library ---- */
typedef struct {
	char	cs_code[8];
	char	cs_pfx[4];
	short	cs_ops[SLOTOPS];	/* SEND-terminated                */
	short	cs_nop;			/* shorts before the SEND         */
	short	cs_pin[2 * MAXPIN];
	char	cs_pnm[MAXPIN][8];	/* pin names ("" = none)          */
	short	cs_npin;
} CSYM;

CSYM	lib[MAXCUST];
int	nlib;
int	cursl	= -1;		/* library slot being edited              */

/* ---- drag / rubber state ---- */
#define	DG_SEG	1
#define	DG_CIRC	2
#define	DG_ARC	3		/* stage 1: centre -> radius/start        */
#define	DG_MOVE	4		/* dragging elements                      */
#define	DG_MARQ	5		/* marquee select                         */
#define	DG_ARC2	6		/* stage 2: sweeping the arc's end        */
int	drag;			/* 0 none, else DG_*                      */
int	dqx, dqy;		/* start, q units                         */
int	cqx, cqy;		/* current, q units                       */
int	rubon;

/* the pending arc's second stage (sweep, click ends) */
int	arcpend;
int	arccx, arccy, arcr, arca0;

/* the Move tool: marquee SELECTION (element tuples by start offset,
 * pins by index) and the grab that drags it */
char	eselm[MAXEOP];
char	pselm[MAXPIN];
int	nselm;			/* selected elements + pins               */

/* ---- integer trig for arcs (no floats anywhere) ---- */
static short sintab[10] = { 0, 44, 88, 128, 165, 196, 222, 241, 252, 256 };

static
isin(a)
{
	register int s, i, f;

	a %= 360;
	if ( a < 0 )
		a += 360;
	s = 1;
	if ( a >= 180 )
	{
		a -= 180;
		s = -1;
	}
	if ( a > 90 )
		a = 180 - a;
	i = a / 10;
	f = a % 10;
	return s * (sintab[i] + (sintab[i + 1 > 9 ? 9 : i + 1] - sintab[i]) *
		    f / 10);
}

static
icos(a)
{
	return isin(a + 90);
}

/* angle of (dx,dy), degrees 0..359, y UP */
static
iangle(dx, dy)
{
	int ax, ay, sw, a;
	long t;

	ax = dx < 0 ? -dx : dx;
	ay = dy < 0 ? -dy : dy;
	if ( ax == 0 && ay == 0 )
		return 0;
	sw = ay > ax;
	t = sw ? (long)ax * 45 * 2 / ay : (long)ay * 45 * 2 / ax;
	a = (int)(t / 2 + t * (90 - t) * 11 / 8100);
	if ( sw )
		a = 90 - a;
	if ( dx < 0 )
		a = 180 - a;
	if ( dy < 0 )
		a = 360 - a;
	return a % 360;
}

/* chorded arc through cl_line (px space) */
static
arcline(cx, cy, r, a0, a1, mode)
{
	register int a, step;
	int x0, y0, x1, y1;

	if ( r <= 0 )
		return 0;
	while ( a1 <= a0 )
		a1 += 360;
	step = 200 / r + 3;
	if ( step > 30 )
		step = 30;
	x0 = cx + (r * icos(a0) + 128) / 256;
	y0 = cy - (r * isin(a0) + 128) / 256;
	for ( a = a0 + step; a < a1 + step; a += step )
	{
		if ( a > a1 )
			a = a1;
		x1 = cx + (r * icos(a) + 128) / 256;
		y1 = cy - (r * isin(a) + 128) / 256;
		cl_line(x0, y0, x1, y1, mode);
		x0 = x1;
		y0 = y1;
	}
	return 0;
}

/* ---- damage bookkeeping (zdraw's discipline, scaled down): actions
 * declare what they touched; flush() repaints exactly that -- one canvas
 * rect, the tool cells whose state changed, the preview only when the
 * drawing changed.  Nothing repaints wholesale on the 6 MHz machine. ---- */
int	ddcanv;			/* whole edit canvas                      */
int	ddrect;			/* 1 = drx.. hold a damage rect           */
int	drx0, dry0, drx1, dry1;
int	ddprev;			/* the true-scale preview box             */
int	ddtools;		/* the whole tool column                  */
int	showntool = -1;		/* tool cell highlighted on screen        */
int	shownlib = -1;		/* (cursl) shown -- forces stat refresh   */

static
alldirty()
{
	ddcanv = 1;
	ddprev = 1;
	ddtools = 1;
	showntool = -1;
	statdirty = 1;
	return 0;
}

static
dmg(x0, y0, x1, y1)
{
	if ( ddrect )
	{
		if ( x0 < drx0 ) drx0 = x0;
		if ( y0 < dry0 ) dry0 = y0;
		if ( x1 > drx1 ) drx1 = x1;
		if ( y1 > dry1 ) dry1 = y1;
	}
	else
	{
		drx0 = x0;  dry0 = y0;  drx1 = x1;  dry1 = y1;
		ddrect = 1;
	}
	return 0;
}

/* Canvas-pixel bbox of the op tuple at p (+2 px margin). */
static
opbox(p, x0, y0, x1, y1)
short *p;
int *x0, *y0, *x1, *y1;
{
	int a, b;

	if ( *p == SE )
	{
		a = qtopx(p[1] < p[3] ? p[1] : p[3]);
		b = qtopx(p[1] > p[3] ? p[1] : p[3]);
		*x0 = a - 2;  *x1 = b + 2;
		a = qtopy(p[2] < p[4] ? p[2] : p[4]);
		b = qtopy(p[2] > p[4] ? p[2] : p[4]);
		*y0 = a - 2;  *y1 = b + 2;
	}
	else if ( *p == SC_ || *p == SA_ )
	{
		*x0 = qtopx(p[1]) - p[3] * SC - 2;
		*x1 = qtopx(p[1]) + p[3] * SC + 2;
		*y0 = qtopy(p[2]) - p[3] * SC - 2;
		*y1 = qtopy(p[2]) + p[3] * SC + 2;
	}
	else				/* ST: one small-font glyph */
	{
		*x0 = qtopx(p[1]) - 2;
		*x1 = qtopx(p[1]) + 8;
		*y0 = qtopy(p[2]) - 2;
		*y1 = qtopy(p[2]) + 10;
	}
	return 0;
}

/* Size in shorts of the op tuple at p. */
static
opsz(p)
short *p;
{
	return (*p == SE) ? 5 : (*p == SA_) ? 6 : 4;
}

static
dmgop(p)
short *p;
{
	int x0, y0, x1, y1;

	opbox(p, &x0, &y0, &x1, &y1);
	dmg(x0, y0, x1, y1);
	return 0;
}

static
dmgpin(qx, qy)
{
	/* the +48 right / -12 top margin covers a pin NAME's glyphs */
	dmg(qtopx(qx) - 6, qtopy(qy) - 12, qtopx(qx) + 48, qtopy(qy) + 6);
	return 0;
}

/* ------------------------------------------------------------------ */
/* coordinates                                                        */
/* ------------------------------------------------------------------ */

static
qtopx(q)
{
	return ORGX + q * SC;
}

static
qtopy(q)
{
	return ORGY + q * SC;
}

/* Round canvas px to the nearest quarter unit (negatives round toward
 * the nearer point too, not toward zero). */
static
pxtoq(v, org)
{
	v -= org;
	if ( v >= 0 )
		v = (v + SC / 2) / SC;
	else
		v = -((-v + SC / 2) / SC);
	return v;
}

static
clampq(q, lo, hi)
{
	if ( q < lo ) q = lo;
	if ( q > hi ) q = hi;
	return q;
}

/* ------------------------------------------------------------------ */
/* drawing                                                            */
/* ------------------------------------------------------------------ */

/* Circles go through the ENGINE primitive (clgfx cl_circle) -- a local
 * bare-cl_point loop is stomped by the cursor save-under (see zdraw). */
static
circle(cx, cy, r, mode)
{
	cl_circle(cx, cy, r, mode);
	return 0;
}

static long
isqrt(v)
long v;
{
	register long r, b;

	r = 0;
	b = 0x40000000L;
	while ( b > v )
		b >>= 2;
	while ( b )
	{
		if ( v >= r + b )
		{
			v -= r + b;
			r = (r >> 1) + b;
		}
		else
			r >>= 1;
		b >>= 2;
	}
	return r;
}

/* Walk an op list and draw it at ppq px per quarter unit around (ox,oy).
 * n = shorts to walk (the edit buffer has no SEND). */
static
drawops(p, n, ox, oy, ppq)
short *p;
{
	register short *e;
	char tb[2];

	e = p + n;
	while ( p < e && *p != SEND )
	{
		if ( *p == SE )
		{
			cl_line(ox + p[1] * ppq, oy + p[2] * ppq,
				ox + p[3] * ppq, oy + p[4] * ppq, 0);
			p += 5;
		}
		else if ( *p == SC_ )
		{
			circle(ox + p[1] * ppq, oy + p[2] * ppq,
			       p[3] * ppq, 0);
			p += 4;
		}
		else if ( *p == SA_ )
		{
			arcline(ox + p[1] * ppq, oy + p[2] * ppq,
				p[3] * ppq, p[4], p[5], 0);
			p += 6;
		}
		else if ( *p == ST )
		{
			tb[0] = p[3];
			tb[1] = 0;
			cl_ptextt(SHM_FICON, ox + p[1] * ppq,
				  oy + p[2] * ppq, tb);
			p += 4;
		}
		else
			break;		/* corrupt: stop walking */
	}
	return 0;
}

static
drawcanvas()
{
	register int qx, qy;
	register int i;
	int px, py;

	int nq, nu;

	cl_fillrect(TW, 0, contw, conth - STH, 1);
	nq = QX1 - QX0 + 1;
	for ( qy = QY0; qy <= QY1; qy++ )	/* a dot per quarter unit */
		cl_dotrow(qtopx(QX0), qtopy(qy), nq, SC);
	nu = (QX1 - QX0) / 4 + 1;		/* 2x2 marks on whole units */
	for ( qy = QY0; qy <= QY1; qy += 4 )
	{
		py = qtopy(qy);
		cl_dotrow(qtopx(QX0) - 1, py - 1, nu, 4 * SC);
		cl_dotrow(qtopx(QX0),     py - 1, nu, 4 * SC);
		cl_dotrow(qtopx(QX0) - 1, py,     nu, 4 * SC);
		cl_dotrow(qtopx(QX0),     py,     nu, 4 * SC);
	}
	px = qtopx(0);				/* the origin cross */
	py = qtopy(0);
	cl_line(px - 8, py, px + 8, py, 0);
	cl_line(px, py - 8, px, py + 8, 0);
	drawops(eops, neop, ORGX, ORGY, SC);
	for ( i = 0; i < npin; i++ )		/* pins: ringed dots */
	{
		px = qtopx(epin[2*i]);
		py = qtopy(epin[2*i + 1]);
		circle(px, py, 4, 0);
		cl_fillrect(px - 1, py - 1, px + 1, py + 1, 0);
		if ( epname[i][0] )
			cl_ptextt(SHM_FICON, px + 6, py - 10, epname[i]);
	}
	drawmsel();
	return 0;
}

/* The gray marker round every selected element (Move tool). */
static
drawmsel()
{
	register int i;
	int x0, y0, x1, y1;

	for ( i = 0; i < neop; i += opsz(&eops[i]) )
		if ( eselm[i] )
		{
			opbox(&eops[i], &x0, &y0, &x1, &y1);
			mselbox1(x0 - 1, y0 - 1, x1 + 2, y1 + 2);
		}
	for ( i = 0; i < npin; i++ )
		if ( pselm[i] )
			mselbox1(qtopx(epin[2*i]) - 6,
				 qtopy(epin[2*i + 1]) - 6,
				 qtopx(epin[2*i]) + 7,
				 qtopy(epin[2*i + 1]) + 7);
	return 0;
}

/* ceil/floor of a pixel offset in q units, safe for negatives. */
static
qceil(v)
{
	return (v >= 0) ? (v + SC - 1) / SC : -((-v) / SC);
}

static
qfloor(v)
{
	return (v >= 0) ? v / SC : -((-v + SC - 1) / SC);
}

/* Repaint ONE canvas rectangle: white it, re-dot the quarter grid and the
 * unit marks inside it, and redraw whatever element overlaps it. */
static
repaint_rect(x0, y0, x1, y1)
{
	register short *p;
	register int i;
	int qx, qy, q0, q1, px, py, nq, bx0, by0, bx1, by1;
	short *e;

	if ( x0 < TW ) x0 = TW;
	if ( y0 < 0 ) y0 = 0;
	if ( x1 > contw ) x1 = contw;
	if ( y1 > conth - STH ) y1 = conth - STH;
	if ( x0 >= x1 || y0 >= y1 )
		return 0;
	cl_fillrect(x0, y0, x1, y1, 1);
	q0 = qceil(x0 - ORGX);
	if ( q0 < QX0 ) q0 = QX0;
	q1 = qfloor(x1 - 1 - ORGX);
	if ( q1 > QX1 ) q1 = QX1;
	nq = q1 - q0 + 1;
	if ( nq > 0 )
		for ( qy = QY0; qy <= QY1; qy++ )
		{
			py = qtopy(qy);
			if ( py < y0 )
				continue;
			if ( py >= y1 )
				break;
			cl_dotrow(qtopx(q0), py, nq, SC);
		}
	for ( qx = QX0; qx <= QX1; qx += 4 )	/* 2x2 whole-unit marks */
	{
		px = qtopx(qx);
		if ( px + 1 < x0 || px - 1 >= x1 )
			continue;
		for ( qy = QY0; qy <= QY1; qy += 4 )
		{
			py = qtopy(qy);
			if ( py + 1 < y0 )
				continue;
			if ( py - 1 >= y1 )
				break;
			cl_fillrect(px - 1, py - 1, px + 1, py + 1, 0);
		}
	}
	px = qtopx(0);				/* the origin cross */
	py = qtopy(0);
	if ( px + 8 >= x0 && px - 8 < x1 && py + 8 >= y0 && py - 8 < y1 )
	{
		cl_line(px - 8, py, px + 8, py, 0);
		cl_line(px, py - 8, px, py + 8, 0);
	}
	p = eops;				/* overlapping elements, whole */
	e = eops + neop;
	while ( p < e )
	{
		int n;

		n = opsz(p);
		opbox(p, &bx0, &by0, &bx1, &by1);
		if ( bx1 >= x0 && bx0 < x1 && by1 >= y0 && by0 < y1 )
			drawops(p, n, ORGX, ORGY, SC);
		p += n;
	}
	for ( i = 0; i < npin; i++ )
	{
		px = qtopx(epin[2*i]);
		py = qtopy(epin[2*i + 1]);
		if ( px + 48 < x0 || px - 6 >= x1 ||
		     py + 6 < y0 || py - 12 >= y1 )
			continue;
		circle(px, py, 4, 0);
		cl_fillrect(px - 1, py - 1, px + 1, py + 1, 0);
		if ( epname[i][0] )
			cl_ptextt(SHM_FICON, px + 6, py - 10, epname[i]);
	}
	drawmsel();		/* selection markers are cheap, idempotent */
	return 0;
}

static
palcell(x, y, w, h, on)
{
	cl_line(x, y, x + w - 1, y, 0);
	cl_line(x + w - 1, y, x + w - 1, y + h - 1, 0);
	cl_line(x + w - 1, y + h - 1, x, y + h - 1, 0);
	cl_line(x, y + h - 1, x, y, 0);
	if ( on )
		cl_fillrect(x + 1, y + 1, x + w - 1, y + h - 1, 2);
	return 0;
}

/* True-scale preview: the buffer at zdraw's 2 px per quarter unit,
 * centred on its own bbox in the box under the tool cells. */
static
drawprev()
{
	register short *p;
	short *e;
	int x0, y0, x1, y1, ox, oy;

	x0 = y0 = 999;
	x1 = y1 = -999;
	p = eops;
	e = eops + neop;
	while ( p < e )
	{
		if ( *p == SE )
		{
			if ( p[1] < x0 ) x0 = p[1];
			if ( p[3] < x0 ) x0 = p[3];
			if ( p[1] > x1 ) x1 = p[1];
			if ( p[3] > x1 ) x1 = p[3];
			if ( p[2] < y0 ) y0 = p[2];
			if ( p[4] < y0 ) y0 = p[4];
			if ( p[2] > y1 ) y1 = p[2];
			if ( p[4] > y1 ) y1 = p[4];
			p += 5;
		}
		else if ( *p == SC_ || *p == SA_ )
		{
			if ( p[1] - p[3] < x0 ) x0 = p[1] - p[3];
			if ( p[1] + p[3] > x1 ) x1 = p[1] + p[3];
			if ( p[2] - p[3] < y0 ) y0 = p[2] - p[3];
			if ( p[2] + p[3] > y1 ) y1 = p[2] + p[3];
			p += (*p == SA_) ? 6 : 4;
		}
		else if ( *p == ST )
		{
			if ( p[1] < x0 ) x0 = p[1];
			if ( p[1] + 3 > x1 ) x1 = p[1] + 3;
			if ( p[2] < y0 ) y0 = p[2];
			if ( p[2] + 4 > y1 ) y1 = p[2] + 4;
			p += 4;
		}
		else
			break;
	}
	if ( x0 > x1 )
		return 0;
	ox = (TW - (x1 - x0) * 2) / 2 - x0 * 2;
	oy = PREVY + (PREVH - (y1 - y0) * 2) / 2 - y0 * 2;
	drawops(eops, neop, ox, oy, 2);
	return 0;
}

/* Draw ONE tool cell (background wiped, so it repaints in place). */
static
tcell(i, on)
{
	int y, w;

	y = i * TCH;
	cl_fillrect(0, y, TW, y + TCH, 1);
	w = strlen(toolnm[i]) * 6;
	cl_ptext(SHM_FICON, (TW - w) / 2, y + (TCH - 8) / 2, toolnm[i]);
	palcell(0, y, TW, TCH, on);
	return 0;
}

/* Wipe and redraw the true-scale preview box. */
static
drawprevbox()
{
	cl_fillrect(0, PREVY, TW - 1, PREVY + PREVH, 1);
	drawprev();
	return 0;
}

/* The symbol-step cells under the preview: matched left/right triangles
 * (like the palette scroll arrows -- never mismatched font glyphs). */
#define	STEPY	(PREVY + PREVH + 4)
#define	STEPH	16

static
symarrow(x, y, right)
{
	register int j;
	int cx, cy, hh;

	cx = x + 12;
	cy = y + STEPH / 2;
	for ( j = 0; j < 5; j++ )
	{
		hh = right ? 4 - j : j;
		cl_line(cx - 2 + j, cy - hh, cx - 2 + j, cy + hh, 0);
	}
	return 0;
}

/* Step to the previous/next symbol in the library, keeping edits. */
static
stepsym(d)
{
	if ( nlib < 2 )
		return 0;
	commit();
	cursl = (cursl + d + nlib) % nlib;
	fetch(cursl);
	return 1;
}

static
drawtools()
{
	register int i;

	cl_fillrect(0, 0, TW, conth, 1);
	for ( i = 0; i < NTOOL; i++ )
		tcell(i, i == tool);
	cl_line(0, PREVY - 3, TW - 1, PREVY - 3, 0);
	drawprev();
	symarrow(0, STEPY, 0);
	symarrow(24, STEPY, 1);
	palcell(0, STEPY, 24, STEPH, 0);
	palcell(24, STEPY, 24, STEPH, 0);
	cl_line(TW - 1, 0, TW - 1, conth - 1, 0);
	return 0;
}

static
drawstat()
{
	char t[96];
	static char *hint[] = {
		"drag the segment", "drag centre to edge",
		"drag centre out, then sweep", "click grid (again: name)",
		"click, then type", "drag it, or marquee",
		"click what goes"
	};

	cl_fillrect(TW, conth - STH, contw, conth, 1);
	cl_line(TW, conth - STH, contw - 1, conth - STH, 0);
	sprintf(t, "%-7s %3d,%-3d  %de %dp  [%.10s]%s  %s",
		ecode[0] ? ecode : "(new)", lastqx, lastqy, nel(), npin,
		libbase, edited ? " *" : "",
		arcpend ? "drag the arc's sweep" : hint[tool]);
	cl_ptext(SHM_FUI, TW + 5, conth - STH + 2, t);
	return 0;
}

/* Elements in the buffer (for the status line). */
static
nel()
{
	register short *p;
	register int n;
	short *e;

	n = 0;
	p = eops;
	e = eops + neop;
	while ( p < e )
	{
		if ( *p != SE && *p != SC_ && *p != SA_ && *p != ST )
			break;
		n++;
		p += opsz(p);
	}
	return n;
}

/* THE painter: repaint exactly the declared damage, once per event batch. */
static
flush()
{
	if ( ddcanv || ddrect )
		ruboff();	/* erase any XOR figure before repainting */
	if ( ddcanv )
	{
		drawcanvas();
		ddcanv = 0;
		ddrect = 0;
	}
	else if ( ddrect )
	{
		repaint_rect(drx0, dry0, drx1, dry1);
		ddrect = 0;
	}
	if ( ddtools )
	{
		drawtools();
		showntool = tool;
		ddtools = 0;
		ddprev = 0;
	}
	else if ( showntool != tool )
	{
		if ( showntool >= 0 )
			tcell(showntool, 0);
		tcell(tool, 1);
		showntool = tool;
	}
	if ( ddprev )
	{
		drawprevbox();
		ddprev = 0;
	}
	if ( statdirty )
	{
		drawstat();
		statdirty = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* the XOR rubber                                                     */
/* ------------------------------------------------------------------ */

/* XOR rect outline with no corner pixel plotted twice. */
static
xrect(x0, y0, x1, y1)
{
	register int t;

	if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
	if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
	cl_line(x0, y0, x1, y0, 2);
	if ( y1 > y0 )
		cl_line(x0, y1, x1, y1, 2);
	if ( y1 > y0 + 1 )
	{
		cl_line(x0, y0 + 1, x0, y1 - 1, 2);
		if ( x1 > x0 )
			cl_line(x1, y0 + 1, x1, y1 - 1, 2);
	}
	return 0;
}

static
rubdraw()
{
	long dx, dy;
	int x0, y0, x1, y1;

	switch ( drag )
	{
	case DG_SEG:
	case DG_ARC:		/* stage 1: the centre -> start-point leg */
		cl_line(qtopx(dqx), qtopy(dqy), qtopx(cqx), qtopy(cqy), 2);
		break;
	case DG_CIRC:
		dx = (cqx - dqx) * SC;
		dy = (cqy - dqy) * SC;
		circle(qtopx(dqx), qtopy(dqy),
		       (int)isqrt(dx * dx + dy * dy), 2);
		break;
	case DG_MOVE:
		if ( mselbbox(&x0, &y0, &x1, &y1) )
			xrect(x0 + (cqx - dqx) * SC, y0 + (cqy - dqy) * SC,
			      x1 + (cqx - dqx) * SC, y1 + (cqy - dqy) * SC);
		break;
	case DG_MARQ:
		xrect(qtopx(dqx), qtopy(dqy), qtopx(cqx), qtopy(cqy));
		break;
	case DG_ARC2:		/* the arc's SWEEP leg */
		cl_line(qtopx(arccx), qtopy(arccy),
			qtopx(cqx), qtopy(cqy), 2);
		break;
	}
	return 0;
}

static
ruboff()
{
	if ( rubon )
	{
		rubdraw();
		rubon = 0;
	}
	return 0;
}

/* An expose made the rubber's pixels unreliable: forget it and damage its
 * extent (never XOR-erase over freshly revealed content). */
static
rubdmg()
{
	int x0, y0, x1, y1;
	long dx, dy, r;

	if ( !rubon )
		return 0;
	if ( drag == DG_CIRC )
	{
		dx = (cqx - dqx) * SC;
		dy = (cqy - dqy) * SC;
		r = isqrt(dx * dx + dy * dy) + 2;
		dmg(qtopx(dqx) - (int)r, qtopy(dqy) - (int)r,
		    qtopx(dqx) + (int)r, qtopy(dqy) + (int)r);
	}
	else if ( drag == DG_MOVE )
	{
		if ( mselbbox(&x0, &y0, &x1, &y1) )
			dmg(x0 + (cqx - dqx) * SC - 2,
			    y0 + (cqy - dqy) * SC - 2,
			    x1 + (cqx - dqx) * SC + 2,
			    y1 + (cqy - dqy) * SC + 2);
	}
	else if ( drag == DG_ARC2 )
	{
		x0 = qtopx(arccx < cqx ? arccx : cqx);
		x1 = qtopx(arccx > cqx ? arccx : cqx);
		y0 = qtopy(arccy < cqy ? arccy : cqy);
		y1 = qtopy(arccy > cqy ? arccy : cqy);
		dmg(x0 - 2, y0 - 2, x1 + 2, y1 + 2);
	}
	else
	{
		x0 = qtopx(dqx < cqx ? dqx : cqx);
		x1 = qtopx(dqx > cqx ? dqx : cqx);
		y0 = qtopy(dqy < cqy ? dqy : cqy);
		y1 = qtopy(dqy > cqy ? dqy : cqy);
		dmg(x0 - 2, y0 - 2, x1 + 2, y1 + 2);
	}
	rubon = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the buffer                                                         */
/* ------------------------------------------------------------------ */

static
addseg(x0, y0, x1, y1)
{
	if ( neop + 5 > MAXEOP || (x0 == x1 && y0 == y1) )
		return 0;
	eops[neop++] = SE;
	eops[neop++] = x0;
	eops[neop++] = y0;
	eops[neop++] = x1;
	eops[neop++] = y1;
	edited = 1;
	dmgop(&eops[neop - 5]);
	ddprev = 1;
	statdirty = 1;
	return 1;
}

static
addcirc(cx, cy, r)
{
	if ( neop + 4 > MAXEOP || r <= 0 )
		return 0;
	eops[neop++] = SC_;
	eops[neop++] = cx;
	eops[neop++] = cy;
	eops[neop++] = r;
	edited = 1;
	dmgop(&eops[neop - 4]);
	ddprev = 1;
	statdirty = 1;
	return 1;
}

static
addchar(x, y, c)
{
	if ( neop + 4 > MAXEOP )
		return 0;
	eops[neop++] = ST;
	eops[neop++] = x;
	eops[neop++] = y;
	eops[neop++] = c;
	edited = 1;
	dmgop(&eops[neop - 4]);
	ddprev = 1;
	statdirty = 1;
	return 1;
}

static
addpin(x, y)
{
	register int i;

	for ( i = 0; i < npin; i++ )
		if ( epin[2*i] == x && epin[2*i + 1] == y )
			return 0;		/* already there */
	if ( npin >= MAXPIN )
		return 0;
	epin[2 * npin] = x;
	epin[2 * npin + 1] = y;
	epname[npin][0] = 0;
	npin++;
	edited = 1;
	dmgpin(x, y);
	statdirty = 1;
	return 1;
}

/* Remove the op tuple starting at short index i. */
static
delop(i)
{
	register int n, j;

	n = opsz(&eops[i]);
	for ( j = i; j + n < neop; j++ )
		eops[j] = eops[j + n];
	neop -= n;
	edited = 1;
	return 0;
}

/* Find the element nearest canvas pixel (px,py) within an 8 px reach:
 * fills *bpin (pin index) or *bop (op-tuple offset), the other -1.
 * Returns 1 when something was found.  Del and Move share it. */
static
findnear(px, py, bpin, bop)
int *bpin, *bop;
{
	register short *p;
	register int i;
	int bx, by, d, best;
	long dx, dy, r, dd, cross, len;

	best = 65;			/* 8 px, squared, exclusive */
	*bop = -1;
	*bpin = -1;
	for ( i = 0; i < npin; i++ )
	{
		bx = px - qtopx(epin[2*i]);
		by = py - qtopy(epin[2*i + 1]);
		if ( bx < -8 || bx > 8 || by < -8 || by > 8 )
			continue;	/* far: bx*bx would overflow 16 bits */
		d = bx * bx + by * by;
		if ( d < best )
		{
			best = d;
			*bpin = i;
		}
	}
	for ( i = 0; i < neop; )
	{
		p = &eops[i];
		if ( *p == SE )
		{
			int x0, y0, x1, y1, t;

			x0 = qtopx(p[1]);  y0 = qtopy(p[2]);
			x1 = qtopx(p[3]);  y1 = qtopy(p[4]);
			bx = x0 < x1 ? x0 : x1;
			by = x0 > x1 ? x0 : x1;
			t = y0 < y1 ? y0 : y1;
			d = y0 > y1 ? y0 : y1;
			if ( px >= bx - 8 && px <= by + 8 &&
			     py >= t - 8 && py <= d + 8 )
			{
				cross = (long)(px - x0) * (y1 - y0) -
					(long)(py - y0) * (x1 - x0);
				if ( cross < 0 )
					cross = -cross;
				len = isqrt((long)(x1 - x0) * (x1 - x0) +
					    (long)(y1 - y0) * (y1 - y0));
				if ( len == 0 )
					len = 1;
				dd = cross / len;
				if ( dd < 9 && dd * dd < best )
				{
					best = (int)(dd * dd);
					*bop = i;
					*bpin = -1;
				}
			}
		}
		else if ( *p == SC_ || *p == SA_ )
		{
			dx = px - qtopx(p[1]);
			dy = py - qtopy(p[2]);
			r = isqrt(dx * dx + dy * dy) - (long)p[3] * SC;
			if ( r < 0 )
				r = -r;
			if ( r < 9 && (int)(r * r) < best )
			{
				best = (int)(r * r);
				*bop = i;
				*bpin = -1;
			}
		}
		else if ( *p == ST )
		{
			bx = px - qtopx(p[1]);
			by = py - qtopy(p[2]);
			if ( bx >= -8 && bx <= 8 && by >= -8 && by <= 8 )
			{
				d = bx * bx + by * by;
				if ( d < best )
				{
					best = d;
					*bop = i;
					*bpin = -1;
				}
			}
		}
		else
			break;
		i += opsz(p);
	}
	return *bop >= 0 || *bpin >= 0;
}

/* Delete pin bp (compacting names along). */
static
delpin(bp)
register int bp;
{
	register int i;

	dmgpin(epin[2*bp], epin[2*bp + 1]);
	for ( i = bp; i < npin - 1; i++ )
	{
		epin[2*i] = epin[2*i + 2];
		epin[2*i + 1] = epin[2*i + 3];
		strcpy(epname[i], epname[i + 1]);
	}
	npin--;
	edited = 1;
	statdirty = 1;
	return 0;
}

/* Delete the element nearest canvas pixel (px,py). */
static
delnear(px, py)
{
	int bp, bi;

	if ( !findnear(px, py, &bp, &bi) )
		return 0;
	if ( bp >= 0 )
	{
		delpin(bp);
		return 1;
	}
	dmgop(&eops[bi]);
	delop(bi);
	ddprev = 1;
	statdirty = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* the Move tool: marquee selection + drag                            */
/* ------------------------------------------------------------------ */

static
mselclear()
{
	register int i;

	for ( i = 0; i < MAXEOP; i++ )
		eselm[i] = 0;
	for ( i = 0; i < MAXPIN; i++ )
		pselm[i] = 0;
	nselm = 0;
	return 0;
}

static
dmgmsel()
{
	register int i;

	for ( i = 0; i < neop; i += opsz(&eops[i]) )
		if ( eselm[i] )
			dmgop(&eops[i]);
	for ( i = 0; i < npin; i++ )
		if ( pselm[i] )
			dmgpin(epin[2*i], epin[2*i + 1]);
	return 0;
}

/* Union canvas-px bbox of the selection; 0 if empty. */
static
mselbbox(bx0, by0, bx1, by1)
int *bx0, *by0, *bx1, *by1;
{
	register int i;
	int x0, y0, x1, y1, got;

	got = 0;
	for ( i = 0; i < neop; i += opsz(&eops[i]) )
	{
		if ( !eselm[i] )
			continue;
		opbox(&eops[i], &x0, &y0, &x1, &y1);
		if ( !got )
		{
			*bx0 = x0;  *by0 = y0;  *bx1 = x1;  *by1 = y1;
			got = 1;
		}
		else
		{
			if ( x0 < *bx0 ) *bx0 = x0;
			if ( y0 < *by0 ) *by0 = y0;
			if ( x1 > *bx1 ) *bx1 = x1;
			if ( y1 > *by1 ) *by1 = y1;
		}
	}
	for ( i = 0; i < npin; i++ )
	{
		if ( !pselm[i] )
			continue;
		x0 = qtopx(epin[2*i]) - 5;
		y0 = qtopy(epin[2*i + 1]) - 5;
		x1 = x0 + 10;
		y1 = y0 + 10;
		if ( !got )
		{
			*bx0 = x0;  *by0 = y0;  *bx1 = x1;  *by1 = y1;
			got = 1;
		}
		else
		{
			if ( x0 < *bx0 ) *bx0 = x0;
			if ( y0 < *by0 ) *by0 = y0;
			if ( x1 > *bx1 ) *bx1 = x1;
			if ( y1 > *by1 ) *by1 = y1;
		}
	}
	return got;
}

/* Move the whole selection by (mx,my) q units. */
static
mselmove(mx, my)
{
	register short *p;
	register int i;

	for ( i = 0; i < neop; i += opsz(&eops[i]) )
	{
		if ( !eselm[i] )
			continue;
		p = &eops[i];
		p[1] += mx;
		p[2] += my;
		if ( *p == SE )
		{
			p[3] += mx;
			p[4] += my;
		}
	}
	for ( i = 0; i < npin; i++ )
		if ( pselm[i] )
		{
			epin[2*i] += mx;
			epin[2*i + 1] += my;
		}
	edited = 1;
	ddprev = 1;
	statdirty = 1;
	return 0;
}

/* Delete the whole selection. */
static
mseldel()
{
	register int i;
	int off[MAXEOP / 4], no;

	if ( nselm == 0 )
		return 0;
	dmgmsel();
	no = 0;
	for ( i = 0; i < neop; i += opsz(&eops[i]) )
		if ( eselm[i] )
			off[no++] = i;
	while ( no-- > 0 )		/* descending: offsets stay valid */
		delop(off[no]);
	for ( i = npin - 1; i >= 0; i-- )
		if ( pselm[i] )
			delpin(i);
	mselclear();
	ddprev = 1;
	statdirty = 1;
	return 1;
}

/* The gray marker round one selected element (both painters call it). */
static
mselbox1(x0, y0, x1, y1)
{
	cl_fillrect(x0, y0, x1, y0 + 1, 3);
	cl_fillrect(x0, y1 - 1, x1, y1, 3);
	cl_fillrect(x0, y0 + 1, x0 + 1, y1 - 1, 3);
	cl_fillrect(x1 - 1, y0 + 1, x1, y1 - 1, 3);
	return 0;
}

/* ------------------------------------------------------------------ */
/* arcs                                                               */
/* ------------------------------------------------------------------ */

static
addarc(cx, cy, r, a0, a1)
{
	if ( neop + 6 > MAXEOP || r <= 0 || a0 == a1 )
		return 0;
	eops[neop++] = SA_;
	eops[neop++] = cx;
	eops[neop++] = cy;
	eops[neop++] = r;
	eops[neop++] = a0;
	eops[neop++] = a1;
	edited = 1;
	dmgop(&eops[neop - 6]);
	ddprev = 1;
	statdirty = 1;
	return 1;
}

/* Angles snap to 5 degrees -- drawn arcs land on round figures. */
static
asnap(a)
{
	a = ((a + 2) / 5) * 5;
	return ((a % 360) + 360) % 360;
}

/* ------------------------------------------------------------------ */
/* whole-buffer transforms ('r' rotate a quarter turn, 'm' mirror)    */
/* ------------------------------------------------------------------ */

static
rotbuf()
{
	register short *p;
	register int i;
	int t;

	for ( i = 0; i < neop; i += opsz(&eops[i]) )
	{
		p = &eops[i];
		t = p[1];
		p[1] = -p[2];
		p[2] = t;
		if ( *p == SE )
		{
			t = p[3];
			p[3] = -p[4];
			p[4] = t;
		}
		else if ( *p == SA_ )
		{
			p[4] = ((p[4] - 90) % 360 + 360) % 360;
			p[5] = ((p[5] - 90) % 360 + 360) % 360;
		}
	}
	for ( i = 0; i < npin; i++ )
	{
		t = epin[2*i];
		epin[2*i] = -epin[2*i + 1];
		epin[2*i + 1] = t;
	}
	edited = 1;
	ddcanv = 1;
	ddprev = 1;
	statdirty = 1;
	return 1;
}

static
mirbuf()
{
	register short *p;
	register int i;
	int t;

	for ( i = 0; i < neop; i += opsz(&eops[i]) )
	{
		p = &eops[i];
		p[1] = -p[1];
		if ( *p == SE )
			p[3] = -p[3];
		else if ( *p == SA_ )
		{
			t = p[4];
			p[4] = ((180 - p[5]) % 360 + 360) % 360;
			p[5] = ((180 - t) % 360 + 360) % 360;
		}
	}
	for ( i = 0; i < npin; i++ )
		epin[2*i] = -epin[2*i];
	edited = 1;
	ddcanv = 1;
	ddprev = 1;
	statdirty = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* the library                                                        */
/* ------------------------------------------------------------------ */

/* Write the edit buffer back into its slot (in memory only). */
static
commit()
{
	register CSYM *c;
	register int i;

	if ( cursl < 0 || cursl >= nlib )
		return 0;
	c = &lib[cursl];
	strcpy(c->cs_code, ecode);
	strcpy(c->cs_pfx, epfx);
	for ( i = 0; i < neop && i < SLOTOPS - 1; i++ )
		c->cs_ops[i] = eops[i];
	c->cs_nop = i;
	c->cs_ops[i] = SEND;
	for ( i = 0; i < 2 * npin; i++ )
		c->cs_pin[i] = epin[i];
	for ( i = 0; i < npin; i++ )
		strcpy(c->cs_pnm[i], epname[i]);
	c->cs_npin = npin;
	return 0;
}

/* Load slot sl into the edit buffer. */
static
fetch(sl)
{
	register CSYM *c;
	register int i;

	c = &lib[sl];
	strcpy(ecode, c->cs_code);
	strcpy(epfx, c->cs_pfx);
	for ( i = 0; i < c->cs_nop; i++ )
		eops[i] = c->cs_ops[i];
	neop = c->cs_nop;
	for ( i = 0; i < 2 * c->cs_npin; i++ )
		epin[i] = c->cs_pin[i];
	for ( i = 0; i < c->cs_npin; i++ )
		strcpy(epname[i], c->cs_pnm[i]);
	npin = c->cs_npin;
	cursl = sl;
	mselclear();			/* selection offsets are stale */
	arcpend = 0;
	ddcanv = 1;			/* a different symbol fills the canvas */
	ddprev = 1;
	statdirty = 1;
	return 0;
}

static char *
tok(pp)
char **pp;
{
	register char *p, *s;

	p = *pp;
	while ( *p == ' ' || *p == '\t' )
		p++;
	if ( *p == 0 || *p == '\n' )
		return 0;
	s = p;
	while ( *p && *p != ' ' && *p != '\t' && *p != '\n' )
		p++;
	if ( *p )
		*p++ = 0;
	*pp = p;
	return s;
}

static
loadlib()
{
	register FILE *fp;
	register CSYM *c;
	char lb[80];
	char *p, *t;
	int i, in;

	nlib = 0;
	if ( (fp = fopen(libfile, "r")) == (FILE *)0 )
		return 0;
	in = 0;
	c = (CSYM *)0;
	while ( fgets(lb, sizeof(lb), fp) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 || t[0] == '#' )
			continue;
		if ( strcmp(t, "symbol") == 0 )
		{
			in = 0;
			if ( nlib >= MAXCUST || (t = tok(&p)) == 0 )
				continue;
			c = &lib[nlib];
			strncpy(c->cs_code, t, 7);
			c->cs_code[7] = 0;
			c->cs_pfx[0] = 0;
			if ( (t = tok(&p)) != 0 && strcmp(t, "-") != 0 )
			{
				strncpy(c->cs_pfx, t, 3);
				c->cs_pfx[3] = 0;
			}
			c->cs_nop = 0;
			c->cs_npin = 0;
			in = 1;
			continue;
		}
		if ( !in )
			continue;
		if ( strcmp(t, "end") == 0 )
		{
			c->cs_ops[c->cs_nop] = SEND;
			nlib++;
			in = 0;
			continue;
		}
		if ( strcmp(t, "s") == 0 && c->cs_nop + 6 < SLOTOPS )
		{
			c->cs_ops[c->cs_nop] = SE;
			for ( i = 1; i <= 4; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				c->cs_ops[c->cs_nop + i] = atoi(t);
			}
			if ( i > 4 )
				c->cs_nop += 5;
		}
		else if ( strcmp(t, "c") == 0 && c->cs_nop + 5 < SLOTOPS )
		{
			c->cs_ops[c->cs_nop] = SC_;
			for ( i = 1; i <= 3; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				c->cs_ops[c->cs_nop + i] = atoi(t);
			}
			if ( i > 3 )
				c->cs_nop += 4;
		}
		else if ( strcmp(t, "t") == 0 && c->cs_nop + 5 < SLOTOPS )
		{
			c->cs_ops[c->cs_nop] = ST;
			for ( i = 1; i <= 2; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				c->cs_ops[c->cs_nop + i] = atoi(t);
			}
			if ( i > 2 && (t = tok(&p)) != 0 )
			{
				c->cs_ops[c->cs_nop + 3] = t[0];
				c->cs_nop += 4;
			}
		}
		else if ( strcmp(t, "a") == 0 && c->cs_nop + 7 < SLOTOPS )
		{
			c->cs_ops[c->cs_nop] = SA_;
			for ( i = 1; i <= 5; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				c->cs_ops[c->cs_nop + i] = atoi(t);
			}
			if ( i > 5 )
				c->cs_nop += 6;
		}
		else if ( strcmp(t, "p") == 0 && c->cs_npin < MAXPIN )
		{
			if ( (t = tok(&p)) == 0 )
				continue;
			c->cs_pin[2 * c->cs_npin] = atoi(t);
			if ( (t = tok(&p)) == 0 )
				continue;
			c->cs_pin[2 * c->cs_npin + 1] = atoi(t);
			c->cs_pnm[c->cs_npin][0] = 0;
			if ( (t = tok(&p)) != 0 && strcmp(t, "-") != 0 )
			{
				strncpy(c->cs_pnm[c->cs_npin], t, 7);
				c->cs_pnm[c->cs_npin][7] = 0;
			}
			c->cs_npin++;
		}
	}
	fclose(fp);
	return 0;
}

static
savelib()
{
	register FILE *fp;
	register CSYM *c;
	register short *p;
	int sl, i;

	commit();
	if ( (fp = fopen(libfile, "w")) == (FILE *)0 )
		return -1;
	fprintf(fp, "# vellum symbol library (edited by symedit)\n");
	for ( sl = 0; sl < nlib; sl++ )
	{
		c = &lib[sl];
		fprintf(fp, "symbol %s %s\n", c->cs_code,
			c->cs_pfx[0] ? c->cs_pfx : "-");
		p = c->cs_ops;
		while ( *p != SEND )
		{
			if ( *p == SE )
			{
				fprintf(fp, "s %d %d %d %d\n",
					p[1], p[2], p[3], p[4]);
				p += 5;
			}
			else if ( *p == SC_ )
			{
				fprintf(fp, "c %d %d %d\n", p[1], p[2], p[3]);
				p += 4;
			}
			else if ( *p == SA_ )
			{
				fprintf(fp, "a %d %d %d %d %d\n",
					p[1], p[2], p[3], p[4], p[5]);
				p += 6;
			}
			else
			{
				fprintf(fp, "t %d %d %c\n",
					p[1], p[2], p[3]);
				p += 4;
			}
		}
		for ( i = 0; i < c->cs_npin; i++ )
		{
			if ( c->cs_pnm[i][0] )
				fprintf(fp, "p %d %d %s\n", c->cs_pin[2*i],
					c->cs_pin[2*i + 1], c->cs_pnm[i]);
			else
				fprintf(fp, "p %d %d\n",
					c->cs_pin[2*i], c->cs_pin[2*i + 1]);
		}
		fprintf(fp, "end\n");
	}
	fclose(fp);
	sync();		/* a library SAVED should survive a power cut */
	edited = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* dialogs                                                            */
/* ------------------------------------------------------------------ */

char	ncode[8], npfx[4];
char	dmsg[36];

HRWIDGET nwg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Code:" },
    { DW_TEXT,    90,  12,  90, 22, (char *)0, 0, 0, ncode, sizeof(ncode) },
    { DW_LABEL,   12,  46,   0,  0, "Prefix:" },
    { DW_TEXT,    90,  42,  60, 22, (char *)0, 0, 0, npfx, sizeof(npfx) },
    { DW_LABEL,   12,  76,   0,  0, dmsg },
    { DW_BUTTON,  50, 102,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 160, 102,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NNWG	(sizeof(nwg) / sizeof(nwg[0]))
#define	NW_MSG	4
#define	NW_OK	5

/* New symbol: ask code + prefix, append a fresh empty slot. */
static
donew()
{
	int w, h, r;
	register int i;

	if ( nlib >= MAXCUST )
		return 0;
	ncode[0] = 0;
	npfx[0] = 0;
	dmsg[0] = 0;
	w = 260;
	h = 146;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	for (;;)
	{
		cl_fillrect(nwg[NW_MSG].dw_x, nwg[NW_MSG].dw_y, w,
			    nwg[NW_MSG].dw_y + hr_font(SHM_FUI)->cellh, 1);
		hr_dlgdraw(nwg, NNWG);
		r = hr_dlgrun(nwg, NNWG);
		if ( r == -1 )
		{
			hr_dlgclose();
			exit(0);
		}
		if ( r != NW_OK )
		{
			hr_dlgclose();
			return 0;
		}
		if ( ncode[0] == 0 )
		{
			strcpy(dmsg, "Enter a code");
			continue;
		}
		for ( i = 0; i < nlib; i++ )
			if ( strcmp(lib[i].cs_code, ncode) == 0 )
				break;
		if ( i < nlib )
		{
			strcpy(dmsg, "That code exists");
			continue;
		}
		break;
	}
	hr_dlgclose();
	commit();			/* keep what we were editing */
	cursl = nlib++;
	strcpy(ecode, ncode);
	strcpy(epfx, npfx);
	neop = 0;
	npin = 0;
	commit();			/* the fresh slot exists at once */
	edited = 1;
	ddcanv = 1;
	ddprev = 1;
	statdirty = 1;
	return 1;
}

/* Open by code; the message line lists what the library holds. */
char	ocode[8];
char	omsg[40];

HRWIDGET owg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Code:" },
    { DW_TEXT,    90,  12,  90, 22, (char *)0, 0, 0, ocode, sizeof(ocode) },
    { DW_LABEL,   12,  46,   0,  0, omsg },
    { DW_BUTTON,  50,  72,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 160,  72,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NOWG	(sizeof(owg) / sizeof(owg[0]))
#define	OW_OK	3

static
doopen()
{
	int w, h, r;
	register int i;

	ocode[0] = 0;
	omsg[0] = 0;
	for ( i = 0; i < nlib && strlen(omsg) + 9 < sizeof(omsg); i++ )
	{
		if ( i )
			strcat(omsg, " ");
		strcat(omsg, lib[i].cs_code);
	}
	if ( nlib == 0 )
		strcpy(omsg, "(library is empty)");
	w = 260;
	h = 116;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	for (;;)
	{
		hr_dlgdraw(owg, NOWG);
		r = hr_dlgrun(owg, NOWG);
		if ( r == -1 )
		{
			hr_dlgclose();
			exit(0);
		}
		if ( r != OW_OK )
		{
			hr_dlgclose();
			return 0;
		}
		for ( i = 0; i < nlib; i++ )
			if ( strcmp(lib[i].cs_code, ocode) == 0 )
				break;
		if ( i == nlib )
		{
			strcpy(omsg, "No such symbol");
			continue;
		}
		break;
	}
	hr_dlgclose();
	commit();
	fetch(i);
	return 1;
}

/* One character for the Char tool. */
char	cbuf[4];

HRWIDGET cwg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Character:" },
    { DW_TEXT,   120,  12,  40, 22, (char *)0, 0, 0, cbuf, sizeof(cbuf) },
    { DW_BUTTON,  40,  46,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 150,  46,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NCWG	(sizeof(cwg) / sizeof(cwg[0]))
#define	CW_OK	2

static
chardlg()
{
	int w, h, r;

	cbuf[0] = 0;
	w = 250;
	h = 90;
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
	return r == CW_OK && cbuf[0] != 0;
}

/* Copy-from: pull a symbol out of ANOTHER library file as the starting
 * point ('f').  The buffer is replaced; code/prefix stay ours. */
char	cfpath[44];
char	cfcode[8];
char	cfmsg[36];

HRWIDGET fwg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Library:" },
    { DW_TEXT,   100,  12, 240, 22, (char *)0, 0, 0, cfpath, sizeof(cfpath) },
    { DW_LABEL,   12,  46,   0,  0, "Code:" },
    { DW_TEXT,   100,  42,  90, 22, (char *)0, 0, 0, cfcode, sizeof(cfcode) },
    { DW_LABEL,   12,  76,   0,  0, cfmsg },
    { DW_BUTTON,  60, 100,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170, 100,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NFWG	(sizeof(fwg) / sizeof(fwg[0]))
#define	FW_MSG	4
#define	FW_OK	5

/* Parse ONE symbol out of a foreign library file into the edit buffer.
 * Returns 1 when found. */
static
pullsym(path, code)
char *path, *code;
{
	register FILE *fp;
	char lb[80];
	char *p, *t;
	int i, in;

	if ( (fp = fopen(path, "r")) == (FILE *)0 )
		return 0;
	in = 0;
	while ( fgets(lb, sizeof(lb), fp) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 || t[0] == '#' )
			continue;
		if ( strcmp(t, "symbol") == 0 )
		{
			in = 0;
			if ( (t = tok(&p)) != 0 && strcmp(t, code) == 0 )
			{
				neop = 0;
				npin = 0;
				in = 1;
			}
			continue;
		}
		if ( !in )
			continue;
		if ( strcmp(t, "end") == 0 )
		{
			fclose(fp);
			edited = 1;
			ddcanv = 1;
			ddprev = 1;
			statdirty = 1;
			mselclear();
			return 1;
		}
		if ( strcmp(t, "s") == 0 && neop + 5 <= MAXEOP )
		{
			eops[neop] = SE;
			for ( i = 1; i <= 4; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				eops[neop + i] = atoi(t);
			}
			if ( i > 4 )
				neop += 5;
		}
		else if ( strcmp(t, "c") == 0 && neop + 4 <= MAXEOP )
		{
			eops[neop] = SC_;
			for ( i = 1; i <= 3; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				eops[neop + i] = atoi(t);
			}
			if ( i > 3 )
				neop += 4;
		}
		else if ( strcmp(t, "a") == 0 && neop + 6 <= MAXEOP )
		{
			eops[neop] = SA_;
			for ( i = 1; i <= 5; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				eops[neop + i] = atoi(t);
			}
			if ( i > 5 )
				neop += 6;
		}
		else if ( strcmp(t, "t") == 0 && neop + 4 <= MAXEOP )
		{
			eops[neop] = ST;
			for ( i = 1; i <= 2; i++ )
			{
				if ( (t = tok(&p)) == 0 )
					break;
				eops[neop + i] = atoi(t);
			}
			if ( i > 2 && (t = tok(&p)) != 0 )
			{
				eops[neop + 3] = t[0];
				neop += 4;
			}
		}
		else if ( strcmp(t, "p") == 0 && npin < MAXPIN )
		{
			if ( (t = tok(&p)) == 0 )
				continue;
			epin[2 * npin] = atoi(t);
			if ( (t = tok(&p)) == 0 )
				continue;
			epin[2 * npin + 1] = atoi(t);
			epname[npin][0] = 0;
			if ( (t = tok(&p)) != 0 && strcmp(t, "-") != 0 )
			{
				strncpy(epname[npin], t, 7);
				epname[npin][7] = 0;
			}
			npin++;
		}
	}
	fclose(fp);
	return 0;
}

static
docopyfrom()
{
	int w, h, r;

	cfmsg[0] = 0;
	if ( cfpath[0] == 0 )
		strcpy(cfpath, "/usr/vellum/lib/");
	w = 360;
	h = 100 + DLG_BTNH + DLG_BSHAD + 10;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	for (;;)
	{
		cl_fillrect(fwg[FW_MSG].dw_x, fwg[FW_MSG].dw_y, w,
			    fwg[FW_MSG].dw_y + 16, 1);
		hr_dlgdraw(fwg, NFWG);
		r = hr_dlgrun(fwg, NFWG);
		if ( r == -1 )
		{
			hr_dlgclose();
			exit(0);
		}
		if ( r != FW_OK )
			break;
		if ( cfpath[0] == 0 || cfcode[0] == 0 )
		{
			strcpy(cfmsg, "Enter library and code");
			continue;
		}
		if ( !pullsym(cfpath, cfcode) )
		{
			strcpy(cfmsg, "No such symbol there");
			continue;
		}
		break;
	}
	hr_dlgclose();
	statdirty = 1;
	return 1;
}

HRWIDGET hwg[] = {
    { DW_LABEL,  12,  10, 0, 0, "Draws ONE symbol for Vellum's palette;" },
    { DW_LABEL,  12,  28, 0, 0, "the whole library is saved together." },
    { DW_LABEL,  12,  54, 0, 0, "Seg/Circ: drag.  Arc: drag centre out," },
    { DW_LABEL,  12,  72, 0, 0, "then drag the sweep.  Pin: click a" },
    { DW_LABEL,  12,  90, 0, 0, "grid mark; click AGAIN to name it." },
    { DW_LABEL,  12, 108, 0, 0, "Move: drag one, or marquee + drag;" },
    { DW_LABEL,  12, 126, 0, 0, "x deletes the selection." },
    { DW_LABEL,  12, 152, 0, 0, "r/m rotate/mirror the WHOLE symbol." },
    { DW_LABEL,  12, 170, 0, 0, "f copies one in from another library." },
    { DW_LABEL,  12, 196, 0, 0, "The cross is the ORIGIN.  < > step" },
    { DW_LABEL,  12, 214, 0, 0, "through the library; Menu: New, Open," },
    { DW_LABEL,  12, 232, 0, 0, "Save.  Vellum rereads on restart." },
    { DW_BUTTON, 150, 258, 70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NHWG	(sizeof(hwg) / sizeof(hwg[0]))

static
dohelp()
{
	int w, h, r;

	w = 372;
	h = 296;
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
/* input                                                              */
/* ------------------------------------------------------------------ */

static
dokey(c)
{
	c &= 0xff;
	switch ( c )
	{
	case 's':	tool = T_SEG;	arcpend = 0;	return 1;
	case 'c':	tool = T_CIRC;	arcpend = 0;	return 1;
	case 'a':	tool = T_ARC;	return 1;
	case 'p':	tool = T_PIN;	arcpend = 0;	return 1;
	case 't':	tool = T_CHR;	arcpend = 0;	return 1;
	case 'v':	tool = T_MOVE;	arcpend = 0;	return 1;
	case 'e':	tool = T_DEL;	arcpend = 0;	return 1;
	case 'r':	return rotbuf();	/* whole buffer */
	case 'm':	return mirbuf();
	case 'f':	return docopyfrom();	/* pull from another lib */
	case 'x':
	case 0x7f:
		return mseldel();
	case 0x1b:
		tool = T_SEG;
		arcpend = 0;
		if ( nselm )
		{
			dmgmsel();
			mselclear();
		}
		return 1;
	}
	return 0;
}

/* Ask for a pin's NAME (netlists print Q1.B); empty clears it. */
char	pnbuf[8];

HRWIDGET pwg[] = {
    { DW_LABEL,   12,  16,   0,  0, "Pin name:" },
    { DW_TEXT,   120,  12,  90, 22, (char *)0, 0, 0, pnbuf, sizeof(pnbuf) },
    { DW_BUTTON,  40,  46,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 150,  46,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NPWG	(sizeof(pwg) / sizeof(pwg[0]))
#define	PW_OK	2

static
pinnamedlg(i)
{
	int w, h, r;

	strcpy(pnbuf, epname[i]);
	w = 260;
	h = 90;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(pwg, NPWG);
	r = hr_dlgrun(pwg, NPWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	if ( r == PW_OK )
	{
		strcpy(epname[i], pnbuf);
		edited = 1;
		dmgpin(epin[2*i], epin[2*i + 1]);
		statdirty = 1;
	}
	return 1;
}

static
canvpress(px, py)
{
	int qx, qy, bp, bi;

	qx = clampq(pxtoq(px, ORGX), QX0, QX1);
	qy = clampq(pxtoq(py, ORGY), QY0, QY1);
	lastqx = qx;
	lastqy = qy;
	if ( arcpend )
	{
		/* stage 2: this press starts the SWEEP drag; its release
		 * fixes the arc's end angle */
		drag = DG_ARC2;
		cqx = qx;
		cqy = qy;
		rubdraw();
		rubon = 1;
		return 0;
	}
	switch ( tool )
	{
	case T_SEG:
	case T_CIRC:
	case T_ARC:
		drag = (tool == T_SEG) ? DG_SEG :
		       (tool == T_CIRC) ? DG_CIRC : DG_ARC;
		dqx = cqx = qx;
		dqy = cqy = qy;
		rubdraw();
		rubon = 1;
		return 0;

	case T_PIN:
		/* snap to a whole grid unit: pins must sit on the grid */
		qx = ((qx >= 0) ? (qx + 2) : (qx - 2)) / 4 * 4;
		qy = ((qy >= 0) ? (qy + 2) : (qy - 2)) / 4 * 4;
		if ( !addpin(qx, qy) )
		{
			register int i;

			/* a pin is already there: name it */
			for ( i = 0; i < npin; i++ )
				if ( epin[2*i] == qx &&
				     epin[2*i + 1] == qy )
					return pinnamedlg(i);
		}
		return 1;

	case T_CHR:
		if ( chardlg() )
			addchar(qx, qy, cbuf[0]);
		return 1;

	case T_MOVE:
		if ( findnear(px, py, &bp, &bi) )
		{
			/* on an UNSELECTED element it becomes the whole
			 * selection; a selected one drags the set */
			if ( (bp >= 0 && !pselm[bp]) ||
			     (bi >= 0 && !eselm[bi]) )
			{
				dmgmsel();
				mselclear();
				if ( bp >= 0 )
					pselm[bp] = 1;
				else
					eselm[bi] = 1;
				nselm = 1;
				if ( bp >= 0 )
					dmgpin(epin[2*bp],
					       epin[2*bp + 1]);
				else
					dmgop(&eops[bi]);
			}
			drag = DG_MOVE;
			dqx = cqx = qx;
			dqy = cqy = qy;
			return 1;
		}
		/* empty canvas: marquee */
		if ( nselm )
		{
			dmgmsel();
			mselclear();
		}
		drag = DG_MARQ;
		dqx = cqx = qx;
		dqy = cqy = qy;
		rubdraw();
		rubon = 1;
		return 1;

	case T_DEL:
		return delnear(px, py);
	}
	return 0;
}

static
canvmotion(px, py)
{
	int qx, qy;

	qx = clampq(pxtoq(px, ORGX), QX0, QX1);
	qy = clampq(pxtoq(py, ORGY), QY0, QY1);
	lastqx = qx;
	lastqy = qy;
	statdirty = 1;
	if ( drag == 0 || (qx == cqx && qy == cqy) )
		return 0;
	ruboff();
	cqx = qx;
	cqy = qy;
	rubdraw();
	rubon = 1;
	return 0;
}

static
canvrelease()
{
	int d;
	long dx, dy;

	if ( drag == 0 )
		return 0;
	ruboff();
	d = drag;
	drag = 0;
	switch ( d )
	{
	case DG_SEG:
		addseg(dqx, dqy, cqx, cqy);
		break;

	case DG_CIRC:
		dx = cqx - dqx;
		dy = cqy - dqy;
		addcirc(dqx, dqy, (int)isqrt(dx * dx + dy * dy));
		break;

	case DG_ARC:
		/* centre + radius + start angle fixed; the next
		 * press-drag-release sweeps and fixes the end */
		dx = cqx - dqx;
		dy = cqy - dqy;
		arcr = (int)isqrt(dx * dx + dy * dy);
		if ( arcr < 1 )
			break;
		arccx = dqx;
		arccy = dqy;
		arca0 = asnap(iangle((int)dx, (int)-dy));
		arcpend = 1;
		statdirty = 1;
		break;

	case DG_ARC2:
		arcpend = 0;
		addarc(arccx, arccy, arcr, arca0,
		       asnap(iangle(cqx - arccx, -(cqy - arccy))));
		statdirty = 1;
		break;

	case DG_MOVE:
		if ( cqx != dqx || cqy != dqy )
		{
			dmgmsel();		/* where it was */
			mselmove(cqx - dqx, cqy - dqy);
			dmgmsel();		/* where it is  */
		}
		break;

	case DG_MARQ:
		{
			register int i;
			int x0, y0, x1, y1, rx0, ry0, rx1, ry1, t;

			rx0 = qtopx(dqx < cqx ? dqx : cqx);
			rx1 = qtopx(dqx > cqx ? dqx : cqx);
			ry0 = qtopy(dqy < cqy ? dqy : cqy);
			ry1 = qtopy(dqy > cqy ? dqy : cqy);
			nselm = 0;
			for ( i = 0; i < neop; i += opsz(&eops[i]) )
			{
				opbox(&eops[i], &x0, &y0, &x1, &y1);
				if ( x0 >= rx0 && x1 <= rx1 &&
				     y0 >= ry0 && y1 <= ry1 )
				{
					eselm[i] = 1;
					nselm++;
					dmgop(&eops[i]);
				}
			}
			for ( i = 0; i < npin; i++ )
			{
				t = qtopx(epin[2*i]);
				x0 = qtopy(epin[2*i + 1]);
				if ( t >= rx0 && t <= rx1 &&
				     x0 >= ry0 && x0 <= ry1 )
				{
					pselm[i] = 1;
					nselm++;
					dmgpin(epin[2*i], epin[2*i + 1]);
				}
			}
			statdirty = 1;
		}
		break;
	}
	return 1;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

main(argc, argv)
char **argv;
{
	WMSG e;
	register int i;
	register char *p;

	me.ha_w = TW + (QX1 - QX0) * SC;
	me.ha_h = (QY1 - QY0) * SC + STH;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;
	conth = me.ha_h;
	/* An optional argument names the library FILE to edit (zdraw's Edit
	 * button passes the current library); default is the user scratch. */
	if ( argc > 1 && argv[1][0] &&
	     strlen(argv[1]) < sizeof(libfile) )
		strcpy(libfile, argv[1]);
	libbase = libfile;
	for ( p = libfile; *p; p++ )
		if ( *p == '/' )
			libbase = p + 1;
	loadlib();
	if ( nlib > 0 )
		fetch(0);

	alldirty();
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		cl_begin();
		flush();
		cl_end();
	}
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
				/* partial expose: repaint the rect, not all */
				{
					int ex0, ey0, ex1, ey1;

					ex0 = e.wm_arg[0];
					ey0 = e.wm_arg[1];
					ex1 = ex0 + e.wm_arg[2];
					ey1 = ey0 + e.wm_arg[3];
					if ( ex0 <= 0 && ey0 <= 0 &&
					     ex1 >= contw && ey1 >= conth )
					{
						drag = 0;
						rubon = 0;
						alldirty();
						break;
					}
					rubdmg();
					dmg(ex0, ey0, ex1, ey1);
					if ( ex0 < TW )
						ddtools = 1;
					if ( ey1 > conth - STH )
						statdirty = 1;
				}
				break;

			case E_RESIZE:
				drag = 0;
				rubon = 0;
				alldirty();
				break;

			case E_KEY:
				if ( drag == 0 )
					dokey(e.wm_arg[0]);
				statdirty = 1;
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					if ( e.wm_arg[0] < TW )
					{
						int i;

						if ( e.wm_arg[1] >= STEPY &&
						     e.wm_arg[1] <
							STEPY + STEPH )
						{
							/* < > : step through
							 * the library */
							stepsym(e.wm_arg[0]
								< 24 ? -1 : 1);
							statdirty = 1;
						}
						else
						{
							i = e.wm_arg[1] / TCH;
							if ( i < NTOOL )
							{
								tool = i;
								statdirty = 1;
							}
						}
					}
					else if ( e.wm_arg[1] < conth - STH )
					{
						canvpress(e.wm_arg[0],
							  e.wm_arg[1]);
						statdirty = 1;
					}
				}
				else
					canvrelease();
				break;

			case E_MOTION:
				if ( drag )
					canvmotion(e.wm_arg[0], e.wm_arg[1]);
				break;

			case E_MENU:
				switch ( e.wm_arg[0] )
				{
				case HRM_NEW:	donew();	break;
				case HRM_OPEN:	doopen();	break;
				case HRM_SAVE:	savelib();	break;
				case HRM_HELP:	dohelp();	break;
				}
				statdirty = 1;
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )
			alldirty();
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )
				alldirty();
			cl_begin();
			flush();
			cl_end();
		}
	}
}
