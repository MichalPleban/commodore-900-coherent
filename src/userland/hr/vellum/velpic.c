/*
 * velpic.c - velpic: a drawing as troff PIC SOURCE, so a sheet drops
 * into a document the way a figure does.
 *
 *	velpic file.d ... > fig.pic
 *
 * The scale is PINNED at 16 grid units to the inch (device px 8 per
 * unit, converted to hundredths of an inch here): a figure that
 * changed size when someone passed -scale would set differently in
 * every document that included it.
 */
#include <stdio.h>
#include "vellum.h"

/* ================================================================== */
/* troff pic source (device px -> inches at 16 units/inch)           */
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
		  cb_style, (int (*)())0, cb_poly, (int (*)())0 };

static
dopic()
{
	xsc = 8;			/* pinned: 16 grid units per inch */
	xorgx = 0;
	xorgy = 0;
	picymax = SHH * XSC;
	printf(".PS\n");
	xwalk(&picxb);
	printf(".PE\n");
	return 0;
}

/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

main(argc, argv)
char **argv;
{
	register int i;
	int r;

	velprog = "velpic";
	if ( argc < 2 )
	{
		fprintf(stderr, "usage: velpic file.d ...\n");
		exit(2);
	}
	nsheets = argc - 1;
	loadsyms();
	r = 0;
	for ( i = 1; i < argc; i++ )
	{
		if ( loadsheet(argv[i]) < 0 )
			exit(1);
		r |= dopic();
	}
	exit(r);
}
