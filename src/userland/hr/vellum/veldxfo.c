/*
 * veldxfo.c - the DXF writer half of veldxf: a vellum drawing OUT to
 * the R10 entity subset, so the office PC can open what the shop drew.
 *
 *	veldxf -x file.d ... > file.dxf
 *
 * The reader half (veldxf.c) is a text filter with no model in it at
 * all; this half is an ordinary walker backend, and it is the only
 * reason the tool links libvellum.  Together they fix-point: a
 * drawing written here and read back there is the same drawing, and
 * the golden gate proves it every run.
 */
#include <stdio.h>
#include "vellum.h"

/* ================================================================== */
/* -x: DXF (R10 entity subset) OUT -- the interchange release          */
/* (VELLUM.md sec. 36).  ENTITIES section only; coordinates in GRID    */
/* units (the walker runs at the pinned xsc 4, so symbol geometry's    */
/* quarter units emit exactly as .25 steps), y flipped (DXF y is up),  */
/* our layer number as the DXF layer name.  Arcs ride b_varc: a        */
/* chorded arc is correct on paper but wrong in a file another CAD     */
/* will edit.                                                          */
/* ================================================================== */

int	dxymax;			/* device y of the sheet top (flip)       */

/* one coordinate group: device px (xsc 4) as grid units, 2 decimals */
static
dx_g(code, v)
{
	register int neg;

	printf("%d\n", code);
	neg = v < 0;
	if ( neg )
		v = -v;
	printf("%s%d.%02d\n", neg ? "-" : "", v / 4, (v % 4) * 25);
	return 0;
}

static
dx_hdr(ent)
char *ent;
{
	printf("0\n%s\n8\n%d\n", ent, xlay);
	return 0;
}

static
dx_line(x0, y0, x1, y1)
{
	dx_hdr("LINE");
	dx_g(10, x0);
	dx_g(20, dxymax - y0);
	dx_g(11, x1);
	dx_g(21, dxymax - y1);
	return 0;
}

static
dx_box(x0, y0, x1, y1, fill)
{
	dx_line(x0, y0, x1, y0);
	dx_line(x1, y0, x1, y1);
	dx_line(x1, y1, x0, y1);
	dx_line(x0, y1, x0, y0);
	return 0;
}

static
dx_circle(cx, cy, r, fill)
{
	dx_hdr("CIRCLE");
	dx_g(10, cx);
	dx_g(20, dxymax - cy);
	dx_g(40, r);
	return 0;
}

static
dx_varc(cx, cy, r, a0, a1)
{
	while ( a1 < 0 )
		a1 += 360;
	while ( a0 < 0 )
		a0 += 360;
	dx_hdr("ARC");
	dx_g(10, cx);
	dx_g(20, dxymax - cy);
	dx_g(40, r);
	printf("50\n%d\n51\n%d\n", a0 % 360, a1 % 360);
	return 0;
}

/* the shared TEXT emitter; rot = the group-50 rotation (vertical text) */
static
dx_text1(x, y, sz, s, rot)
char *s;
{
	static short ch[3] = { 8, 15, 16 };

	if ( sz < 0 ) sz = 0;
	if ( sz > 2 ) sz = 2;
	dx_hdr("TEXT");
	dx_g(10, x);
	dx_g(20, dxymax - y - ch[sz] * 4 / 8);	/* cell top -> baseline */
	dx_g(40, ch[sz] * 4 / 8);	/* height = the cell, grid units --
					 * veldxf's size thresholds map it
					 * straight back (round trip)      */
	printf("1\n%s\n", s);
	if ( rot )
		printf("50\n%d\n", rot);
	return 0;
}

static
dx_text(x, y, sz, s)
char *s;
{
	return dx_text1(x, y, sz, s, 0);
}

static
dx_vtext(x, y, sz, s)
char *s;
{
	return dx_text1(x, y, sz, s, 270);
}

/* smooth polylines keep their POINTS (POLYLINE/VERTEX/SEQEND) */
static
dx_poly(xy, n)
register int *xy;
{
	register int k;

	dx_hdr("POLYLINE");
	printf("66\n1\n70\n0\n");
	for ( k = 0; k < n; k++ )
	{
		dx_hdr("VERTEX");
		dx_g(10, xy[2*k]);
		dx_g(20, dxymax - xy[2*k + 1]);
	}
	printf("0\nSEQEND\n");
	return 0;
}

static
dx_style(fl)
{
	return 0;
}

XB	dxfxb = { dx_line, dx_box, dx_circle, dx_text, (int (*)())0,
		  dx_style, dx_vtext, dx_poly, dx_varc };

int	dxopen;			/* the ENTITIES section is open           */

static
dodxf()
{
	int gx0, gy0, gx1, gy1;

	xsc = 4;			/* grid units out, quarter-unit exact */

	xsc = 4;			/* grid units out, quarter-unit exact */

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "veldxf: nothing to export\n");
		return 1;
	}
	xorgx = gx0 * XSC;		/* the extent's min corner at 0,0 */
	xorgy = gy0 * XSC;
	dxymax = (gy1 - gy0) * XSC;
	if ( !dxopen )
	{
		printf("0\nSECTION\n2\nENTITIES\n");
		dxopen = 1;
	}
	xwalk(&dxfxb);
	return 0;
}

/* The whole -x pass: the sheets on the command line into ONE ENTITIES
 * section, the way a set prints onto one roll. */
dxfmain(argc, argv, first)
char **argv;
{
	register int i;
	int r;

	velprog = "veldxf";
	nsheets = argc - first;
	loadsyms();
	r = 0;
	for ( i = first; i < argc; i++ )
	{
		if ( loadsheet(argv[i]) < 0 )
			return 1;
		r |= dodxf();
	}
	if ( dxopen )
		printf("0\nENDSEC\n0\nEOF\n");
	return r;
}
