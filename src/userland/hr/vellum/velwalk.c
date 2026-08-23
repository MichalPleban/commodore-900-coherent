/*
 * velwalk.c - Vellum's device-coordinate object WALKER: every printable
 * object fed through the 8-function backend struct (vellum.h XB), plus
 * the cl_-free geometry it needs (integer sqrt/trig, the symbol
 * transform, chorded arcs and shape outlines/fills in device px).
 *
 * Factored OUT of velport.c (VELLUM.md sec. 25) so two binaries share
 * it: velxport (the headless -print/-pic/-hpgl exporters) and velprev
 * (the print-preview window, whose cl_* backend draws the same walk
 * into a window) -- the SAME walker feeds the Epson bands and the
 * preview, so what the window shows is what the paper gets.  Keeps the
 * exporters' own copies of sqrt/sin (velgfx has twins, but velgfx pulls
 * cl_* and the headless build must not).
 */
#include <stdio.h>
#include "vellum.h"

int	xsc	= 8;		/* device px per grid unit (-scale N;     */
				/* print only -- pic/hpgl pin it back)    */
int	widef;			/* -wide: landscape raster (print)        */

int	xorgx, xorgy;		/* device origin (print: the used extent) */
int	xlay;			/* layer of the object being walked (the  */
				/* hpgl backend picks its pen by it)      */

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

long
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

/* arc through the backend: its own TRUE arc when it has one (b_varc --
 * a chorded arc is correct on paper but wrong in a file another CAD
 * will edit, VELLUM.md sec. 36), else chorded through b_line */
static
xarc(xb, cx, cy, r, a0, a1)
XB *xb;
{
	register int a, step;
	int x0, y0, x1, y1;

	if ( r <= 0 )
		return 0;
	if ( xb->b_varc )
		return (*xb->b_varc)(cx, cy, r, a0, a1);
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

/* closed even-odd fill of a polyline's device points through b_span
 * (velgfx fillpoly's device twin -- VELLUM.md sec. 39: the OF_FILL
 * bits on P lines finally draw, in every export and the preview) */
static
xfillpoly(xb, xy, n, val)
XB *xb;
register int *xy;
{
	short ex[PMAXPT];
	register int i, k;
	int y, y0, y1, m, t;

	if ( n < 3 || xb->b_span == 0 )
		return 0;
	y0 = y1 = xy[1];
	for ( i = 1; i < n; i++ )
	{
		if ( xy[2*i + 1] < y0 ) y0 = xy[2*i + 1];
		if ( xy[2*i + 1] > y1 ) y1 = xy[2*i + 1];
	}
	for ( y = y0; y <= y1; y++ )
	{
		m = 0;
		for ( i = 0; i < n; i++ )
		{
			int ax, ay, bx, by;

			ax = xy[2*i];
			ay = xy[2*i + 1];
			k = i + 1 == n ? 0 : i + 1;
			bx = xy[2*k];
			by = xy[2*k + 1];
			if ( ay == by )
				continue;
			if ( (y >= ay && y < by) || (y >= by && y < ay) )
				ex[m++] = ax + (int)((long)(y - ay) *
					  (bx - ax) / (by - ay));
		}
		for ( i = 1; i < m; i++ )
			for ( k = i; k > 0 && ex[k - 1] > ex[k]; k-- )
			{
				t = ex[k];
				ex[k] = ex[k - 1];
				ex[k - 1] = t;
			}
		for ( i = 0; i + 1 < m; i += 2 )
			(*xb->b_span)(ex[i], ex[i + 1], y, val);
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
 * curve (print, hpgl, the preview): bspline calls xseg per chord */
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
	char db[DIMLBL];
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
	char lb[TVMAX];
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
xprn(l)
{
	return l >= 0 && l < NLAYER && layprn[l] && l != 3;
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

/* ---- THE WALKER: every printable object through the backend ---- */
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
		xlay = o->o_layer;
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
					       (int)o->o_rot, oval(o));
			else
				xtext(xb, dx(o->o_x), dy(o->o_y),
				      (int)o->o_rot, oval(o));
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
			{
				int pxy[2 * PMAXPT];

				for ( k = 0; k < o->o_sym; k++ )
				{
					pxy[2*k] = dx(ppool[o->o_x2 + 2*k]);
					pxy[2*k + 1] =
					    dy(ppool[o->o_x2 + 2*k + 1]);
				}
				if ( fill >= 0 )	/* v4: filled polys */
					xfillpoly(xb, pxy, (int)o->o_sym,
						  fill);
				if ( (fl & OF_SMOOTH) && o->o_sym >= 3 )
				{
					if ( xb->b_poly )
						(*xb->b_poly)(pxy,
							      (int)o->o_sym);
					else
					{
						xbcur = xb;
						bspline(pxy, (int)o->o_sym,
							xseg);
					}
					break;
				}
				for ( k = 1; k < o->o_sym; k++ )
					(*xb->b_line)(pxy[2*k - 2],
					    pxy[2*k - 1], pxy[2*k],
					    pxy[2*k + 1]);
			}
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
			if ( oval(o)[0] && y1 - y0 >= 18 )
			{
				char tb[TVMAX];
				register char *vp;

				vp = oval(o);
				k = (x1 - x0 - 4) / 9;
				for ( t = 0; vp[t] && t < k &&
					     t < TVMAX - 1; t++ )
					tb[t] = vp[t];
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

/* grid extent of the printable drawing; 0 when empty */
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
