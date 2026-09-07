/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * velsym.c - velsym: a DRAWING becomes a STENCIL (VELLUM.md sec. 55).
 * What the shop draws becomes what the shop draws with.
 *
 *	velsym [-pfx P] [-org x,y] [-scale n] [-type NAME=t]  *		CODE sketch.d >> lib.sym
 *
 * A stencil SKETCH is an ordinary drawing -- it opens in the editor,
 * prints, and takes the whole revision workflow -- so a library is a
 * BUILD PRODUCT with a Makefile and a gate on it (see
 * /usr/vellum/etc/library.mk, whose worked example rebuilds the stock
 * pid library byte for byte from nine sketches).
 *
 * The sketch's L/C/A/B/T objects become the stencil's e/c/a/t ops in
 * quarter-grid symbol space, its N markers become PINS (named, and
 * typed by -type), and everything the stencil format cannot say is
 * SKIPPED AND COUNTED on stderr -- veldxf's border lesson, applied to
 * the library.
 */
#include <stdio.h>
#include "vellum.h"

/* ================================================================== */
/* a drawing becomes a stencil (sec. 55)                             */
/* ================================================================== */

/* the command line's half, filled by main()'s option loop */
char	*mkcode;		/* the stencil's file CODE                */
char	*mkpfx = "-";		/* -pfx: the designator prefix            */
int	mkorgx, mkorgy;		/* -org x,y: the grid point that is 0,0   */
int	mkorgf;			/* ... was given                          */
int	mksc = 1;		/* -scale n: the sketch is n x oversize   */

#define	MKTYPE	16		/* -type NAME=t entries                   */
static char	mktnm[MKTYPE][NAMEL];
static char	mktty[MKTYPE];
static int	nmktype;

/* one -type NAME=t; returns 0 when the argument does not parse */
mktype(s)
register char *s;
{
	register char *p;
	register int n;

	for ( p = s; *p && *p != '='; p++ )
		;
	if ( *p != '=' || p == s || p[1] == 0 || p[2] != 0 ||
	     nmktype >= MKTYPE )
		return 0;
	for ( n = 0; n < NAMEL - 1 && s + n < p; n++ )
		mktnm[nmktype][n] = s[n];
	mktnm[nmktype][n] = 0;
	mktty[nmktype] = p[1];
	nmktype++;
	return 1;
}

static
mktlook(nm)
char *nm;
{
	register int i;

	for ( i = 0; i < nmktype; i++ )
		if ( strcmp(mktnm[i], nm) == 0 )
			return mktty[i];
	return 0;
}

/* grid -> quarter-grid.  A stencil is in quarter units and a drawing
 * in whole ones, so the conversion is x4 and every pin lands on a
 * multiple of 4 -- exactly the constraint loadlib wants.  -scale n
 * divides instead, for a stencil sketched n x oversize so its detail
 * was drawable at all; at -scale 4 a drawing unit IS a quarter unit. */
static
mqx(g)
{
	register int v;

	v = (g - mkorgx) * 4;
	return v >= 0 ? (v + mksc / 2) / mksc : -((-v + mksc / 2) / mksc);
}

static
mqy(g)
{
	register int v;

	v = (g - mkorgy) * 4;
	return v >= 0 ? (v + mksc / 2) / mksc : -((-v + mksc / 2) / mksc);
}

static
mqr(g)				/* a radius: a length, not a position     */
{
	register long v;

	v = (long)g * 4;
	return (int)((v + mksc / 2) / mksc);
}

static
mkseg(x0, y0, x1, y1)
{
	printf("s %d %d %d %d\n", mqx(x0), mqy(y0), mqx(x1), mqy(y1));
	return 0;
}

/* Everything a stencil has no form for is SKIPPED AND COUNTED, on
 * stderr -- veldxf's border lesson for the third time: a silently
 * half-converted stencil is a part that draws wrong forever. */
#define	MKDROP	5
static char	*mkdnm[MKDROP] = {
	"shapes", "connectors", "dimensions", "symbols", "long texts"
};
static int	mkdct[MKDROP];

domksym(fn)
char *fn;
{
	register DOBJ *o;
	register int i, k;
	int gx0, gy0, gx1, gy1, npin, got;
	char *nm;
	long r;

	for ( i = 0; i < MKDROP; i++ )
		mkdct[i] = 0;
	/* the origin: -org, else the FIRST PIN (a stencil's origin is
	 * where it snaps), else the printable drawing's top-left */
	if ( !mkorgf )
	{
		got = 0;
		for ( i = 0; i < nobj; i++ )
			if ( obj[i].o_type == OT_NNAME &&
			     xprn((int)obj[i].o_layer) )
			{
				mkorgx = obj[i].o_x;
				mkorgy = obj[i].o_y;
				got = 1;
				break;
			}
		if ( !got )
		{
			if ( xextent(&gx0, &gy0, &gx1, &gy1) )
			{
				mkorgx = gx0;
				mkorgy = gy0;
			}
			else
				mkorgx = mkorgy = 0;
		}
	}
	printf("symbol %s %s\n", mkcode, mkpfx[0] ? mkpfx : "-");
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( !xprn((int)o->o_layer) )
			continue;
		switch ( o->o_type )
		{
		case OT_LINE:
		case OT_WIRE:
			mkseg((int)o->o_x, (int)o->o_y, (int)o->o_x2,
			      (int)o->o_y2);
			break;

		case OT_BOX:		/* four honest lines beat a new op */
			mkseg((int)o->o_x, (int)o->o_y, (int)o->o_x2,
			      (int)o->o_y);
			mkseg((int)o->o_x2, (int)o->o_y, (int)o->o_x2,
			      (int)o->o_y2);
			mkseg((int)o->o_x2, (int)o->o_y2, (int)o->o_x,
			      (int)o->o_y2);
			mkseg((int)o->o_x, (int)o->o_y2, (int)o->o_x,
			      (int)o->o_y);
			break;

		case OT_POLY:
			for ( k = 1; k < o->o_sym; k++ )
				mkseg((int)ppool[o->o_x2 + 2*k - 2],
				      (int)ppool[o->o_x2 + 2*k - 1],
				      (int)ppool[o->o_x2 + 2*k],
				      (int)ppool[o->o_x2 + 2*k + 1]);
			break;

		case OT_CIRC:
			r = xsqrt((long)(o->o_x2 - o->o_x) *
				  (o->o_x2 - o->o_x) +
				  (long)(o->o_y2 - o->o_y) *
				  (o->o_y2 - o->o_y));
			printf("c %d %d %d\n", mqx((int)o->o_x),
			       mqy((int)o->o_y), mqr((int)r));
			break;

		case OT_ARC:
			printf("a %d %d %d %d %d\n", mqx((int)o->o_x),
			       mqy((int)o->o_y), mqr((int)o->o_x2),
			       (int)OA0(o), (int)OA1(o));
			break;

		case OT_TEXT:	/* the stencil font is one glyph per op */
			if ( oval(o)[0] && oval(o)[1] == 0 )
				printf("t %d %d %c\n", mqx((int)o->o_x),
				       mqy((int)o->o_y), oval(o)[0]);
			else
				mkdct[4]++;
			break;

		case OT_SHAPE:	mkdct[0]++;	break;
		case OT_CONN:	mkdct[1]++;	break;
		case OT_DIM:	mkdct[2]++;	break;
		case OT_SYM:	mkdct[3]++;	break;
		}
	}
	/* Pins come from the N net-name markers, and the net name becomes
	 * the PIN name: an N at a point already means "this point is a
	 * terminal called VBUS".  Pin ORDER is their order in the file --
	 * z-order, which Front/Back edits and -spice reads.  Pin TYPES
	 * come from -type, never from the drawing (sec. 61). */
	npin = 0;
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_NNAME || !xprn((int)o->o_layer) )
			continue;
		k = mktlook(o->o_name);
		/* the .sym format spells "no name" as "-", and so does a
		 * sketch: an N marker called "-" is a terminal with no
		 * name, which is what an unnamed hand-cut pin is */
		nm = (o->o_name[0] == '-' && o->o_name[1] == 0) ? ""
								: o->o_name;
		printf("p %d %d", mqx((int)o->o_x), mqy((int)o->o_y));
		if ( nm[0] || k )
			printf(" %s", nm[0] ? nm : "-");
		if ( k )
			printf(" %c", k);
		printf("\n");
		npin++;
	}
	printf("end\n");
	got = 0;
	for ( i = 0; i < MKDROP; i++ )
		if ( mkdct[i] )
			fprintf(stderr, "%s %d %s", got++ ? "," :
				"velsym: -mksym: dropped:", mkdct[i],
				mkdnm[i]);
	if ( got )
		fprintf(stderr, "\n");
	if ( npin == 0 )
		fprintf(stderr,
	"velsym: -mksym: %s: no N markers, so the stencil has no pins\n",
			fn);
	return 0;
}


/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

main(argc, argv)
char **argv;
{
	register int i;

	velprog = "velsym";
	for ( i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++ )
	{
		if ( strcmp(argv[i], "-pfx") == 0 && i + 1 < argc )
			mkpfx = argv[++i];
		else if ( strcmp(argv[i], "-scale") == 0 && i + 1 < argc )
			mksc = atoi(argv[++i]);
		else if ( strcmp(argv[i], "-org") == 0 && i + 1 < argc )
		{
			register char *q;

			q = argv[++i];
			mkorgx = atoi(q);
			while ( *q && *q != ',' )
				q++;
			mkorgy = *q ? atoi(q + 1) : 0;
			mkorgf = 1;
		}
		else if ( strcmp(argv[i], "-type") == 0 && i + 1 < argc )
		{
			if ( !mktype(argv[++i]) )
			{
				fprintf(stderr, "velsym: -type wants NAME=t\n");
				exit(2);
			}
		}
		else
			break;
	}
	/* the stencil's CODE is not a file: it is the name the drawing
	 * will call this symbol by */
	if ( argc - i != 2 )
	{
		fprintf(stderr,
    "usage: velsym [-pfx P] [-org x,y] [-scale n] [-type NAME=t] CODE file.d\n");
		exit(2);
	}
	mkcode = argv[i];
	if ( mksc < 1 || mksc > 32 )
		mksc = 1;
	loadsyms();
	if ( loadsheet(argv[i + 1]) < 0 )
		exit(1);
	exit(domksym(argv[i + 1]));
}
