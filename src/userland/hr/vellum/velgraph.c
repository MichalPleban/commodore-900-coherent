/*
 * velgraph.c - x/y data -> a vellum drawing (VELLUM.md sec. 37): the
 * second user of this program was always make(1); the third is the
 * pipeline that ends in numbers.
 *
 *	velgraph [-t title] [-x label] [-y label] [-smooth] < data > g.d
 *
 * Reads "x y" pairs (blank-line-separated series, graph(1)'s
 * convention) and emits an ORDINARY drawing on stdout: axes on the
 * frame layer, 1-2-5 auto-scaled ticks with labels, each series a
 * styled polyline (solid, dashed, dotted, bold in turn); -smooth sets
 * the spline flag and the curve is real all the way to pic and
 * PostScript.  The output is a DRAWING: open it, annotate it,
 * dimension it, stamp it, print it -- a graph is not a special thing,
 * it is objects.
 *
 * No floating point, still: input decimals parse to fixed-point longs
 * (x1000); ranges, tick steps and labels are long arithmetic with the
 * decimal point re-inserted by the formatter.  The 1-2-5 ladder is
 * exact in fixed point, which is precisely why it is the 1-2-5 ladder
 * and not "nice numbers".
 *
 * Links velbase + velfile (the object model and fmtobj); no gfx.
 */
#include <stdio.h>
#include "vellum.h"

#define	MAXPT	280		/* data points, all series together       */
#define	NSER	12

/* the plot box, grid units on the 160x120 sheet */
#define	PX0	24
#define	PX1	148
#define	PY0	12
#define	PY1	92

long	dxv[MAXPT], dyv[MAXPT];
char	dser[MAXPT];
int	npt;
int	nser;

int	smoothf;
char	*title, *xlab, *ylab;

long	xmin, xmax, ymin, ymax;

/* ---- fixed point x1000 ---- */

static long
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

/* fixed -> text, trailing zeros trimmed */
static
fmtfix(v, b)
long v;
char *b;
{
	register char *p;
	long a;

	a = v < 0 ? -v : v;
	sprintf(b, "%s%ld", v < 0 ? "-" : "", a / 1000);
	if ( a % 1000 )
	{
		sprintf(b + strlen(b), ".%03ld", a % 1000);
		p = b + strlen(b);
		while ( p[-1] == '0' )
			*--p = 0;
	}
	return 0;
}

/* the 1-2-5 ladder: smallest step giving at most 6 intervals over r */
static long
step125(r)
long r;
{
	register long s;

	s = 1;
	for (;;)
	{
		if ( 6 * s >= r )
			return s;
		if ( 12 * s >= r )
			return 2 * s;
		if ( 30 * s >= r )
			return 5 * s;
		s *= 10;
	}
}

/* first tick at or above mn (C division truncates toward zero, so the
 * negative side needs no correction and the positive side rounds up) */
static long
tick0(mn, st)
long mn, st;
{
	return mn > 0 ? ((mn + st - 1) / st) * st : (mn / st) * st;
}

/* data -> grid, x across the plot box, y up the (screen-down) box */
static
mapx(x)
long x;
{
	long den;

	den = xmax - xmin;
	if ( den == 0 )
		return (PX0 + PX1) / 2;
	return PX0 + (int)(((x - xmin) * (PX1 - PX0) + den / 2) / den);
}

static
mapy(y)
long y;
{
	long den;

	den = ymax - ymin;
	if ( den == 0 )
		return (PY0 + PY1) / 2;
	return PY1 - (int)(((y - ymin) * (PY1 - PY0) + den / 2) / den);
}

/* ---- object building (velbase's model, emitted through fmtobj) ---- */

static
addo(type, x, y, x2, y2, fl, lay)
{
	register DOBJ *o;
	register int i;

	if ( nobj >= MAXOBJ )
		return -1;
	o = &obj[nobj];
	o->o_type = type;
	o->o_sym = 0;
	o->o_rot = o->o_mir = 0;
	o->o_x = x;
	o->o_y = y;
	o->o_x2 = x2;
	o->o_y2 = y2;
	o->o_flags = fl;
	o->o_layer = lay;
	o->o_grp = 0;
	for ( i = 0; i < NAMEL; i++ )
		o->o_name[i] = 0;
	o->o_val[0] = 0;
	return nobj++;
}

static
addtext(x, y, sz, fl, s)
char *s;
{
	register int i;

	i = addo(OT_TEXT, x, y, 0, 0, fl, 1);
	if ( i < 0 )
		return -1;
	obj[i].o_rot = sz;
	setoval(&obj[i], s);
	return i;
}

/* one series slice as a P object (points already in grid units) */
static
addpoly(px, py, n, fl)
register short *px, *py;
{
	register int i, k;

	if ( n < 2 || ppuse + 2 * n > PPOOL )
		return -1;
	i = addo(OT_POLY, px[0], py[0], ppuse, 0, fl, 0);
	if ( i < 0 )
		return -1;
	obj[i].o_sym = n;
	for ( k = 0; k < n; k++ )
	{
		ppool[ppuse++] = px[k];
		ppool[ppuse++] = py[k];
	}
	return i;
}

main(argc, argv)
char **argv;
{
	char lb[120], tb[24];
	register char *p, *t;
	register int i;
	int s, n, at;
	long step, v;
	short gx[MAXPT], gy[MAXPT];
	static char serfl[4] = { 0, OF_DASH, OF_DOT, OF_BOLD };

	title = xlab = ylab = (char *)0;
	for ( i = 1; i < argc; i++ )
	{
		if ( strcmp(argv[i], "-smooth") == 0 )
			smoothf = 1;
		else if ( strcmp(argv[i], "-t") == 0 && i + 1 < argc )
			title = argv[++i];
		else if ( strcmp(argv[i], "-x") == 0 && i + 1 < argc )
			xlab = argv[++i];
		else if ( strcmp(argv[i], "-y") == 0 && i + 1 < argc )
			ylab = argv[++i];
		else
		{
			fprintf(stderr,
	"usage: velgraph [-t title] [-x label] [-y label] [-smooth] < data\n");
			exit(1);
		}
	}

	/* ---- read the series ---- */
	s = 0;
	n = 0;			/* points in the current series */
	while ( fgets(lb, sizeof(lb), stdin) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 )
		{
			if ( n > 0 && s < NSER - 1 )	/* blank: next series */
			{
				s++;
				n = 0;
			}
			continue;
		}
		if ( npt >= MAXPT )
			continue;
		dxv[npt] = atofix(t);
		if ( (t = tok(&p)) == 0 )
			continue;
		dyv[npt] = atofix(t);
		dser[npt] = s;
		npt++;
		n++;
	}
	if ( npt < 2 )
	{
		fprintf(stderr, "velgraph: no data\n");
		exit(1);
	}
	nser = s + 1;
	xmin = xmax = dxv[0];
	ymin = ymax = dyv[0];
	for ( i = 1; i < npt; i++ )
	{
		if ( dxv[i] < xmin ) xmin = dxv[i];
		if ( dxv[i] > xmax ) xmax = dxv[i];
		if ( dyv[i] < ymin ) ymin = dyv[i];
		if ( dyv[i] > ymax ) ymax = dyv[i];
	}

	/* ---- the axes (frame layer) ---- */
	addo(OT_LINE, PX0, PY1, PX1, PY1, 0, 2);
	addo(OT_LINE, PX0, PY0, PX0, PY1, 0, 2);

	/* ---- ticks and labels, the 1-2-5 ladder ---- */
	step = step125(xmax - xmin);
	for ( v = tick0(xmin, step); v <= xmax; v += step )
	{
		i = mapx(v);
		addo(OT_LINE, i, PY1, i, PY1 + 2, 0, 2);
		fmtfix(v, tb);
		addtext(i - (int)strlen(tb) * 6 / 16, PY1 + 3, 0, 0, tb);
	}
	step = step125(ymax - ymin);
	for ( v = tick0(ymin, step); v <= ymax; v += step )
	{
		i = mapy(v);
		addo(OT_LINE, PX0 - 2, i, PX0, i, 0, 2);
		fmtfix(v, tb);
		addtext(PX0 - 3 - (int)strlen(tb) * 6 / 8, i - 1, 0, 0, tb);
	}

	/* ---- the series, one styled polyline each (split at PMAXPT,
	 * pieces sharing their seam point) ---- */
	for ( s = 0; s < nser; s++ )
	{
		register int fl;

		n = 0;
		for ( i = 0; i < npt; i++ )
		{
			if ( dser[i] != s )
				continue;
			gx[n] = mapx(dxv[i]);
			gy[n] = mapy(dyv[i]);
			n++;
		}
		fl = serfl[s & 3];
		if ( smoothf && n >= 3 )
			fl |= OF_SMOOTH;
		at = 0;
		while ( at < n - 1 )
		{
			i = n - at;
			if ( i > PMAXPT )
				i = PMAXPT;
			addpoly(&gx[at], &gy[at], i, fl);
			at += i - 1;
		}
	}

	/* ---- title and axis labels ---- */
	if ( title )
		addtext((PX0 + PX1) / 2 - (int)strlen(title) * 9 / 16,
			PY0 - 8, 2, 0, title);
	if ( xlab )
		addtext((PX0 + PX1) / 2 - (int)strlen(xlab) * 6 / 16,
			PY1 + 6, 0, 0, xlab);
	if ( ylab )
		addtext(2, (PY0 + PY1) / 2 - (int)strlen(ylab) * 6 / 16,
			0, OF_VERT, ylab);

	/* ---- emit: an ordinary drawing ---- */
	printf("vellum1\n");
	for ( i = 0; i < nobj; i++ )
	{
		fmtobj(i, lb);
		if ( lb[0] )
			printf("%s\n", lb);
	}
	exit(0);
}
