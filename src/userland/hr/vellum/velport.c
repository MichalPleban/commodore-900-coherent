/*
 * velport.c - Vellum's headless exporters: every render/export works
 * from the command line, with no window and no server connection, so
 * make(1) and shell scripts are the program's second user.
 *
 *	vellum -print file.d > /dev/lp    Epson raster (ESC * bands)
 *	vellum -pic   file.d > fig.pic    troff pic source
 *	vellum -net   file.d              netlist (schematics)
 *
 * One object WALKER feeds a 6-function backend struct: the print
 * backend rasterizes into a 1-bpp band buffer (one band = one 24-pin
 * head pass), the pic backend emits pic primitives.  Scale: 1 grid
 * unit = 8 printer dots (device px); pic converts device px to inches
 * at 16 units/inch.  Layers marked no-print and the construction layer
 * are skipped.
 */
#include <stdio.h>
#include "vellum.h"

extern char	*malloc();

/* ---- the backend contract ---- */
typedef struct {
	int	(*b_line)();	/* x0,y0,x1,y1 (device px)                */
	int	(*b_box)();	/* x0,y0,x1,y1, fill (-1 none/0 blk/     */
				/* 1 wht/2 gray)                          */
	int	(*b_circle)();	/* cx,cy,r, fill                          */
	int	(*b_text)();	/* x,y, size, str (y = cell top)          */
	int	(*b_span)();	/* x0,x1,y, val -- 0: backend can't fill  */
	int	(*b_style)();	/* style bits (OF_STYLE|OF_BOLD) for the  */
				/* following lines                        */
	int	(*b_vtext)();	/* x,y, size, str VERTICAL (glyph
				 * transpose) -- 0: fall back to b_text   */
	int	(*b_poly)();	/* xy[], n: a SMOOTH polyline as the
				 * backend's own curve (pic spline) --
				 * 0: the walker chords it via bspline    */
} XB;

int	xsc	= 8;		/* device px per grid unit (-scale N;     */
#define	XSC	xsc		/* print only -- pic/hpgl pin it back)    */
int	widef;			/* -wide: landscape raster (print)        */
int	nsheets;		/* the sheet SET on the command line      */

int	xorgx, xorgy;		/* device origin (print: the used extent) */

static
dx(gx)
{
	return gx * XSC - xorgx;
}

static
dy(gy)
{
	return gy * XSC - xorgy;
}

/* ---- small math (the GUI half lives in velgfx.c; the exporters keep
 * their own copies so a headless build never touches clgfx) ---- */

static long
xsqrt(v)
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

static short xsintab[10] = { 0, 44, 88, 128, 165, 196, 222, 241, 252, 256 };

static
xsin(a)
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
	return s * (xsintab[i] + (xsintab[i + 1 > 9 ? 9 : i + 1] -
		    xsintab[i]) * f / 10);
}

static
xcos(a)
{
	return xsin(a + 90);
}

/* symbol-space transform (vellum.c txq's twin), q -> device px */
static
xtxq(x, y, rot, mir, ox, oy, ppq, px, py)
int *px, *py;
{
	register int t;

	if ( mir )
		x = -x;
	switch ( rot & 3 )
	{
	case 1:	t = x;  x = -y;  y = t;  break;
	case 2:	x = -x;  y = -y;  break;
	case 3:	t = x;  x = y;  y = -t;  break;
	}
	*px = ox + x * ppq;
	*py = oy + y * ppq;
}

/* chorded arc through the backend's line */
static
xarc(xb, cx, cy, r, a0, a1)
XB *xb;
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
	x0 = cx + (r * xcos(a0) + 128) / 256;
	y0 = cy - (r * xsin(a0) + 128) / 256;
	for ( a = a0 + step; a < a1 + step; a += step )
	{
		if ( a > a1 )
			a = a1;
		x1 = cx + (r * xcos(a) + 128) / 256;
		y1 = cy - (r * xsin(a) + 128) / 256;
		(*xb->b_line)(x0, y0, x1, y1);
		x0 = x1;
		y0 = y1;
	}
	return 0;
}

/* the drum/doc shallow bulge, chorded */
static
xflat(xb, x0, x1, y, e)
XB *xb;
{
	register int x, xn;
	int w, ly, ny;
	long m;

	w = x1 - x0;
	if ( w <= 0 )
		return 0;
	ly = y;
	for ( x = x0; x < x1; x = xn )
	{
		xn = x + 4;
		if ( xn > x1 )
			xn = x1;
		m = (long)(2 * (xn - x0) - w);
		ny = (xn == x1) ? y
		   : y + e - (int)((long)e * m * m / ((long)w * w));
		(*xb->b_line)(x, ly, xn, ny);
		ly = ny;
	}
	return 0;
}

/* row-span fill of a shape kind (velgfx fillshape's device twin) */
static
xfill(xb, kind, x0, y0, x1, y1, val)
XB *xb;
{
	register int y, k;
	int w2, h2, cx, cy, r, e, s, xo, yb;

	if ( xb->b_span == 0 )
		return 0;
	w2 = (x1 - x0) / 2;
	h2 = (y1 - y0) / 2;
	cx = (x0 + x1) / 2;
	cy = (y0 + y1) / 2;
	switch ( kind )
	{
	case SH_BOX:
		for ( y = y0; y <= y1; y++ )
			(*xb->b_span)(x0, x1, y, val);
		break;
	case SH_RBOX:
		r = (w2 < h2 ? w2 : h2) / 2;
		if ( r > 12 ) r = 12;
		for ( y = y0; y <= y1; y++ )
		{
			k = (y < y0 + r) ? y0 + r - y :
			    (y > y1 - r) ? y - (y1 - r) : 0;
			xo = k ? r - (int)xsqrt((long)r * r - (long)k * k)
			       : 0;
			(*xb->b_span)(x0 + xo, x1 - xo, y, val);
		}
		break;
	case SH_DIAM:
		if ( h2 <= 0 )
			break;
		for ( y = y0; y <= y1; y++ )
		{
			k = y - cy;
			if ( k < 0 ) k = -k;
			xo = (h2 - k) * w2 / h2;
			(*xb->b_span)(cx - xo, cx + xo, y, val);
		}
		break;
	case SH_OVAL:
		r = w2 < h2 ? w2 : h2;
		for ( y = y0; y <= y1; y++ )
		{
			if ( w2 >= h2 )
			{
				k = y - cy;
				if ( k < 0 ) k = -k;
				if ( k > r )
					continue;
				xo = (int)xsqrt((long)r * r - (long)k * k);
				(*xb->b_span)(x0 + r - xo, x1 - r + xo, y,
					      val);
			}
			else
			{
				k = (y < y0 + r) ? y0 + r - y :
				    (y > y1 - r) ? y - (y1 - r) : 0;
				xo = k ? r - (int)xsqrt((long)r * r -
							(long)k * k) : 0;
				(*xb->b_span)(x0 + xo, x1 - xo, y, val);
			}
		}
		break;
	case SH_PAR:
		s = (x1 - x0) / 4;
		if ( s > y1 - y0 ) s = y1 - y0;
		if ( y1 <= y0 )
			break;
		for ( y = y0; y <= y1; y++ )
			(*xb->b_span)(x0 + s * (y1 - y) / (y1 - y0),
				      x1 - s * (y - y0) / (y1 - y0), y, val);
		break;
	case SH_DRUM:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		for ( y = y0; y <= y1; y++ )
		{
			if ( y < y0 + e )
				k = y0 + e - y;
			else if ( y > y1 - e )
				k = y - (y1 - e);
			else
				k = 0;
			if ( k >= e )
				continue;
			xo = k ? w2 * (int)xsqrt((long)e * e -
						 (long)k * k) / e : w2;
			(*xb->b_span)(cx - xo, cx + xo, y, val);
		}
		break;
	case SH_DOC:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		yb = y1 - e;
		for ( y = y0; y <= yb; y++ )
			(*xb->b_span)(x0, x1, y, val);
		break;
	case SH_CIRC:
		r = w2 < h2 ? w2 : h2;
		for ( y = cy - r; y <= cy + r; y++ )
		{
			k = y - cy;
			if ( k < 0 ) k = -k;
			xo = (int)xsqrt((long)r * r - (long)k * k);
			(*xb->b_span)(cx - xo, cx + xo, y, val);
		}
		break;
	case SH_ELL:
		if ( h2 <= 0 )
			break;
		for ( y = cy - h2; y <= cy + h2; y++ )
		{
			k = y - cy;
			if ( k < 0 ) k = -k;
			xo = (int)((long)w2 *
				   xsqrt((long)h2 * h2 - (long)k * k) / h2);
			(*xb->b_span)(cx - xo, cx + xo, y, val);
		}
		break;
	}
	return 0;
}

/* shape outline through the backend */
static
xshape(xb, kind, x0, y0, x1, y1)
XB *xb;
{
	int w2, h2, cx, cy, r, e, s, yb;

	w2 = (x1 - x0) / 2;
	h2 = (y1 - y0) / 2;
	cx = (x0 + x1) / 2;
	cy = (y0 + y1) / 2;
	switch ( kind )
	{
	case SH_BOX:
		(*xb->b_box)(x0, y0, x1, y1, -1);
		break;
	case SH_RBOX:
		r = (w2 < h2 ? w2 : h2) / 2;
		if ( r > 12 ) r = 12;
		(*xb->b_line)(x0 + r, y0, x1 - r, y0);
		(*xb->b_line)(x0 + r, y1, x1 - r, y1);
		(*xb->b_line)(x0, y0 + r, x0, y1 - r);
		(*xb->b_line)(x1, y0 + r, x1, y1 - r);
		xarc(xb, x0 + r, y0 + r, r, 90, 180);
		xarc(xb, x1 - r, y0 + r, r, 0, 90);
		xarc(xb, x0 + r, y1 - r, r, 180, 270);
		xarc(xb, x1 - r, y1 - r, r, 270, 360);
		break;
	case SH_DIAM:
		(*xb->b_line)(cx, y0, x1, cy);
		(*xb->b_line)(x1, cy, cx, y1);
		(*xb->b_line)(cx, y1, x0, cy);
		(*xb->b_line)(x0, cy, cx, y0);
		break;
	case SH_OVAL:
		r = w2 < h2 ? w2 : h2;
		if ( w2 >= h2 )
		{
			(*xb->b_line)(x0 + r, y0, x1 - r, y0);
			(*xb->b_line)(x0 + r, y1, x1 - r, y1);
			xarc(xb, x0 + r, cy, r, 90, 270);
			xarc(xb, x1 - r, cy, r, 270, 90);
		}
		else
		{
			(*xb->b_line)(x0, y0 + r, x0, y1 - r);
			(*xb->b_line)(x1, y0 + r, x1, y1 - r);
			xarc(xb, cx, y0 + r, r, 0, 180);
			xarc(xb, cx, y1 - r, r, 180, 360);
		}
		break;
	case SH_PAR:
		s = (x1 - x0) / 4;
		if ( s > y1 - y0 ) s = y1 - y0;
		(*xb->b_line)(x0 + s, y0, x1, y0);
		(*xb->b_line)(x1, y0, x1 - s, y1);
		(*xb->b_line)(x1 - s, y1, x0, y1);
		(*xb->b_line)(x0, y1, x0 + s, y0);
		break;
	case SH_DRUM:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		xflat(xb, x0, x1, y0 + e, -e);
		xflat(xb, x0, x1, y0 + e, e);
		(*xb->b_line)(x0, y0 + e, x0, y1 - e);
		(*xb->b_line)(x1, y0 + e, x1, y1 - e);
		xflat(xb, x0, x1, y1 - e, e);
		break;
	case SH_DOC:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		yb = y1 - e;
		(*xb->b_line)(x0, y0, x1, y0);
		(*xb->b_line)(x0, y0, x0, yb);
		(*xb->b_line)(x1, y0, x1, yb);
		xflat(xb, x0, cx, yb, e);
		xflat(xb, cx, x1, yb, -e);
		break;
	case SH_CIRC:
		(*xb->b_circle)(cx, cy, w2 < h2 ? w2 : h2, -1);
		break;
	case SH_ELL:
		{
			register int a, step;
			int lx, ly, nx, ny;

			r = w2 > h2 ? w2 : h2;
			if ( r <= 0 )
				break;
			step = 200 / r + 3;
			if ( step > 30 )
				step = 30;
			lx = cx + w2;
			ly = cy;
			for ( a = step; a - step < 360; a += step )
			{
				if ( a > 360 )
					a = 360;
				nx = cx + (w2 * xcos(a) + 128) / 256;
				ny = cy - (h2 * xsin(a) + 128) / 256;
				(*xb->b_line)(lx, ly, nx, ny);
				lx = nx;
				ly = ny;
			}
		}
		break;
	}
	return 0;
}

/* arrowhead barbs at (bx,by), coming from direction (dxv,dyv) */
static
xarrow(xb, bx, by, dxv, dyv)
XB *xb;
{
	long len;
	int hx, hy, px, py;

	len = xsqrt((long)dxv * dxv + (long)dyv * dyv);
	if ( len == 0 )
		return 0;
	hx = (int)((long)dxv * 7 / len);
	hy = (int)((long)dyv * 7 / len);
	px = (int)(-(long)dyv * 3 / len);
	py = (int)((long)dxv * 3 / len);
	(*xb->b_line)(bx, by, bx - hx + px, by - hy + py);
	(*xb->b_line)(bx, by, bx - hx - px, by - hy - py);
	return 0;
}

/* the chord emitter for smooth polylines on backends without a native
 * curve (print, hpgl): bspline calls xseg per chord */
static XB	*xbcur;

static
xseg(x0, y0, x1, y1)
{
	(*xbcur->b_line)(x0, y0, x1, y1);
	return 0;
}

/* dimension: extension ticks + the line + arrowheads both ends + label */
static
xdim(xb, o)
XB *xb;
register DOBJ *o;
{
	char db[24];
	int x0, y0, x1, y1, ax, ay, mx, my, lw;

	x0 = dx((int)o->o_x);   y0 = dy((int)o->o_y);
	x1 = dx((int)o->o_x2);  y1 = dy((int)o->o_y2);
	ax = x1 - x0;	if ( ax < 0 ) ax = -ax;
	ay = y1 - y0;	if ( ay < 0 ) ay = -ay;
	if ( ax >= ay )
	{
		(*xb->b_line)(x0, y0 - 4, x0, y0 + 4);
		(*xb->b_line)(x1, y1 - 4, x1, y1 + 4);
	}
	else
	{
		(*xb->b_line)(x0 - 4, y0, x0 + 4, y0);
		(*xb->b_line)(x1 - 4, y1, x1 + 4, y1);
	}
	(*xb->b_line)(x0, y0, x1, y1);
	xarrow(xb, x0, y0, x0 - x1, y0 - y1);
	xarrow(xb, x1, y1, x1 - x0, y1 - y0);
	dimlbl(o, db);
	mx = (x0 + x1) / 2;
	my = (y0 + y1) / 2;
	lw = strlen(db) * 6;
	if ( ax >= ay )
		(*xb->b_text)(mx - lw / 2, my - 12, 0, db);
	else
		(*xb->b_text)(mx + 5, my - 4, 0, db);
	return 0;
}

/* multi-line text: split o_val on '|', b_text per line */
static
xtext(xb, x, y, sz, s)
XB *xb;
char *s;
{
	char lb[VALL];
	register char *e;
	register int n;
	int ch;

	ch = sz == 0 ? 8 : sz == 1 ? 15 : 16;
	for (;;)
	{
		for ( e = s, n = 0; *e && *e != '|'; e++ )
			lb[n++] = *e;
		lb[n] = 0;
		(*xb->b_text)(x, y, sz, lb);
		if ( *e == 0 )
			break;
		s = e + 1;
		y += ch;
	}
	return 0;
}

/* Is a layer printable? */
static
xprn(l)
{
	return l >= 0 && l < NLAYER && layprn[l] && l != 3;
}

/* ---- THE WALKER: every printable object through the backend ---- */
static
xwalk(xb)
register XB *xb;
{
	register DOBJ *o;
	register int i;
	int x0, y0, x1, y1, t, k, fl, fill;
	long r;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( !xprn((int)o->o_layer) )
			continue;
		fl = o->o_flags;
		t = fl & OF_FILL;
		fill = (t == 0) ? -1 : (t == OF_FILLW) ? 1 :
		       (t == OF_FILLG) ? 2 : 0;
		if ( fill == 2 && (fl & OF_HATCH) )
			fill = 3;	/* gray modified to 45-deg hatch */
		(*xb->b_style)(fl & (OF_STYLE | OF_BOLD));
		switch ( o->o_type )
		{
		case OT_WIRE:
			x0 = dx(o->o_x);   y0 = dy(o->o_y);
			x1 = dx(o->o_x2);  y1 = dy(o->o_y2);
			if ( y0 == y1 || x0 == x1 )
				(*xb->b_line)(x0, y0, x1, y1);
			else
			{
				(*xb->b_line)(x0, y0, x1, y0);
				(*xb->b_line)(x1, y0, x1, y1);
			}
			break;

		case OT_LINE:
			(*xb->b_line)(dx(o->o_x), dy(o->o_y),
				      dx(o->o_x2), dy(o->o_y2));
			break;

		case OT_BOX:
			x0 = dx(o->o_x);   y0 = dy(o->o_y);
			x1 = dx(o->o_x2);  y1 = dy(o->o_y2);
			if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
			if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
			(*xb->b_box)(x0, y0, x1, y1, fill);
			break;

		case OT_CIRC:
			r = xsqrt((long)(o->o_x2 - o->o_x) * XSC *
				  (o->o_x2 - o->o_x) * XSC +
				  (long)(o->o_y2 - o->o_y) * XSC *
				  (o->o_y2 - o->o_y) * XSC);
			(*xb->b_circle)(dx(o->o_x), dy(o->o_y), (int)r,
					fill);
			break;

		case OT_ARC:
			xarc(xb, dx(o->o_x), dy(o->o_y), o->o_x2 * XSC,
			     (int)OA0(o), (int)OA1(o));
			break;

		case OT_TEXT:
			if ( (fl & OF_VERT) && xb->b_vtext )
				(*xb->b_vtext)(dx(o->o_x), dy(o->o_y),
					       (int)o->o_rot, o->o_val);
			else
				xtext(xb, dx(o->o_x), dy(o->o_y),
				      (int)o->o_rot, o->o_val);
			break;

		case OT_NNAME:
			(*xb->b_box)(dx(o->o_x) - 1, dy(o->o_y) - 1,
				     dx(o->o_x) + 1, dy(o->o_y) + 1, 0);
			(*xb->b_text)(dx(o->o_x) + 3, dy(o->o_y) - 9, 0,
				      o->o_name);
			break;

		case OT_DIM:
			(*xb->b_style)(0);
			xdim(xb, o);
			break;

		case OT_POLY:
			if ( (fl & OF_SMOOTH) && o->o_sym >= 3 )
			{
				int pxy[2 * PMAXPT];

				for ( k = 0; k < o->o_sym; k++ )
				{
					pxy[2*k] = dx(ppool[o->o_x2 + 2*k]);
					pxy[2*k + 1] =
					    dy(ppool[o->o_x2 + 2*k + 1]);
				}
				if ( xb->b_poly )
					(*xb->b_poly)(pxy, (int)o->o_sym);
				else
				{
					xbcur = xb;
					bspline(pxy, (int)o->o_sym, xseg);
				}
				break;
			}
			for ( k = 1; k < o->o_sym; k++ )
				(*xb->b_line)(
				    dx(ppool[o->o_x2 + 2*k - 2]),
				    dy(ppool[o->o_x2 + 2*k - 1]),
				    dx(ppool[o->o_x2 + 2*k]),
				    dy(ppool[o->o_x2 + 2*k + 1]));
			break;

		case OT_SHAPE:
			x0 = dx(o->o_x);   y0 = dy(o->o_y);
			x1 = dx(o->o_x2);  y1 = dy(o->o_y2);
			if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
			if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
			if ( fill >= 0 &&
			     (o->o_sym == SH_BOX || o->o_sym == SH_CIRC) )
				;	/* b_box/b_circle carry the fill */
			else if ( fill >= 0 )
				xfill(xb, (int)o->o_sym, x0, y0, x1, y1,
				      fill);
			if ( o->o_sym == SH_BOX )
				(*xb->b_box)(x0, y0, x1, y1, fill);
			else if ( o->o_sym == SH_CIRC )
				(*xb->b_circle)((x0 + x1) / 2,
					(y0 + y1) / 2,
					((x1 - x0) < (y1 - y0) ?
					 (x1 - x0) : (y1 - y0)) / 2, fill);
			else
				xshape(xb, (int)o->o_sym, x0, y0, x1, y1);
			if ( o->o_val[0] && y1 - y0 >= 18 )
			{
				char tb[VALL];

				k = (x1 - x0 - 4) / 9;
				for ( t = 0; o->o_val[t] && t < k &&
					     t < VALL - 1; t++ )
					tb[t] = o->o_val[t];
				tb[t] = 0;
				(*xb->b_text)((x0 + x1 - t * 9) / 2 + 1,
					      (y0 + y1 - 16) / 2 + 1, 2, tb);
			}
			break;

		case OT_CONN:
			x0 = dx(o->o_x);   y0 = dy(o->o_y);
			x1 = dx(o->o_x2);  y1 = dy(o->o_y2);
			if ( o->o_sym == CS_HV || o->o_sym == CS_HARROW )
			{
				if ( y0 == y1 || x0 == x1 )
					(*xb->b_line)(x0, y0, x1, y1);
				else
				{
					(*xb->b_line)(x0, y0, x1, y0);
					(*xb->b_line)(x1, y0, x1, y1);
				}
			}
			else
				(*xb->b_line)(x0, y0, x1, y1);
			(*xb->b_style)(0);
			if ( o->o_sym == CS_ARROW )
				xarrow(xb, x1, y1, x1 - x0, y1 - y0);
			else if ( o->o_sym == CS_HARROW )
			{
				if ( y1 != y0 )
					xarrow(xb, x1, y1, 0, y1 - y0);
				else
					xarrow(xb, x1, y1, x1 - x0, 0);
			}
			break;

		case OT_SYM:
			{
				register short *p;
				int ox, oy, a0, a1;
				char tb[2];

				ox = dx(o->o_x);
				oy = dy(o->o_y);
				(*xb->b_style)(0);
				p = symtab[o->o_sym].sy_ops;
				while ( *p != SEND )
				{
					if ( *p == SE )
					{
						xtxq(p[1], p[2],
						     (int)o->o_rot,
						     (int)o->o_mir, ox, oy,
						     XSC / 4, &x0, &y0);
						xtxq(p[3], p[4],
						     (int)o->o_rot,
						     (int)o->o_mir, ox, oy,
						     XSC / 4, &x1, &y1);
						(*xb->b_line)(x0, y0,
							      x1, y1);
						p += 5;
					}
					else if ( *p == SC )
					{
						xtxq(p[1], p[2],
						     (int)o->o_rot,
						     (int)o->o_mir, ox, oy,
						     XSC / 4, &x0, &y0);
						(*xb->b_circle)(x0, y0,
						    p[3] * (XSC / 4), -1);
						p += 4;
					}
					else if ( *p == SA )
					{
						xtxq(p[1], p[2],
						     (int)o->o_rot,
						     (int)o->o_mir, ox, oy,
						     XSC / 4, &x0, &y0);
						a0 = p[4];
						a1 = p[5];
						if ( o->o_mir )
						{
							t = a0;
							a0 = 180 - a1;
							a1 = 180 - t;
						}
						a0 -= 90 * (o->o_rot & 3);
						a1 -= 90 * (o->o_rot & 3);
						xarc(xb, x0, y0,
						     p[3] * (XSC / 4),
						     a0, a1);
						p += 6;
					}
					else
					{
						xtxq(p[1], p[2],
						     (int)o->o_rot,
						     (int)o->o_mir, ox, oy,
						     XSC / 4, &x0, &y0);
						tb[0] = p[3];
						tb[1] = 0;
						(*xb->b_text)(x0, y0, 0, tb);
						p += 4;
					}
				}
				/* designator + value beside the body */
				symdbox(i, &x0, &y0, &x1, &y1);
				if ( o->o_rot & 1 )
				{
					if ( o->o_name[0] )
						(*xb->b_text)(x1 + 3,
						    y0 + 2, 0, o->o_name);
					if ( o->o_val[0] )
						(*xb->b_text)(x1 + 3,
						    y0 + 11, 0, o->o_val);
				}
				else
				{
					if ( o->o_name[0] )
						(*xb->b_text)(x0, y0 - 9,
						    0, o->o_name);
					if ( o->o_val[0] )
						(*xb->b_text)(x0, y1 + 2,
						    0, o->o_val);
				}
			}
			break;
		}
	}
	(*xb->b_style)(0);
	return 0;
}

/* device bbox of symbol i's body (for its labels) */
static
symdbox(i, bx0, by0, bx1, by1)
int *bx0, *by0, *bx1, *by1;
{
	register DOBJ *o;
	register SYMDEF *s;
	int cx[4], cy[4], k, ox, oy, x, y;

	o = &obj[i];
	s = &symtab[o->o_sym];
	ox = dx(o->o_x);
	oy = dy(o->o_y);
	xtxq(s->sy_x0, s->sy_y0, (int)o->o_rot, (int)o->o_mir, ox, oy,
	     XSC / 4, &cx[0], &cy[0]);
	xtxq(s->sy_x1, s->sy_y0, (int)o->o_rot, (int)o->o_mir, ox, oy,
	     XSC / 4, &cx[1], &cy[1]);
	xtxq(s->sy_x0, s->sy_y1, (int)o->o_rot, (int)o->o_mir, ox, oy,
	     XSC / 4, &cx[2], &cy[2]);
	xtxq(s->sy_x1, s->sy_y1, (int)o->o_rot, (int)o->o_mir, ox, oy,
	     XSC / 4, &cx[3], &cy[3]);
	*bx0 = *bx1 = cx[0];
	*by0 = *by1 = cy[0];
	for ( k = 1; k < 4; k++ )
	{
		x = cx[k];
		y = cy[k];
		if ( x < *bx0 ) *bx0 = x;
		if ( x > *bx1 ) *bx1 = x;
		if ( y < *by0 ) *by0 = y;
		if ( y > *by1 ) *by1 = y;
	}
	return 0;
}

/* grid extent of the printable drawing; 0 when empty */
static
xextent(gx0, gy0, gx1, gy1)
int *gx0, *gy0, *gx1, *gy1;
{
	register int i;
	int x0, y0, x1, y1, got;

	got = 0;
	for ( i = 0; i < nobj; i++ )
	{
		if ( !xprn((int)obj[i].o_layer) )
			continue;
		objgbox(i, &x0, &y0, &x1, &y1);
		if ( !got )
		{
			*gx0 = x0;  *gy0 = y0;  *gx1 = x1;  *gy1 = y1;
			got = 1;
		}
		else
		{
			if ( x0 < *gx0 ) *gx0 = x0;
			if ( y0 < *gy0 ) *gy0 = y0;
			if ( x1 > *gx1 ) *gx1 = x1;
			if ( y1 > *gy1 ) *gy1 = y1;
		}
	}
	return got;
}

/* ================================================================== */
/* -print: raster into 24-row bands, ESC * 39 to stdout               */
/* ================================================================== */

#define	BANDH	24		/* one 24-pin head pass                   */
#define	MAXDOTS	960		/* printable width, dots                  */

char	*band;			/* BANDH rows x bandwb bytes              */
int	bandw, bandwb;		/* OUTPUT width dots / bytes per row      */
int	bandy;			/* output y of the band's first row       */
int	devw;			/* drawing width, dots (-wide transform)  */
int	prsty;			/* current OF_STYLE|OF_BOLD               */
int	prrun;			/* rotating dash mask                     */

/* the three .hf fonts, loaded from disk (no VRAM here) */
struct hf {
	short	first, nch, cellw, cellh;
	unsigned short	*bits;
} hfont[3];

static
loadhf(k, path)
char *path;
{
	register FILE *fp;
	short hd[4];
	int n;

	hfont[k].bits = 0;
	if ( (fp = fopen(path, "r")) == (FILE *)0 )
		return -1;
	if ( fread((char *)hd, sizeof(short), 4, fp) != 4 )
	{
		fclose(fp);
		return -1;
	}
	hfont[k].first = hd[0];
	hfont[k].nch = hd[1];
	hfont[k].cellw = hd[2];
	hfont[k].cellh = hd[3];
	n = hd[1] * hd[3];
	hfont[k].bits = (unsigned short *)malloc(n * sizeof(short));
	if ( hfont[k].bits == 0 ||
	     fread((char *)hfont[k].bits, sizeof(short), n, fp) != n )
	{
		hfont[k].bits = 0;
		fclose(fp);
		return -1;
	}
	fclose(fp);
	return 0;
}

/* Set one DRAWING-space pixel in the output band; -wide rotates the
 * page a quarter turn (out x = y, out y = devw-1-x), so landscape costs
 * a coordinate swap here and nothing anywhere else. */
static
bset(x, y)
{
	register int t;

	if ( widef )
	{
		t = x;
		x = y;
		y = devw - 1 - t;
	}
	if ( x < 0 || x >= bandw || y < bandy || y >= bandy + BANDH )
		return 0;
	band[(y - bandy) * bandwb + (x >> 3)] |= 0x80 >> (x & 7);
	return 0;
}

/* ... and its eraser (white spans, hatch grounds). */
static
bclr(x, y)
{
	register int t;

	if ( widef )
	{
		t = x;
		x = y;
		y = devw - 1 - t;
	}
	if ( x < 0 || x >= bandw || y < bandy || y >= bandy + BANDH )
		return 0;
	band[(y - bandy) * bandwb + (x >> 3)] &= ~(0x80 >> (x & 7));
	return 0;
}

static
bsetb(x, y)		/* honours bold: doubles the pixel */
{
	bset(x, y);
	if ( prsty & OF_BOLD )
		bset(x + 1, y);
	return 0;
}

static
pb_span(x0, x1, y, val)
{
	register int x;

	if ( !widef && (y < bandy || y >= bandy + BANDH) )
		return 0;		/* -wide: a span crosses bands */
	for ( x = x0; x <= x1; x++ )
		switch ( val )
		{
		case 1:			/* white: clear the run */
			bclr(x, y);
			break;
		case 0:			/* black */
			bset(x, y);
			break;
		case 3:			/* hatch: white + 45-deg stripes */
			if ( ((x + y) & 7) == 0 )
				bset(x, y);
			else
				bclr(x, y);
			break;
		default:		/* gray: the anchored checker */
			if ( ((x ^ y) & 1) == 0 )
				bset(x, y);
			break;
		}
	return 0;
}

static
pb_line(x0, y0, x1, y1)
{
	int dxv, dyv, sx, sy, err, e2;

	/* quick band reject (portrait only: -wide swaps axes in bset) */
	if ( !widef &&
	     ((y0 < bandy && y1 < bandy) ||
	      (y0 >= bandy + BANDH && y1 >= bandy + BANDH)) )
		return 0;
	dxv = x1 - x0;  if ( dxv < 0 ) dxv = -dxv;
	dyv = y1 - y0;  if ( dyv < 0 ) dyv = -dyv;
	sx = x0 < x1 ? 1 : -1;
	sy = y0 < y1 ? 1 : -1;
	err = dxv - dyv;
	prrun = ((prsty & OF_STYLE) == OF_DASH) ? 0xf0f0 :
		((prsty & OF_STYLE) == OF_DOT) ? 0xaaaa : 0xffff;
	for (;;)
	{
		if ( prrun & 0x8000 )
			bsetb(x0, y0);
		prrun = ((prrun << 1) | ((prrun >> 15) & 1)) & 0xffff;
		if ( x0 == x1 && y0 == y1 )
			break;
		e2 = err + err;
		if ( e2 > -dyv ) { err -= dyv;  x0 += sx; }
		if ( e2 <  dxv ) { err += dxv;  y0 += sy; }
	}
	return 0;
}

static
pb_box(x0, y0, x1, y1, fill)
{
	register int y;

	if ( fill >= 0 )
		for ( y = y0; y <= y1; y++ )
			pb_span(x0, x1, y, fill);
	pb_line(x0, y0, x1, y0);
	pb_line(x1, y0, x1, y1);
	pb_line(x1, y1, x0, y1);
	pb_line(x0, y1, x0, y0);
	return 0;
}

static
pb_circle(cx, cy, r, fill)
{
	register int y, k;
	int xo;

	if ( r <= 0 )
	{
		bset(cx, cy);
		return 0;
	}
	if ( fill >= 0 )
		for ( y = cy - r; y <= cy + r; y++ )
		{
			k = y - cy;
			if ( k < 0 ) k = -k;
			xo = (int)xsqrt((long)r * r - (long)k * k);
			pb_span(cx - xo, cx + xo, y, fill);
		}
	/* midpoint circle */
	{
		register int x2, y2;
		int d;

		x2 = 0;
		y2 = r;
		d = 1 - r;
		while ( x2 <= y2 )
		{
			bset(cx + x2, cy + y2);  bset(cx - x2, cy + y2);
			bset(cx + x2, cy - y2);  bset(cx - x2, cy - y2);
			bset(cx + y2, cy + x2);  bset(cx - y2, cy + x2);
			bset(cx + y2, cy - x2);  bset(cx - y2, cy - x2);
			if ( d < 0 )
				d += 2 * x2 + 3;
			else
			{
				d += 2 * (x2 - y2) + 5;
				y2--;
			}
			x2++;
		}
	}
	return 0;
}

static
pb_text(x, y, sz, s)
char *s;
{
	register struct hf *f;
	register int i, row, col;
	unsigned short w;
	int gi;

	f = &hfont[sz <= 0 ? 0 : sz == 1 ? 1 : 2];
	if ( f->bits == 0 )
		return 0;
	if ( !widef && (y + f->cellh <= bandy || y >= bandy + BANDH) )
		return 0;
	for ( i = 0; s[i]; i++ )
	{
		gi = (s[i] & 0xff) - f->first;
		if ( gi < 0 || gi >= f->nch )
			continue;
		for ( row = 0; row < f->cellh; row++ )
		{
			w = f->bits[gi * f->cellh + row];
			for ( col = 0; col < f->cellw; col++ )
				if ( w & (0x8000 >> col) )
					bset(x + i * f->cellw + col,
					     y + row);
		}
	}
	return 0;
}

static
pb_style(fl)
{
	prsty = fl;
	return 0;
}

/* vertical text: the same glyph transpose the editor draws (velgfx
 * vtext) -- a quarter turn clockwise, '|' one cell to the right */
static
pb_vtext(x, y, sz, s)
char *s;
{
	register struct hf *f;
	register int row, col;
	unsigned short w;
	int gy, gi;

	f = &hfont[sz <= 0 ? 0 : sz == 1 ? 1 : 2];
	if ( f->bits == 0 )
		return 0;
	gy = y;
	for ( ; *s; s++ )
	{
		if ( *s == '|' )
		{
			x += f->cellh;
			gy = y;
			continue;
		}
		gi = (*s & 0xff) - f->first;
		if ( gi >= 0 && gi < f->nch )
			for ( row = 0; row < f->cellh; row++ )
			{
				w = f->bits[gi * f->cellh + row];
				for ( col = 0; col < f->cellw; col++ )
					if ( w & (0x8000 >> col) )
						bset(x + f->cellh - 1 - row,
						     gy + col);
			}
		gy += f->cellw;
	}
	return 0;
}

XB	printxb = { pb_line, pb_box, pb_circle, pb_text, pb_span, pb_style,
		    pb_vtext, (int (*)())0 };

static
doprint()
{
	register int b, x, r;
	int gx0, gy0, gx1, gy1, hdots, nb, devh;

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "vellum: nothing to print\n");
		return 1;
	}
	xorgx = (gx0 - 1) * XSC;
	xorgy = (gy0 - 1) * XSC;
	devw = (gx1 - gx0 + 2) * XSC;
	devh = (gy1 - gy0 + 2) * XSC;
	bandw = widef ? devh : devw;	/* -wide: x/y exchanged */
	if ( bandw > MAXDOTS )
		bandw = MAXDOTS;
	bandwb = (bandw + 7) / 8;
	hdots = widef ? devw : devh;
	if ( band == 0 )		/* once: a SET prints many sheets */
		band = malloc(BANDH * ((MAXDOTS + 7) / 8));
	if ( band == 0 )
		return 1;
	if ( hfont[0].bits == 0 )
	{
		loadhf(0, "/usr/hr/fonts/sail.hf");
		loadhf(1, "/usr/hr/fonts/gacha.r.hf");
		loadhf(2, "/usr/hr/fonts/gacha.b.hf");
	}
	printf("\033@");		/* reset */
	nb = (hdots + BANDH - 1) / BANDH;
	for ( b = 0; b < nb; b++ )
	{
		bandy = b * BANDH;
		for ( x = 0; x < BANDH * bandwb; x++ )
			band[x] = 0;
		xwalk(&printxb);
		/* ESC * 39: 24-pin triple-density, 3 bytes per column */
		printf("\033*%c%c%c", 39, bandw & 0xff,
		       (bandw >> 8) & 0xff);
		for ( x = 0; x < bandw; x++ )
			for ( r = 0; r < 3; r++ )
			{
				register int bit, byte;
				register int rr;

				byte = 0;
				for ( bit = 0; bit < 8; bit++ )
				{
					rr = r * 8 + bit;
					if ( band[rr * bandwb + (x >> 3)] &
					     (0x80 >> (x & 7)) )
						byte |= 0x80 >> bit;
				}
				putchar(byte);
			}
		printf("\r\033J%c", BANDH);	/* advance 24/180 inch */
	}
	putchar('\f');			/* eject */
	printf("\033@");
	return 0;
}

/* ================================================================== */
/* -pic: troff pic source (device px -> inches at 16 units/inch)      */
/* ================================================================== */

int	picsty;
int	picymax;		/* device y of the sheet bottom (flip)    */

/* device px -> hundredths of an inch (128 dots/inch) */
static
in100(v)
{
	return (int)((long)v * 25 / 32);
}

static char *
picxy(x, y, buf)
char *buf;
{
	int a, b;

	a = in100(x);
	b = in100(picymax - y);
	sprintf(buf, "%d.%02d,%d.%02d", a / 100, a % 100 < 0 ? 0 : a % 100,
		b / 100, b % 100 < 0 ? 0 : b % 100);
	return buf;
}

static char *
picdim(v, buf)
char *buf;
{
	int a;

	a = in100(v);
	sprintf(buf, "%d.%02d", a / 100, a % 100);
	return buf;
}

static char *
picdash()
{
	if ( (picsty & OF_STYLE) == OF_DASH )
		return " dashed";
	if ( (picsty & OF_STYLE) == OF_DOT )
		return " dotted";
	return "";
}

static
cb_line(x0, y0, x1, y1)
{
	char a[24], b[24];

	printf("line%s from %s to %s\n", picdash(),
	       picxy(x0, y0, a), picxy(x1, y1, b));
	return 0;
}

static
cb_box(x0, y0, x1, y1, fill)
{
	char a[24], w[16], h[16];

	printf("box%s wid %s ht %s with .nw at %s", picdash(),
	       picdim(x1 - x0, w), picdim(y1 - y0, h), picxy(x0, y0, a));
	if ( fill == 0 )
		printf(" fill 1");
	else if ( fill == 2 || fill == 3 )
		printf(" fill 0.5");
	printf("\n");
	return 0;
}

static
cb_circle(cx, cy, r, fill)
{
	char a[24], d[16];

	printf("circle%s rad %s at %s", picdash(), picdim(r, d),
	       picxy(cx, cy, a));
	if ( fill == 0 )
		printf(" fill 1");
	else if ( fill == 2 || fill == 3 )
		printf(" fill 0.5");
	printf("\n");
	return 0;
}

static
cb_text(x, y, sz, s)
char *s;
{
	char a[24];
	register int i;
	int ch;

	ch = sz == 0 ? 8 : sz == 1 ? 15 : 16;
	printf("\"");
	for ( i = 0; s[i]; i++ )
	{
		if ( s[i] == '"' )
			printf("\\(dq");	/* a quote inside a pic string */
		else
			putchar(s[i]);
	}
	printf("\" ljust at %s\n", picxy(x, y + ch / 2, a));
	return 0;
}

static
cb_style(fl)
{
	picsty = fl;
	return 0;
}

/* smooth polylines land as pic's own curve */
static
cb_poly(xy, n)
int *xy;
{
	char a[24];
	register int k;

	printf("spline%s from %s", picdash(), picxy(xy[0], xy[1], a));
	for ( k = 1; k < n; k++ )
		printf(" to %s", picxy(xy[2*k], xy[2*k + 1], a));
	printf("\n");
	return 0;
}

XB	picxb = { cb_line, cb_box, cb_circle, cb_text, (int (*)())0,
		  cb_style, (int (*)())0, cb_poly };

static
dopic()
{
	xorgx = 0;
	xorgy = 0;
	picymax = SHH * XSC;
	printf(".PS\n");
	xwalk(&picxb);
	printf(".PE\n");
	return 0;
}

/* ================================================================== */
/* -hpgl: the third walker backend (pen plotters).  Device px x 4 =    */
/* plotter units (~1016/inch against the printer's ~254/inch), y       */
/* flipped -- HPGL's origin is bottom-left.                            */
/* ================================================================== */

int	hpsty;
int	hymax;			/* device y of the sheet bottom (flip)    */

static
hp_xy(x, y, buf)
char *buf;
{
	sprintf(buf, "%d,%d", x * 4, (hymax - y) * 4);
	return 0;
}

static
hb_style(fl)
{
	register int s;

	s = fl & OF_STYLE;
	if ( s != (hpsty & OF_STYLE) )
	{
		if ( s == OF_DASH )
			printf("LT2;");
		else if ( s == OF_DOT )
			printf("LT1;");
		else
			printf("LT;");
	}
	hpsty = fl;
	return 0;
}

static
hb_line(x0, y0, x1, y1)
{
	char a[16], b[16];

	hp_xy(x0, y0, a);
	hp_xy(x1, y1, b);
	printf("PU%s;PD%s;\n", a, b);
	return 0;
}

static
hb_box(x0, y0, x1, y1, fill)
{
	char a[16], b[16];

	hp_xy(x0, y0, a);
	hp_xy(x1, y1, b);
	printf("PU%s;EA%s;\n", a, b);	/* edge rectangle */
	return 0;
}

static
hb_circle(cx, cy, r, fill)
{
	char a[16];

	hp_xy(cx, cy, a);
	printf("PU%s;CI%d;\n", a, r * 4);
	return 0;
}

static
hb_text(x, y, sz, s)
char *s;
{
	char a[16];

	hp_xy(x, y + (sz <= 0 ? 8 : sz == 1 ? 15 : 16), a);
	printf("PU%s;LB%s\003;\n", a, s);
	return 0;
}

static
hb_vtext(x, y, sz, s)
char *s;
{
	char a[16];

	hp_xy(x, y, a);				/* label runs DOWN the page */
	printf("PU%s;DR0,-1;LB%s\003;DR1,0;\n", a, s);
	return 0;
}

XB	hpglxb = { hb_line, hb_box, hb_circle, hb_text, (int (*)())0,
		   hb_style, hb_vtext, (int (*)())0 };

static
dohpgl()
{
	int gx0, gy0, gx1, gy1;

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "vellum: nothing to plot\n");
		return 1;
	}
	xorgx = (gx0 - 1) * XSC;
	xorgy = (gy0 - 1) * XSC;
	hymax = (gy1 - gy0 + 2) * XSC;
	hpsty = 0;
	printf("IN;SP1;LT;\n");
	xwalk(&hpglxb);
	printf("PU0,0;SP0;\n");
	return 0;
}

/* ================================================================== */
/* -bom: bill of materials -- designator, value, count, sorted; one    */
/* text walker over the Y objects, ACCUMULATED over the whole sheet    */
/* set so a design gets ONE parts list (secs. 18-19).                  */
/* ================================================================== */

#define	MAXBOM	96
char	bomcode[MAXBOM][8];	/* stencil code                           */
char	bomval[MAXBOM][VALL];	/* value ("" groups separately)           */
char	bomref[MAXBOM][60];	/* the designators, space separated       */
short	bomcnt[MAXBOM];
int	nbom;

/* fold this (loaded) sheet's designated symbols into the table */
static
bomsheet()
{
	register DOBJ *o;
	register int i, k;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
			continue;
		for ( k = 0; k < nbom; k++ )
			if ( strcmp(bomcode[k],
				    symtab[o->o_sym].sy_code) == 0 &&
			     strcmp(bomval[k], o->o_val) == 0 )
				break;
		if ( k == nbom )
		{
			if ( nbom >= MAXBOM )
				continue;
			strcpy(bomcode[k], symtab[o->o_sym].sy_code);
			strcpy(bomval[k], o->o_val);
			bomref[k][0] = 0;
			bomcnt[k] = 0;
			nbom++;
		}
		bomcnt[k]++;
		if ( strlen(bomref[k]) + strlen(o->o_name) + 2 <
		     sizeof(bomref[0]) )
		{
			if ( bomref[k][0] )
				strcat(bomref[k], " ");
			strcat(bomref[k], o->o_name);
		}
	}
	return 0;
}

/* sort (code, then value) and print */
static
dobomout()
{
	register int i, j;
	short ix[MAXBOM];
	int t, c;

	for ( i = 0; i < nbom; i++ )
		ix[i] = i;
	for ( i = 1; i < nbom; i++ )		/* insertion sort */
		for ( j = i; j > 0; j-- )
		{
			c = strcmp(bomcode[ix[j - 1]], bomcode[ix[j]]);
			if ( c == 0 )
				c = strcmp(bomval[ix[j - 1]], bomval[ix[j]]);
			if ( c <= 0 )
				break;
			t = ix[j];  ix[j] = ix[j - 1];  ix[j - 1] = t;
		}
	for ( i = 0; i < nbom; i++ )
	{
		j = ix[i];
		printf("%3d  %-7s %-15s %s\n", bomcnt[j], bomcode[j],
		       bomval[j][0] ? bomval[j] : "-", bomref[j]);
	}
	return 0;
}

/* ================================================================== */
/* -net: union-find over wire endpoints, junctions and pins           */
/* ================================================================== */

short	wnet[MAXOBJ];		/* wire object -> net root (union-find)   */

static
nfind(i)
{
	while ( wnet[i] != i )
		i = wnet[i] = wnet[wnet[i]];
	return i;
}

static
nunion(a, b)
{
	a = nfind(a);
	b = nfind(b);
	if ( a != b )
		wnet[a] = b;
	return 0;
}

/* Is grid point (px,py) ON wire o (either leg)? */
static
xonwire(o, px, py)
DOBJ *o;
{
	register int a, b;

	if ( o->o_x != o->o_x2 && py == o->o_y )
	{
		a = o->o_x;  b = o->o_x2;
		if ( a > b ) { a = o->o_x2;  b = o->o_x; }
		if ( px >= a && px <= b )
			return 1;
	}
	if ( o->o_y != o->o_y2 && px == o->o_x2 )
	{
		a = o->o_y;  b = o->o_y2;
		if ( a > b ) { a = o->o_y2;  b = o->o_y; }
		if ( py >= a && py <= b )
			return 1;
	}
	if ( px == o->o_x && py == o->o_y )
		return 1;
	if ( px == o->o_x2 && py == o->o_y2 )
		return 1;
	return 0;
}

/* device-independent pin position of pin k of symbol object i */
static
xpinpos(i, k, gx, gy)
int *gx, *gy;
{
	register DOBJ *o;
	register short *pp;
	int qx, qy;

	o = &obj[i];
	pp = symtab[o->o_sym].sy_pins;
	xtxq(pp[1 + 2*k], pp[2 + 2*k], (int)o->o_rot, (int)o->o_mir,
	     0, 0, 1, &qx, &qy);
	*gx = o->o_x + qx / 4;
	*gy = o->o_y + qy / 4;
	return 0;
}

#define	MAXNPIN	256

/* Named nets MERGE across the sheet set (sec. 19): a net named VBUS on
 * sheet 1 is the same conductor as VBUS on sheet 3.  Their pins pool
 * here and print after the last sheet; unnamed nets stay sheet-local. */
#define	MAXMNET	24
char	mnname[MAXMNET][10];
char	mnpins[MAXMNET][120];
short	mncnt[MAXMNET];
int	nmnet;

/* one pin as " REF.PIN" (pin NAMES when the library has them) */
static
pinstr(o, pin, buf)
register DOBJ *o;
char *buf;
{
	register int e;

	sprintf(buf, " %s.", o->o_name[0] ? o->o_name : "?");
	e = pinnm[PINSLOT(&symtab[o->o_sym], pin)];
	if ( e )
		strcat(buf, &pnmpool[e]);
	else
		sprintf(buf + strlen(buf), "%d", pin + 1);
	return 0;
}

/* after the last sheet: the merged named nets, and their pin check */
static
donetend()
{
	register int i;

	for ( i = 0; i < nmnet; i++ )
	{
		printf("%s:%s\n", mnname[i], mnpins[i]);
		if ( mncnt[i] < 2 )
			fprintf(stderr,
				"vellum: warning: net %s has %d pin\n",
				mnname[i], mncnt[i]);
	}
	return 0;
}

static
donet(sheet)
{
	register DOBJ *o;
	register int i, j;
	int k, np, gx, gy, e, px, py;
	short pobj[MAXNPIN], ppin[MAXNPIN], pnet[MAXNPIN];
	char nnm[MAXOBJ / 4][10];	/* net root -> name (GND/VCC/N)   */
	short nroot[MAXOBJ / 4];
	int nnames;
	int nid, first, cnt;
	short outed[MAXOBJ];

	for ( i = 0; i < nobj; i++ )
		wnet[i] = i;
	/* wires: endpoints on another wire join the nets */
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_WIRE )
			continue;
		for ( j = 0; j < nobj; j++ )
		{
			if ( j == i || obj[j].o_type != OT_WIRE )
				continue;
			if ( xonwire(&obj[j], (int)o->o_x, (int)o->o_y) ||
			     xonwire(&obj[j], (int)o->o_x2, (int)o->o_y2) )
				nunion(i, j);
		}
	}
	/* pins: each pin joins the net of any wire through its point;
	 * pins sharing a point join each other */
	np = 0;
	for ( i = 0; i < nobj && np < MAXNPIN; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM )
			continue;
		if ( symtab[o->o_sym].sy_pins == 0 )
			continue;
		for ( k = 0; k < symtab[o->o_sym].sy_pins[0] &&
			     np < MAXNPIN; k++ )
		{
			xpinpos(i, k, &gx, &gy);
			pobj[np] = i;
			ppin[np] = k;
			pnet[np] = -1;
			for ( j = 0; j < nobj; j++ )
				if ( obj[j].o_type == OT_WIRE &&
				     xonwire(&obj[j], gx, gy) )
				{
					pnet[np] = nfind(j);
					break;
				}
			if ( pnet[np] < 0 )
			{
				/* no wire: pins stacked on the same
				 * point still connect */
				for ( j = 0; j < np; j++ )
				{
					int ox, oy;

					xpinpos((int)pobj[j], (int)ppin[j],
						&ox, &oy);
					if ( ox == gx && oy == gy &&
					     pnet[j] >= 0 )
					{
						pnet[np] = pnet[j];
						break;
					}
				}
			}
			np++;
		}
	}
	/* re-root every pin net after the unions above */
	for ( i = 0; i < np; i++ )
		if ( pnet[i] >= 0 )
			pnet[i] = nfind((int)pnet[i]);
	/* GND/VCC symbols and N objects NAME their nets */
	nnames = 0;
	for ( i = 0; i < np && nnames < MAXOBJ / 4; i++ )
	{
		register char *c;

		c = symtab[obj[pobj[i]].o_sym].sy_code;
		if ( strcmp(c, "GND") != 0 && strcmp(c, "VCC") != 0 )
			continue;
		if ( pnet[i] < 0 )
			continue;
		for ( j = 0; j < nnames; j++ )
			if ( nroot[j] == pnet[i] )
				break;
		if ( j == nnames )
		{
			nroot[nnames] = pnet[i];
			strcpy(nnm[nnames], c);
			nnames++;
		}
	}
	for ( i = 0; i < nobj && nnames < MAXOBJ / 4; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_NNAME )
			continue;
		px = o->o_x;
		py = o->o_y;
		for ( j = 0; j < nobj; j++ )
			if ( obj[j].o_type == OT_WIRE &&
			     xonwire(&obj[j], px, py) )
			{
				int r;

				r = nfind(j);
				for ( k = 0; k < nnames; k++ )
					if ( nroot[k] == r )
						break;
				if ( k == nnames )
				{
					nroot[nnames] = r;
					strncpy(nnm[nnames], o->o_name, 9);
					nnm[nnames][9] = 0;
					nnames++;
				}
				break;
			}
	}
	/* output: one line per net with pins on it.  A NAMED net is not
	 * printed here: its pins pool into the set-wide merge table and
	 * print after the last sheet (donetend); unnamed nets print now,
	 * qualified NET-<sheet>.<n> when the set has several sheets. */
	for ( i = 0; i < nobj; i++ )
		outed[i] = 0;
	nid = 0;
	for ( i = 0; i < np; i++ )
	{
		int r, m;
		char pb[24];

		if ( pnet[i] < 0 )
			continue;
		r = pnet[i];
		if ( outed[r] )
			continue;
		outed[r] = 1;
		nid++;
		for ( k = 0; k < nnames; k++ )
			if ( nroot[k] == r )
				break;
		m = -1;
		if ( k < nnames )
		{
			for ( m = 0; m < nmnet; m++ )
				if ( strcmp(mnname[m], nnm[k]) == 0 )
					break;
			if ( m == nmnet )
			{
				if ( nmnet >= MAXMNET )
					m = -1;
				else
				{
					strcpy(mnname[m], nnm[k]);
					mnpins[m][0] = 0;
					mncnt[m] = 0;
					nmnet++;
				}
			}
		}
		else if ( nsheets > 1 )
			printf("NET-%d.%d:", sheet, nid);
		else
			printf("NET-%d:", nid);
		cnt = 0;
		for ( j = 0; j < np; j++ )
		{
			if ( pnet[j] != r )
				continue;
			o = &obj[pobj[j]];
			if ( symtab[o->o_sym].sy_pfx[0] == 0 )
				continue;	/* a designator-less stencil
						 * (rails, taps, off-page
						 * markers) only NAMES or
						 * carries the net -- it is
						 * never a listed pin */
			pinstr(o, (int)ppin[j], pb);
			if ( k < nnames )
			{
				if ( m >= 0 &&
				     strlen(mnpins[m]) + strlen(pb) <
				     sizeof(mnpins[0]) )
					strcat(mnpins[m], pb);
			}
			else
				printf("%s", pb);
			cnt++;
		}
		if ( k < nnames )
		{
			if ( m >= 0 )
				mncnt[m] += cnt;
		}
		else
		{
			printf("\n");
			if ( cnt < 2 )
				fprintf(stderr,
				    "vellum: warning: NET-%d has %d pin\n",
				    nid, cnt);
		}
	}
	/* unconnected pins are the classic drawing error: say so --
	 * except on designator-less stencils, whose spare pins are by
	 * design (an off-page marker uses one of its two) */
	for ( i = 0; i < np; i++ )
		if ( pnet[i] < 0 )
		{
			o = &obj[pobj[i]];
			if ( symtab[o->o_sym].sy_pfx[0] == 0 )
				continue;
			fprintf(stderr,
				"vellum: warning: %s pin %d unconnected\n",
				o->o_name[0] ? o->o_name : "?",
				ppin[i] + 1);
		}
	return 0;
}

/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

/* Every file argument is a SHEET; shell globs are the set syntax
 * (sec. 19): -print emits a form feed between sheets, -net merges
 * named nets across them, -bom yields ONE parts list for the design. */
velxport(argc, argv)
char **argv;
{
	register char *mode;
	register int i;
	int r, first;

	mode = (char *)0;
	for ( i = 1; i < argc && argv[i][0] == '-'; i++ )
	{
		if ( strcmp(argv[i], "-wide") == 0 )
			widef = 1;
		else if ( strcmp(argv[i], "-scale") == 0 && i + 1 < argc )
			xsc = atoi(argv[++i]);
		else if ( mode == (char *)0 )
			mode = argv[i] + 1;
		else
			mode = "?";
	}
	if ( xsc < 2 || xsc > 32 )
		xsc = 8;
	nsheets = argc - i;
	if ( mode == (char *)0 || nsheets < 1 ||
	     (strcmp(mode, "print") != 0 && strcmp(mode, "pic") != 0 &&
	      strcmp(mode, "net") != 0 && strcmp(mode, "hpgl") != 0 &&
	      strcmp(mode, "bom") != 0) )
	{
		fprintf(stderr,
	"usage: vellum -print|-pic|-net|-hpgl|-bom [-wide] [-scale N] file.d ...\n");
		return 1;
	}
	loadsyms();
	r = 0;
	first = i;
	for ( ; i < argc; i++ )
	{
		if ( loadfile(argv[i]) < 0 )
		{
			fprintf(stderr, "vellum: cannot open %s\n", argv[i]);
			return 1;
		}
		if ( strcmp(mode, "print") == 0 )
			r |= doprint();
		else if ( strcmp(mode, "pic") == 0 )
		{
			xsc = 8;	/* pic is pinned: 16 units/inch */
			r |= dopic();
		}
		else if ( strcmp(mode, "hpgl") == 0 )
			r |= dohpgl();
		else if ( strcmp(mode, "bom") == 0 )
			bomsheet();
		else
			donet(i - first + 1);
	}
	if ( strcmp(mode, "bom") == 0 )
		dobomout();
	else if ( strcmp(mode, "net") == 0 )
		donetend();
	return r;
}
