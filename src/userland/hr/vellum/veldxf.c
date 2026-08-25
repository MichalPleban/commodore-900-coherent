/*
 * veldxf.c - DXF (R10 entity subset) -> vellum .d converter (VELLUM.md
 * sec. 36): the office PC runs AutoCAD, and refusing its files makes
 * Vellum an island.
 *
 *	veldxf [-layer name=n] [-scale N] file.dxf > file.d	(in)
 *	veldxf -x file.d ... > file.dxf				(out)
 *
 * The two directions are one tool because they are one FORMAT,
 * and because the pair has to fix-point: the writer half lives in
 * veldxfo.c (the only half that links the model), this half reads.
 *
 * A headless binary of its own -- no velbase, no gfx: it parses DXF
 * group-code pairs and PRINTS .d lines.  Scope is the honest subset:
 * LINE -> L, CIRCLE -> C, ARC -> A (centre/radius/angles land exactly),
 * TEXT -> T at the nearest tail size, POLYLINE/LWPOLYLINE -> P (split
 * at the polyline point ceiling).  DXF layer names fold onto 0-3 by the
 * -layer map (a bare digit name folds onto itself); -scale is drawing
 * units PER GRID UNIT (default 1 -- one unit is a millimetre, and the
 * matching U header is emitted).  Everything else is SKIPPED AND
 * COUNTED -- the exit report says "dropped: 12 SPLINE, 3 INSERT",
 * because a silent partial import is how drawings lose limbs (the
 * -check lesson, applied at the border).
 *
 * Import is a CONVERSION, not a link: this runs once, the result is an
 * ordinary drawing, and nothing remembers where it came from.  Two
 * passes over the file: the first finds the accepted entities' extent
 * (DXF y is UP -- the flip needs the top), the second emits, the min
 * corner anchored at grid 0,0 so the export/import pair fix-points.
 */
#include <stdio.h>

#define	PMAXPT	20		/* vellum.h's polyline ceiling (no model  */
				/* header on purpose -- kept in step)     */
#define	MAXVTX	200		/* one DXF polyline's vertex buffer       */
#define	MAXDROP	20		/* distinct dropped entity names          */
#define	MAXLMAP	8		/* -layer name=n mappings                 */

long	atofix();

/* ---- fixed point: coordinates are longs x1000 (no floating point,
 * still) ---- */

long	sca	= 1000;		/* -scale: drawing units per grid unit    */

/* ---- the group-code pair reader ---- */

FILE	*in;
int	gcode;			/* current pair                           */
char	gval[80];

static
getpair()
{
	char cb[24];
	register int i;

	if ( fgets(cb, sizeof(cb), in) == 0 )
		return 0;
	gcode = atoi(cb);
	if ( fgets(gval, sizeof(gval), in) == 0 )
		return 0;
	for ( i = 0; gval[i]; i++ )
		if ( gval[i] == '\n' || gval[i] == '\r' )
		{
			gval[i] = 0;
			break;
		}
	return 1;
}

/* ---- layer folding ---- */

char	lmnm[MAXLMAP][16];
int	lmto[MAXLMAP];
int	nlmap;

static
foldlayer(nm)
register char *nm;
{
	register int i;

	for ( i = 0; i < nlmap; i++ )
		if ( strcmp(lmnm[i], nm) == 0 )
			return lmto[i];
	if ( nm[0] >= '0' && nm[0] <= '3' && nm[1] == 0 )
		return nm[0] - '0';
	return 0;
}

/* ---- dropped-entity accounting ---- */

char	dropnm[MAXDROP][14];
int	dropct[MAXDROP];
int	ndrop;

static
drop(nm)
register char *nm;
{
	register int i;

	for ( i = 0; i < ndrop; i++ )
		if ( strcmp(dropnm[i], nm) == 0 )
		{
			dropct[i]++;
			return 0;
		}
	if ( ndrop < MAXDROP )
	{
		strncpy(dropnm[ndrop], nm, 13);
		dropnm[ndrop][13] = 0;
		dropct[ndrop] = 1;
		ndrop++;
	}
	return 0;
}

/* ---- one parsed entity: the groups the subset needs ---- */

long	e10, e20, e11, e21, e40, e50, e51;
char	e1[76];			/* the TEXT string                        */
char	e8[16];			/* the layer name                         */
long	vx[MAXVTX], vy[MAXVTX];	/* POLYLINE/LWPOLYLINE vertices           */
int	nvx;

/* ---- pass state ---- */

int	pass;			/* 1 = extent, 2 = emit                   */
long	minx, maxy;		/* accepted-entity extent (DXF units)     */
int	gotext;

static
extpt(x, y)
long x, y;
{
	if ( !gotext )
	{
		minx = x;
		maxy = y;
		gotext = 1;
		return 0;
	}
	if ( x < minx ) minx = x;
	if ( y > maxy ) maxy = y;
	return 0;
}

/* DXF x -> grid x (min corner at 0; y flipped -- DXF y is up) */
static
gx(x)
long x;
{
	x -= minx;
	return (int)((x + (x < 0 ? -sca / 2 : sca / 2)) / sca);
}

static
gy(y)
long y;
{
	long v;

	v = maxy - y;
	return (int)((v + (v < 0 ? -sca / 2 : sca / 2)) / sca);
}

static
grd(v)			/* a length (radius, height): no flip, no origin */
long v;
{
	return (int)((v + sca / 2) / sca);
}

/* ---- the entity emitters (pass 2; pass 1 only extends the extent) ---- */

static
e_line()
{
	extpt(e10, e20);
	extpt(e11, e21);
	if ( pass == 2 )
		printf("L %d %d %d %d /0.%d\n", gx(e10), gy(e20),
		       gx(e11), gy(e21), foldlayer(e8));
	return 0;
}

static
e_circle()
{
	extpt(e10 - e40, e20 - e40);
	extpt(e10 + e40, e20 + e40);
	if ( pass == 2 )
		printf("C %d %d %d %d /0.%d\n", gx(e10), gy(e20),
		       gx(e10) + grd(e40), gy(e20), foldlayer(e8));
	return 0;
}

static
e_arc()
{
	register int a0, a1;

	extpt(e10 - e40, e20 - e40);
	extpt(e10 + e40, e20 + e40);
	if ( pass != 2 )
		return 0;
	/* our angles are degrees CCW y-up -- exactly DXF's convention,
	 * and the whole-drawing y flip preserves them */
	a0 = (int)(e50 / 1000) % 360;
	a1 = (int)(e51 / 1000) % 360;
	if ( a0 < 0 ) a0 += 360;
	if ( a1 < 0 ) a1 += 360;
	printf("A %d %d %d %d %d /0.%d\n", gx(e10), gy(e20), grd(e40),
	       a0, a1, foldlayer(e8));
	return 0;
}

static
e_text()
{
	register int sz;
	long h;

	extpt(e10, e20);
	extpt(e10 + e40, e20 + e40);
	if ( pass != 2 )
		return 0;
	if ( e1[0] == 0 )
		return 0;
	/* height (drawing units) -> grid units -> the nearest tail size
	 * (cells 8/15/16 px = 1/2/2 units; the split is at 1.2 and 1.9) */
	h = e40 * 1000 / sca;
	sz = h <= 1200 ? 0 : h <= 1900 ? 1 : 2;
	/* DXF 10/20 is the BASELINE left end; our T anchor is the cell
	 * top -- one cell up in grid terms */
	printf("T %d %d s%d /0.%d %s\n", gx(e10),
	       gy(e20) - (sz == 0 ? 1 : 2), sz, foldlayer(e8), e1);
	return 0;
}

static
e_poly()
{
	register int i, n, at;

	for ( i = 0; i < nvx; i++ )
		extpt(vx[i], vy[i]);
	if ( pass != 2 || nvx < 2 )
		return 0;
	/* split at the polyline ceiling; pieces share their seam point */
	at = 0;
	while ( at < nvx - 1 )
	{
		n = nvx - at;
		if ( n > PMAXPT )
			n = PMAXPT;
		printf("P %d", n);
		for ( i = at; i < at + n; i++ )
			printf(" %d %d", gx(vx[i]), gy(vy[i]));
		printf(" /0.%d\n", foldlayer(e8));
		at += n - 1;
	}
	return 0;
}

/* ---- the entity parser: from a "0 <NAME>" pair to the next "0" ---- */

static
clearent()
{
	e10 = e20 = e11 = e21 = e40 = e50 = e51 = 0;
	e1[0] = 0;
	strcpy(e8, "0");
	nvx = 0;
	return 0;
}

/* read this entity's groups; returns while gcode==0 (the next entity)
 * or at EOF.  lw = LWPOLYLINE: 10/20 pairs accumulate as vertices. */
static
readent(lw)
{
	while ( getpair() )
	{
		if ( gcode == 0 )
			return 1;
		switch ( gcode )
		{
		case 10:
			if ( lw && nvx < MAXVTX )
			{
				vx[nvx] = atofix(gval);
				nvx++;	/* 20 completes it below */
				break;
			}
			e10 = atofix(gval);
			break;
		case 20:
			if ( lw && nvx > 0 )
			{
				vy[nvx - 1] = atofix(gval);
				break;
			}
			e20 = atofix(gval);
			break;
		case 11:	e11 = atofix(gval);	break;
		case 21:	e21 = atofix(gval);	break;
		case 40:	e40 = atofix(gval);	break;
		case 50:	e50 = atofix(gval);	break;
		case 51:	e51 = atofix(gval);	break;
		case 1:
			strncpy(e1, gval, sizeof(e1) - 1);
			e1[sizeof(e1) - 1] = 0;
			break;
		case 8:
			strncpy(e8, gval, sizeof(e8) - 1);
			e8[sizeof(e8) - 1] = 0;
			break;
		}
	}
	return 0;
}

/* the old-style POLYLINE: VERTEX entities until SEQEND */
static
readpoly()
{
	int more;

	more = readent(0);		/* the POLYLINE's own groups */
	while ( more && strcmp(gval, "VERTEX") == 0 )
	{
		char sl[16];

		strcpy(sl, e8);		/* the VERTEX overwrites e8/e10 */
		more = readent(0);
		if ( nvx < MAXVTX )
		{
			vx[nvx] = e10;
			vy[nvx] = e20;
			nvx++;
		}
		strcpy(e8, sl);
	}
	if ( more && strcmp(gval, "SEQEND") == 0 )
		more = readent(0);
	return more;
}

/* ---- one pass over the file ---- */

static
dopass()
{
	int insec, more;
	char ent[20];

	insec = 0;
	rewind(in);
	if ( !getpair() )
		return 0;
	for (;;)
	{
		if ( gcode != 0 )	/* hunt the next 0 group */
		{
			if ( !getpair() )
				break;
			continue;
		}
		if ( strcmp(gval, "SECTION") == 0 )
		{
			if ( getpair() && gcode == 2 )
				insec = strcmp(gval, "ENTITIES") == 0;
			if ( !getpair() )
				break;
			continue;
		}
		if ( strcmp(gval, "ENDSEC") == 0 || strcmp(gval, "EOF") == 0 )
		{
			insec = 0;
			if ( !getpair() )
				break;
			continue;
		}
		if ( !insec )
		{
			if ( !getpair() )
				break;
			continue;
		}
		strncpy(ent, gval, sizeof(ent) - 1);
		ent[sizeof(ent) - 1] = 0;
		clearent();
		if ( strcmp(ent, "POLYLINE") == 0 )
			more = readpoly();
		else
			more = readent(strcmp(ent, "LWPOLYLINE") == 0);
		if ( strcmp(ent, "LINE") == 0 )
			e_line();
		else if ( strcmp(ent, "CIRCLE") == 0 )
			e_circle();
		else if ( strcmp(ent, "ARC") == 0 )
			e_arc();
		else if ( strcmp(ent, "TEXT") == 0 )
			e_text();
		else if ( strcmp(ent, "POLYLINE") == 0 ||
			  strcmp(ent, "LWPOLYLINE") == 0 )
			e_poly();
		else if ( pass == 1 )
			drop(ent);
		if ( !more )
			break;
	}
	return 0;
}

/* "[-]digits[.digits]" -> long x1000 */
long
atofix(s)
register char *s;
{
	register long v;
	register int neg, i;

	while ( *s == ' ' || *s == '\t' )
		s++;
	neg = 0;
	if ( *s == '-' )
	{
		neg = 1;
		s++;
	}
	v = 0;
	while ( *s >= '0' && *s <= '9' )
		v = v * 10 + (*s++ - '0');
	v *= 1000;
	if ( *s == '.' )
	{
		s++;
		for ( i = 100; i >= 1 && *s >= '0' && *s <= '9'; i /= 10 )
			v += (long)(*s++ - '0') * i;
	}
	return neg ? -v : v;
}

main(argc, argv)
char **argv;
{
	register int i;
	register char *p;

	/* -x is the OTHER direction, and it takes a sheet SET: hand the
	 * rest of the line to the writer half and be done. */
	if ( argc > 2 && strcmp(argv[1], "-x") == 0 )
		exit(dxfmain(argc, argv, 2));
	for ( i = 1; i < argc && argv[i][0] == '-'; i++ )
	{
		if ( strcmp(argv[i], "-scale") == 0 && i + 1 < argc )
		{
			sca = atofix(argv[++i]);
			if ( sca <= 0 )
				sca = 1000;
		}
		else if ( strcmp(argv[i], "-layer") == 0 && i + 1 < argc &&
			  nlmap < MAXLMAP )
		{
			i++;
			for ( p = argv[i]; *p && *p != '='; p++ )
				;
			if ( *p == '=' )
			{
				*p++ = 0;
				strncpy(lmnm[nlmap], argv[i], 15);
				lmnm[nlmap][15] = 0;
				lmto[nlmap] = atoi(p) & 3;
				nlmap++;
			}
		}
		else
			break;
	}
	if ( i != argc - 1 )
	{
		fprintf(stderr,
		    "usage: veldxf [-layer name=n] [-scale N] file.dxf\n");
		fprintf(stderr, "       veldxf -x file.d ...\n");
		exit(1);
	}
	if ( (in = fopen(argv[i], "r")) == (FILE *)0 )
	{
		fprintf(stderr, "veldxf: cannot open %s\n", argv[i]);
		exit(1);
	}
	pass = 1;
	dopass();
	printf("vellum1\n");
	printf("U %d mm\n", (int)((sca + 500) / 1000));
	if ( gotext )
	{
		pass = 2;
		dopass();
	}
	fclose(in);
	if ( ndrop )
	{
		fprintf(stderr, "veldxf: dropped:");
		for ( i = 0; i < ndrop; i++ )
			fprintf(stderr, "%s %d %s", i ? "," : "",
				dropct[i], dropnm[i]);
		fprintf(stderr, "\n");
	}
	exit(0);
}
