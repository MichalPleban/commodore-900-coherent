/*
 * velgfx.c - Vellum's pure drawing helpers: integer trig, chorded arcs,
 * styled lines (dash/dot via cl_lpat, cheap bold), the parametric SHAPE
 * outlines and row-span fills, shape labels and arrowheads.  No object
 * or UI state lives here -- everything is (px, style) in, cl_* out --
 * which makes this the clean seam for keeping any one module inside the
 * assembler's fix-up capacity.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "vellum.h"



/* ---- integer trig for arcs: sin(deg) scaled by 256, no floating point.
 * Ten-degree table, linear interpolation between entries -- more than
 * enough for chorded arcs on a 1-bpp screen and the 180-dpi printer. ---- */
static short sintab[10] = { 0, 44, 88, 128, 165, 196, 222, 241, 252, 256 };

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

icos(a)
{
	return isin(a + 90);
}

/* Integer "atan2": angle of (dx,dy) in degrees 0..359, y UP (canvas dy is
 * given as screen-down, callers negate).  Octant fold + a slope table. */
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
	/* t in 0..90 "double-octant" units; map through tan correction:
	 * atan(r) ~ r*45 + r*(1-r)*11 for r in 0..1 (r = t/90) */
	a = (int)(t / 2 + t * (90 - t) * 11 / 8100);
	if ( sw )
		a = 90 - a;
	if ( dx < 0 )
		a = 180 - a;
	if ( dy < 0 )
		a = 360 - a;
	return a % 360;
}

/* Draw a circular arc: centre (cx,cy) px, radius r px, from angle a0 CCW
 * to a1 (degrees, y up = CCW on screen runs visually clockwise; every
 * consumer -- canvas, printer, pic -- uses the same convention so arcs
 * round-trip).  Chorded: short cl_line segments, step sized to radius. */
arcline(cx, cy, r, a0, a1, mode)
{
	register int a, step;
	int x0, y0, x1, y1;

	if ( r <= 0 )
		return 0;
	while ( a1 <= a0 )
		a1 += 360;
	step = 200 / r + 3;		/* ~2-4 px chords */
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

/* A shallow "flat arc" from (x0,y) to (x1,y) bulging e px at the middle
 * (e > 0 bulges DOWN) -- the drum's ellipse caps and the document shape's
 * wavy base, chorded parabolically in integers. */
flatarc(x0, x1, y, e, mode)
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
		cl_line(x, ly, xn, ny, mode);
		ly = ny;
	}
	return 0;
}

/* ---- styled drawing: every outline primitive honours the object's
 * OF_STYLE (solid/dashed/dotted, via clgfx's cl_lpat run-length mask in
 * the Bresenham walk) and OF_BOLD (the line drawn twice, offset toward
 * its fatter axis -- cheap width 2, no general thick-line engine). ---- */

stpat(fl)
{
	switch ( fl & OF_STYLE )
	{
	case OF_DASH:	cl_lpat(0xf0f0);	break;
	case OF_DOT:	cl_lpat(0xaaaa);	break;
	default:	cl_lpat(0xffff);	break;
	}
	return 0;
}

sline(x0, y0, x1, y1, mode, fl)
{
	register int ax, ay;

	cl_line(x0, y0, x1, y1, mode);
	if ( fl & OF_BOLD )
	{
		ax = x1 - x0;	if ( ax < 0 ) ax = -ax;
		ay = y1 - y0;	if ( ay < 0 ) ay = -ay;
		if ( ax >= ay )
			cl_line(x0, y0 + 1, x1, y1 + 1, mode);
		else
			cl_line(x0 + 1, y0, x1 + 1, y1, mode);
	}
	return 0;
}

/* Styled arc: solid arcs go through the chorded walker; dashed/dotted
 * ones alternate ANGULAR chunks (a chord restarts the pixel pattern, so
 * the mask alone cannot dash a curve). */
arcst(cx, cy, r, a0, a1, mode, fl)
{
	register int a, dang;
	int b, px, py;

	if ( r <= 0 )
		return 0;
	while ( a1 <= a0 )
		a1 += 360;
	switch ( fl & OF_STYLE )
	{
	case OF_DASH:
		dang = 380 / r + 4;
		for ( a = a0; a < a1; a += 2 * dang )
		{
			b = a + dang;
			if ( b > a1 )
				b = a1;
			arcline(cx, cy, r, a, b, mode);
			if ( (fl & OF_BOLD) && r > 1 )
				arcline(cx, cy, r - 1, a, b, mode);
		}
		break;
	case OF_DOT:
		dang = 190 / r + 3;
		for ( a = a0; a <= a1; a += dang )
		{
			px = cx + (r * icos(a) + 128) / 256;
			py = cy - (r * isin(a) + 128) / 256;
			cl_line(px, py, px, py, mode);
		}
		break;
	default:
		arcline(cx, cy, r, a0, a1, mode);
		if ( (fl & OF_BOLD) && r > 1 )
			arcline(cx, cy, r - 1, a0, a1, mode);
		break;
	}
	return 0;
}

scirc(cx, cy, r, mode, fl)
{
	if ( (fl & OF_STYLE) == 0 )
	{
		cl_circle(cx, cy, r, mode);
		if ( (fl & OF_BOLD) && r > 1 )
			cl_circle(cx, cy, r - 1, mode);
		return 0;
	}
	arcst(cx, cy, r, 0, 360, mode, fl);
	return 0;
}

/* The font slot a text size selects: the three shared tail fonts. */
fontslot(sz)
{
	return sz <= 0 ? SHM_FICON : sz == 1 ? SHM_FTERM : SHM_FUI;
}

/* Vertical text (OF_VERT): each glyph's cell TRANSPOSED into a small
 * buffer and blitted -- no new fonts, no angles beyond 90 (sec. 15).
 * A quarter turn clockwise, so the string reads top-to-bottom; '|'
 * starts the next line one cell to the RIGHT. */
vtext(fs, x, y, s)
char *s;
{
	register HRFONT *f;
	register int r, c;
	unsigned short vb[16];		/* rotated glyph: cellw rows, 1 word */
	unsigned short w;
	int gy, gi;

	f = hr_font(fs);
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
		{
			for ( r = 0; r < f->cellw; r++ )
			{
				w = 0;
				for ( c = 0; c < f->cellh; c++ )
					if ( f->bits[gi * f->cellh +
						     f->cellh - 1 - c] &
					     (0x8000 >> r) )
						w |= 0x8000 >> c;
				vb[r] = w;
			}
			cl_blit(x, gy, x + f->cellh, gy + f->cellw,
				(int *)vb, 1);
		}
		gy += f->cellw;
	}
	return 0;
}

/* ---- shapes: parametric outlines from the bounding rect ---- */

/* One horizontal fill span [x0..x1] at row y: val as cl_fillrect, plus
 * val 4 = HATCH -- white ground with 45-degree stripes from cl_dotrow,
 * one primitive per row (sec. 16), the phase anchored to canvas x+y so
 * partial (damage) repaints mesh. */
rowspan(x0, x1, y, val)
{
	register int xs, n;

	if ( x1 < x0 )
		return 0;
	if ( val != 4 )
	{
		cl_fillrect(x0, y, x1 + 1, y + 1, val);
		return 0;
	}
	cl_fillrect(x0, y, x1 + 1, y + 1, 1);
	xs = x0 + ((8 - ((x0 + y) & 7)) & 7);
	n = (x1 - xs) / 8 + 1;
	if ( n > 0 )
		cl_dotrow(xs, y, n, 8);
	return 0;
}

/* Object flags -> the rowspan/fillshape val: 1 white / 3 gray / 0 black,
 * 4 = the hatch modifier on a gray fill. */
fillval(fl)
{
	register int f;

	f = fl & OF_FILL;
	if ( f == OF_FILLG && (fl & OF_HATCH) )
		return 4;
	return f == OF_FILLW ? 1 : f == OF_FILLG ? 3 : 0;
}

/* Row-span fill of shape kind in px rect (x0,y0)-(x1,y1) (normalized),
 * val as rowspan above.  Fill runs BEFORE the outline, so z-order (list
 * order) composes filled shapes correctly. */
fillshape(kind, x0, y0, x1, y1, val)
{
	register int y, k;
	int w2, h2, cx, cy, r, e, s, xo, yb;

	w2 = (x1 - x0) / 2;
	h2 = (y1 - y0) / 2;
	cx = (x0 + x1) / 2;
	cy = (y0 + y1) / 2;
	switch ( kind )
	{
	case SH_BOX:
		if ( val != 4 )
		{
			cl_fillrect(x0, y0, x1 + 1, y1 + 1, val);
			break;
		}
		for ( y = y0; y <= y1; y++ )
			rowspan(x0, x1, y, val);
		break;

	case SH_RBOX:
		r = (w2 < h2 ? w2 : h2) / 2;
		if ( r > 12 ) r = 12;
		for ( y = y0; y <= y1; y++ )
		{
			k = (y < y0 + r) ? y0 + r - y :
			    (y > y1 - r) ? y - (y1 - r) : 0;
			xo = k ? r - (int)isqrt((long)r * r - (long)k * k) : 0;
			rowspan(x0 + xo, x1 - xo, y, val);
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
			rowspan(cx - xo, cx + xo, y, val);
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
				xo = (int)isqrt((long)r * r - (long)k * k);
				rowspan(x0 + r - xo, x1 - r + xo, y, val);
			}
			else
			{
				k = (y < y0 + r) ? y0 + r - y :
				    (y > y1 - r) ? y - (y1 - r) : 0;
				xo = k ? r - (int)isqrt((long)r * r -
							(long)k * k) : 0;
				rowspan(x0 + xo, x1 - xo, y, val);
			}
		}
		break;

	case SH_PAR:
		s = (x1 - x0) / 4;
		if ( s > y1 - y0 ) s = y1 - y0;
		if ( y1 <= y0 )
			break;
		for ( y = y0; y <= y1; y++ )
			rowspan(x0 + s * (y1 - y) / (y1 - y0),
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
			xo = k ? w2 * (int)isqrt((long)e * e - (long)k * k) / e
			       : w2;
			rowspan(cx - xo, cx + xo, y, val);
		}
		break;

	case SH_DOC:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		yb = y1 - e;
		if ( val != 4 )
		{
			cl_fillrect(x0, y0, x1 + 1, yb + 1, val);
			break;
		}
		for ( y = y0; y <= yb; y++ )
			rowspan(x0, x1, y, val);
		break;

	case SH_CIRC:
		r = w2 < h2 ? w2 : h2;
		for ( y = cy - r; y <= cy + r; y++ )
		{
			k = y - cy;
			if ( k < 0 ) k = -k;
			xo = (int)isqrt((long)r * r - (long)k * k);
			rowspan(cx - xo, cx + xo, y, val);
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
				   isqrt((long)h2 * h2 - (long)k * k) / h2);
			rowspan(cx - xo, cx + xo, y, val);
		}
		break;
	}
	return 0;
}

/* Outline of shape kind in the px rect, honouring style/bold. */
shapeoutline(kind, x0, y0, x1, y1, fl)
{
	int w2, h2, cx, cy, r, e, s, xm, yb;

	w2 = (x1 - x0) / 2;
	h2 = (y1 - y0) / 2;
	cx = (x0 + x1) / 2;
	cy = (y0 + y1) / 2;
	stpat(fl);
	switch ( kind )
	{
	case SH_BOX:
		sline(x0, y0, x1, y0, 0, fl);
		sline(x1, y0, x1, y1, 0, fl);
		sline(x1, y1, x0, y1, 0, fl);
		sline(x0, y1, x0, y0, 0, fl);
		break;

	case SH_RBOX:
		r = (w2 < h2 ? w2 : h2) / 2;
		if ( r > 12 ) r = 12;
		sline(x0 + r, y0, x1 - r, y0, 0, fl);
		sline(x0 + r, y1, x1 - r, y1, 0, fl);
		sline(x0, y0 + r, x0, y1 - r, 0, fl);
		sline(x1, y0 + r, x1, y1 - r, 0, fl);
		arcst(x0 + r, y0 + r, r, 90, 180, 0, fl);
		arcst(x1 - r, y0 + r, r, 0, 90, 0, fl);
		arcst(x0 + r, y1 - r, r, 180, 270, 0, fl);
		arcst(x1 - r, y1 - r, r, 270, 360, 0, fl);
		break;

	case SH_DIAM:
		sline(cx, y0, x1, cy, 0, fl);
		sline(x1, cy, cx, y1, 0, fl);
		sline(cx, y1, x0, cy, 0, fl);
		sline(x0, cy, cx, y0, 0, fl);
		break;

	case SH_OVAL:
		r = w2 < h2 ? w2 : h2;
		if ( w2 >= h2 )
		{
			sline(x0 + r, y0, x1 - r, y0, 0, fl);
			sline(x0 + r, y1, x1 - r, y1, 0, fl);
			arcst(x0 + r, cy, r, 90, 270, 0, fl);
			arcst(x1 - r, cy, r, 270, 90, 0, fl);
		}
		else
		{
			sline(x0, y0 + r, x0, y1 - r, 0, fl);
			sline(x1, y0 + r, x1, y1 - r, 0, fl);
			arcst(cx, y0 + r, r, 0, 180, 0, fl);
			arcst(cx, y1 - r, r, 180, 360, 0, fl);
		}
		break;

	case SH_PAR:
		s = (x1 - x0) / 4;
		if ( s > y1 - y0 ) s = y1 - y0;
		sline(x0 + s, y0, x1, y0, 0, fl);
		sline(x1, y0, x1 - s, y1, 0, fl);
		sline(x1 - s, y1, x0, y1, 0, fl);
		sline(x0, y1, x0 + s, y0, 0, fl);
		break;

	case SH_DRUM:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		flatarc(x0, x1, y0 + e, -e, 0);
		flatarc(x0, x1, y0 + e, e, 0);
		sline(x0, y0 + e, x0, y1 - e, 0, fl);
		sline(x1, y0 + e, x1, y1 - e, 0, fl);
		flatarc(x0, x1, y1 - e, e, 0);
		break;

	case SH_DOC:
		e = (y1 - y0) / 6;
		if ( e < 2 ) e = 2;
		yb = y1 - e;
		xm = cx;
		sline(x0, y0, x1, y0, 0, fl);
		sline(x0, y0, x0, yb, 0, fl);
		sline(x1, y0, x1, yb, 0, fl);
		flatarc(x0, xm, yb, e, 0);
		flatarc(xm, x1, yb, -e, 0);
		break;

	case SH_CIRC:
		scirc(cx, cy, w2 < h2 ? w2 : h2, 0, fl);
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
				nx = cx + (w2 * icos(a) + 128) / 256;
				ny = cy - (h2 * isin(a) + 128) / 256;
				sline(lx, ly, nx, ny, 0, fl);
				lx = nx;
				ly = ny;
			}
		}
		break;
	}
	cl_lpat(0xffff);
	return 0;
}

/* ---- smooth polylines: the chorded quadratic B-spline (velbase
 * bspline) drawn with the object's style ---- */
static int	splfl;

static
splseg(x0, y0, x1, y1)
{
	sline(x0, y0, x1, y1, 0, splfl);
	return 0;
}

drawspline(xy, n, fl)
int *xy;
{
	splfl = fl;
	stpat(fl);
	bspline(xy, n, splseg);
	cl_lpat(0xffff);
	return 0;
}

/* The centred label (FUI font, +1,+1 optical correction), clipped to the
 * shape by TRUNCATION -- never wider than the rect. */
shlabel(s, x0, y0, x1, y1)
char *s;
{
	char tb[VALL];
	register int i;
	int maxc;

	if ( s[0] == 0 || y1 - y0 < 18 )
		return 0;
	maxc = (x1 - x0 - 4) / 9;
	if ( maxc <= 0 )
		return 0;
	for ( i = 0; s[i] && i < maxc && i < VALL - 1; i++ )
		tb[i] = s[i];
	tb[i] = 0;
	cl_ptextt(SHM_FUI, (x0 + x1 - i * 9) / 2 + 1, (y0 + y1 - 16) / 2 + 1,
		  tb);
	return 0;
}

/* A dimension between two px points: extension ticks across each end,
 * the dimension line with arrowheads BOTH ends, and the label centred
 * beside the middle (above a horizontal-ish line, right of a vertical
 * one) -- small font, transparent, so it sits ON drawings cleanly. */
dimdraw(x0, y0, x1, y1, s)
char *s;
{
	register int ax, ay;
	int mx, my, lw;

	ax = x1 - x0;	if ( ax < 0 ) ax = -ax;
	ay = y1 - y0;	if ( ay < 0 ) ay = -ay;
	if ( ax >= ay )			/* ticks cross the ends */
	{
		cl_line(x0, y0 - 4, x0, y0 + 4, 0);
		cl_line(x1, y1 - 4, x1, y1 + 4, 0);
	}
	else
	{
		cl_line(x0 - 4, y0, x0 + 4, y0, 0);
		cl_line(x1 - 4, y1, x1 + 4, y1, 0);
	}
	cl_line(x0, y0, x1, y1, 0);
	arrowhead(x0, y0, x0 - x1, y0 - y1);
	arrowhead(x1, y1, x1 - x0, y1 - y0);
	mx = (x0 + x1) / 2;
	my = (y0 + y1) / 2;
	lw = strlen(s) * 6;
	if ( ax >= ay )
		cl_ptextt(SHM_FICON, mx - lw / 2, my - 12, s);
	else
		cl_ptextt(SHM_FICON, mx + 5, my - 4, s);
	return 0;
}

/* Arrowhead barbs at (bx,by) coming FROM direction (dx,dy) (need not be
 * normalized); solid regardless of the connector's line style. */
arrowhead(bx, by, dx, dy)
{
	long len;
	int hx, hy, px, py;

	len = isqrt((long)dx * dx + (long)dy * dy);
	if ( len == 0 )
		return 0;
	hx = (int)((long)dx * 7 / len);
	hy = (int)((long)dy * 7 / len);
	px = (int)(-(long)dy * 3 / len);
	py = (int)((long)dx * 3 / len);
	cl_line(bx, by, bx - hx + px, by - hy + py, 0);
	cl_line(bx, by, bx - hx - px, by - hy - py, 0);
	return 0;
}

