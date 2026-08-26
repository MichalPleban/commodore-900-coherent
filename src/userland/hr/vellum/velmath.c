/*
 * velmath.c - the geometry with no state in it at all: an integer
 * square root and a chorded B-spline.
 *
 * A member of its OWN because of who needs it.  velgfx calls both and
 * nothing else outside itself, so a client that only DRAWS must be
 * able to link velgfx without dragging the 400-object drawing table
 * in behind it.  Coherent's
 * ld pulls a whole member for one symbol, so what shares a member is a
 * decision about what a client pays for.
 */
#include <stdio.h>
#include "vellum.h"

/* Integer square root -- radius math everywhere. */
long
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


/* Chorded quadratic B-spline through a polyline's DEVICE points (2n ints
 * in xy[]): the curve runs from the first point, through the midpoint of
 * each interior edge with the vertex as control, to the last -- chords
 * from integer midpoint subdivision (the flatarc trick generalized).
 * cl_-free: the editor's emit draws styled canvas lines, the exporters'
 * emit feeds a backend. */
static
bsseg(x0, y0, cx, cy, x1, y1, emit, dep)
int (*emit)();
{
	int mx0, my0, mx1, my1, mx, my;

	if ( dep <= 0 )
	{
		(*emit)(x0, y0, x1, y1);
		return 0;
	}
	mx0 = (x0 + cx) / 2;	my0 = (y0 + cy) / 2;
	mx1 = (cx + x1) / 2;	my1 = (cy + y1) / 2;
	mx = (mx0 + mx1) / 2;	my = (my0 + my1) / 2;
	bsseg(x0, y0, mx0, my0, mx, my, emit, dep - 1);
	bsseg(mx, my, mx1, my1, x1, y1, emit, dep - 1);
	return 0;
}

bspline(xy, n, emit)
register int *xy;
int (*emit)();
{
	register int i;
	int ax, ay, bx, by;

	if ( n < 3 )
	{
		if ( n == 2 )
			(*emit)(xy[0], xy[1], xy[2], xy[3]);
		return 0;
	}
	ax = xy[0];
	ay = xy[1];
	for ( i = 1; i < n - 1; i++ )
	{
		if ( i == n - 2 )
		{
			bx = xy[2 * n - 2];
			by = xy[2 * n - 1];
		}
		else
		{
			bx = (xy[2*i] + xy[2*i + 2]) / 2;
			by = (xy[2*i + 1] + xy[2*i + 3]) / 2;
		}
		bsseg(ax, ay, xy[2*i], xy[2*i + 1], bx, by, emit, 3);
		ax = bx;
		ay = by;
	}
	return 0;
}
