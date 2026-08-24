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

/* The walker, its geometry and the XB backend contract live in velwalk.c
 * now (shared with the velprev preview window -- VELLUM.md sec. 25); this
 * file keeps the exporter BACKENDS (print / pic / hpgl / bom / net) and
 * the entry point. */
int	nsheets;		/* the sheet SET on the command line      */

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
		    pb_vtext, (int (*)())0, (int (*)())0 };

int	fitf;			/* -fit: fill the page (print: the        */
				/* discrete ladder; ps: + a PostScript-   */
				/* side remainder scale)                  */

/* ---- -tile: the sheet bigger than the paper (VELLUM.md sec. 58) ----
 * A drawing office tapes pages together.  The chosen backend runs ONCE
 * PER PAGE with the walker's device origin stepped -- xorgx/xorgy
 * already exist and already carry the used extent, so a page is an
 * origin and a size, and there is no new rendering path.  One grid unit
 * of OVERLAP on each seam gives the tape something to align on; the crop
 * marks and the seam label are drawn with the backend's OWN b_line and
 * b_text, so the XB struct does not grow (v4.1's promise kept). */
#define	TLABH	12		/* margin strip for the seam label, px    */
#define	TCROP	8		/* crop-mark tick length, px              */
#define	PGLONG	1920		/* the Epson's page down the paper, dots  */

int	tilef;			/* -tile                                  */
int	tilen;			/* -tile -n: count the pages and stop     */
static int	tpw, tph;	/* the page's CONTENT box, device px      */
static int	trow, tcol;	/* the page being drawn, 0-based          */
static int	tnr, tnc;	/* rows and columns of pages              */

/* the seam furniture of the page just walked */
static
tfurn(xb)
register XB *xb;
{
	char lb[40];

	if ( !tilef )
		return 0;
	(*xb->b_style)(0);
	(*xb->b_line)(0, 0, TCROP, 0);
	(*xb->b_line)(0, 0, 0, TCROP);
	(*xb->b_line)(tpw - TCROP, 0, tpw, 0);
	(*xb->b_line)(tpw, 0, tpw, TCROP);
	(*xb->b_line)(0, tph - TCROP, 0, tph);
	(*xb->b_line)(0, tph, TCROP, tph);
	(*xb->b_line)(tpw - TCROP, tph, tpw, tph);
	(*xb->b_line)(tpw, tph - TCROP, tpw, tph);
	sprintf(lb, "%d/%d row %c col %d", trow * tnc + tcol + 1, tnr * tnc,
		'A' + trow, tcol + 1);
	(*xb->b_text)(0, tph + 2, 0, lb);
	return 0;
}

/* ONE PAGE of Epson raster, banded into 24-row head passes with the
 * origin already stepped.  pw/ph are the page in DRAWING device px;
 * -wide exchanges them on the way to the head (bset's transform). */
static
prpage(pw, ph)
{
	register int b, x, r;
	int hdots, nb;

	devw = pw;
	bandw = widef ? ph : pw;	/* -wide: x/y exchanged */
	if ( bandw > MAXDOTS )
		bandw = MAXDOTS;
	bandwb = (bandw + 7) / 8;
	hdots = widef ? pw : ph;
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
		tfurn(&printxb);
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

static
doprint()
{
	int gx0, gy0, gx1, gy1, r;

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "vellum: nothing to print\n");
		return 1;
	}
	if ( fitf )
	{
		/* the largest scale on the discrete 2..32 ladder whose
		 * printed width still fits the head */
		r = (widef ? gy1 - gy0 : gx1 - gx0) + 2;
		xsc = MAXDOTS / r;
		if ( xsc > 32 ) xsc = 32;
		if ( xsc < 2 ) xsc = 2;
	}
	xorgx = (gx0 - 1) * XSC;
	xorgy = (gy0 - 1) * XSC;
	/* the live bug -print has carried since v1.2: the head is 960 dots
	 * and the default sheet is 160 units wide.  It is no longer SILENT,
	 * and -tile is the answer that keeps the edge (sec. 58). */
	r = ((widef ? gy1 - gy0 : gx1 - gx0) + 2) * XSC;
	if ( r > MAXDOTS )
		fprintf(stderr,
	"vellum: print: %d dots wide, the head prints %d -- use -tile or -fit\n",
			r, MAXDOTS);
	return prpage((gx1 - gx0 + 2) * XSC, (gy1 - gy0 + 2) * XSC);
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
		  cb_style, (int (*)())0, cb_poly, (int (*)())0 };

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
int	hppen;			/* pen in the holder (pen-by-layer, v4.0: */
				/* annotation draws in the second pen --  */
				/* the cheapest two-color printing this   */
				/* machine will ever do)                  */

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

	s = (xlay == 1) ? 2 : 1;
	if ( s != hppen )
	{
		printf("SP%d;", s);
		hppen = s;
	}
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
		   hb_style, hb_vtext, (int (*)())0, (int (*)())0 };

/* ONE PAGE of HP-GL, with the origin already stepped: the plotter's own
 * page is fixed, so -tile steps a big drawing across several of them. */
static
hpone(pw, ph)
{
	hymax = ph;
	hpsty = 0;
	hppen = 1;
	printf("IN;SP1;LT;\n");
	if ( tilef )			/* clip the walk to this page */
		printf("IW0,%d,%d,%d;\n", TLABH * 4, tpw * 4,
		       (tph + TLABH) * 4);
	xwalk(&hpglxb);
	if ( tilef )
		printf("IW;\n");	/* ... the furniture rides outside it */
	tfurn(&hpglxb);
	printf("PU0,0;SP0;\n");
	return 0;
}

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
	return hpone((gx1 - gx0 + 2) * XSC, (gy1 - gy0 + 2) * XSC);
}

/* ================================================================== */
/* -ps: PostScript Level 1 -- the laser release (VELLUM.md sec. 35).   */
/* User space is DEVICE PX: the prolog scales 16 grid units per inch   */
/* onto the page (72/128 pt per px at the pinned xsc 8), y flipped per */
/* sheet like hpgl.  Text is Courier sized so the MONOSPACE ADVANCE    */
/* matches the editor's cell (6/8/9 px -> ~6/8/10 pt on paper):        */
/* nothing collides on the page that did not collide on screen, which  */
/* is the whole reason it is Courier and not Times.  Smooth polylines  */
/* emit real curveto through the same midpoint control points the pic  */
/* splines and the chord walker use; vertical text is a -90 rotate     */
/* around the anchor -- the transpose loop stays the bitmap printers'  */
/* problem.  One %%Page per sheet, showpage between.                   */
/* ================================================================== */

int	pssty;
int	psymax;			/* device y of the sheet bottom (flip)    */
int	pspage;			/* %%Page counter across the sheet set    */
int	psfont;			/* size set by the last selectfont (-1)   */

static
ps_y(y)
{
	return psymax - y;
}

static
ps_style(fl)
{
	if ( (fl & (OF_STYLE | OF_BOLD)) == (pssty & (OF_STYLE | OF_BOLD)) )
		return 0;
	if ( (fl & OF_STYLE) == OF_DASH )
		printf("[4 4] 0 setdash ");
	else if ( (fl & OF_STYLE) == OF_DOT )
		printf("[1 2] 0 setdash ");
	else
		printf("[] 0 setdash ");
	printf("%d setlinewidth\n", (fl & OF_BOLD) ? 2 : 1);
	pssty = fl;
	return 0;
}

static
ps_line(x0, y0, x1, y1)
{
	printf("%d %d %d %d L\n", x0, ps_y(y0), x1, ps_y(y1));
	return 0;
}

/* Paint the current path per the walker's fill (-1 outline only; else
 * setgray fill first, then the outline) -- z-order composes exactly as
 * on screen.  Hatch prints as the same 0.5 gray pic uses. */
static
ps_paint(fill)
{
	if ( fill >= 0 )
		printf(" gsave %s setgray fill grestore",
		       fill == 0 ? "0" : fill == 1 ? "1" : ".5");
	printf(" stroke\n");
	return 0;
}

static
ps_box(x0, y0, x1, y1, fill)
{
	printf("%d %d %d %d BX", x0, ps_y(y0), x1, ps_y(y1));
	ps_paint(fill);
	return 0;
}

static
ps_circle(cx, cy, r, fill)
{
	printf("newpath %d %d %d 0 360 arc closepath", cx, ps_y(cy), r);
	ps_paint(fill);
	return 0;
}

/* a TRUE arc (b_varc): our angles are degrees CCW y-up, which the per-
 * sheet flip maps exactly onto PostScript's arc convention */
static
ps_varc(cx, cy, r, a0, a1)
{
	while ( a1 <= a0 )
		a1 += 360;
	printf("newpath %d %d %d %d %d arc stroke\n",
	       cx, ps_y(cy), r, a0, a1);
	return 0;
}

/* (s) with the PS specials escaped */
static
ps_str(s)
register char *s;
{
	putchar('(');
	for ( ; *s; s++ )
	{
		if ( *s == '(' || *s == ')' || *s == '\\' )
			putchar('\\');
		putchar(*s);
	}
	putchar(')');
	return 0;
}

/* font sizes in device px, chosen so 0.6 x size = the editor cell */
static short	psfsz[3] = { 10, 13, 15 };

static
ps_setf(sz)
{
	if ( sz < 0 ) sz = 0;
	if ( sz > 2 ) sz = 2;
	if ( sz != psfont )
	{
		printf("/Courier findfont %d scalefont setfont\n", psfsz[sz]);
		psfont = sz;
	}
	return sz;
}

/* y is the CELL TOP (the walker's convention); baseline near its foot */
static short	psbase[3] = { 7, 12, 13 };

static
ps_text(x, y, sz, s)
char *s;
{
	sz = ps_setf(sz);
	printf("%d %d moveto ", x, ps_y(y + psbase[sz]));
	ps_str(s);
	printf(" show\n");
	return 0;
}

static
ps_vtext(x, y, sz, s)
char *s;
{
	char lb[TVMAX];
	register char *e;
	register int n;
	int ch;

	sz = ps_setf(sz);
	ch = sz == 0 ? 8 : sz == 1 ? 15 : 16;
	for (;;)
	{
		for ( e = s, n = 0; *e && *e != '|'; e++ )
			lb[n++] = *e;
		lb[n] = 0;
		printf("gsave %d %d translate -90 rotate 0 0 moveto ",
		       x + psbase[sz], ps_y(y));
		ps_str(lb);
		printf(" show grestore\n");
		if ( *e == 0 )
			break;
		s = e + 1;
		x += ch;
	}
	return 0;
}

/* smooth polylines as real curveto: the quadratic (a, c, b) knots the
 * chord walker subdivides, lifted to cubics with integer thirds */
static
ps_curve(ax, ay, cx, cy, bx, by)
{
	printf("%d %d %d %d %d %d curveto\n",
	       (ax + 2 * cx) / 3, ps_y((ay + 2 * cy) / 3),
	       (bx + 2 * cx) / 3, ps_y((by + 2 * cy) / 3),
	       bx, ps_y(by));
	return 0;
}

static
ps_poly(xy, n)
register int *xy;
{
	register int i;
	int ax, ay, bx, by;

	if ( n < 2 )
		return 0;
	printf("newpath %d %d moveto\n", xy[0], ps_y(xy[1]));
	if ( n == 2 )
		printf("%d %d lineto\n", xy[2], ps_y(xy[3]));
	ax = xy[0];
	ay = xy[1];
	for ( i = 1; i < n - 1; i++ )
	{
		if ( i == n - 2 )
		{
			bx = xy[2*n - 2];
			by = xy[2*n - 1];
		}
		else
		{
			bx = (xy[2*i] + xy[2*i + 2]) / 2;
			by = (xy[2*i + 1] + xy[2*i + 3]) / 2;
		}
		ps_curve(ax, ay, xy[2*i], xy[2*i + 1], bx, by);
		ax = bx;
		ay = by;
	}
	printf("stroke\n");
	return 0;
}

XB	psxb = { ps_line, ps_box, ps_circle, ps_text, (int (*)())0,
		 ps_style, ps_vtext, ps_poly, ps_varc };

/* ONE PAGE of PostScript, with the origin already stepped.  pw/ph are
 * the page in device px; -fit's page-fill ratio reads them back as grid
 * units, which is the same quantity it has always measured. */
static
psone(pw, ph)
{
	int w, h;

	psymax = ph;
	w = pw / XSC;			/* extent, grid units */
	h = ph / XSC;
	if ( pspage == 0 )
	{
		printf("%%!PS-Adobe-1.0\n");
		printf("%%%%Creator: vellum\n");
		printf("%%%%EndComments\n");
		printf("/L { newpath 4 2 roll moveto lineto stroke } def\n");
		printf("/BX { /by1 exch def /bx1 exch def /by0 exch def\n");
		printf("  /bx0 exch def newpath bx0 by0 moveto bx1 by0 lineto\n");
		printf("  bx1 by1 lineto bx0 by1 lineto closepath } def\n");
	}
	pspage++;
	printf("%%%%Page: %d %d\n", pspage, pspage);
	printf("gsave 36 36 translate\n");
	if ( fitf )
	{
		/* fill = min(523 / 4.5w, 770 / 4.5h) = min(1046/9w, 1540/9h) */
		if ( (long)1046 * h < (long)1540 * w )
			printf("1046 %d div dup scale\n", 9 * w);
		else
			printf("1540 %d div dup scale\n", 9 * h);
	}
	printf("72 %d div dup scale\n", 16 * XSC);
	printf("1 setlinecap 1 setlinewidth [] 0 setdash\n");
	pssty = 0;
	psfont = -1;
	/* -tile: CLIP the walk to this page.  The device clips anyway --
	 * paper has edges -- but a page that draws the whole drawing and
	 * lets the printer sort it out overprints its own crop marks, and
	 * the marks are the reason the page has a margin. */
	if ( tilef )
	{
		printf("gsave newpath 0 %d moveto %d %d lineto %d %d lineto\n",
		       TLABH, tpw, TLABH, tpw, tph + TLABH);
		printf("0 %d lineto closepath clip\n", tph + TLABH);
	}
	xwalk(&psxb);
	if ( tilef )
	{
		printf("grestore\n");	/* the state gsave saw: style 0, no font */
		pssty = 0;
		psfont = -1;
	}
	tfurn(&psxb);
	printf("grestore showpage\n");
	return 0;
}

/* One sheet = one page.  -fit adds a PostScript-side scale filling the
 * A4 usable box (523 x 770 pt inside 36 pt margins): the ratio is
 * emitted as a PS rational -- OUR side stays integer. */
static
dops()
{
	int gx0, gy0, gx1, gy1;

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "vellum: nothing to print\n");
		return 1;
	}
	xorgx = (gx0 - 1) * XSC;
	xorgy = (gy0 - 1) * XSC;
	return psone((gx1 - gx0 + 2) * XSC, (gy1 - gy0 + 2) * XSC);
}

/* ================================================================== */
/* -tile: the drawing across as many pages as it takes (sec. 58)      */
/* ================================================================== */

static
dotile(mode)
char *mode;
{
	int gx0, gy0, gx1, gy1, pw, ph, fw, fh, w, h, ovl, sx, sy, r;

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "vellum: nothing to print\n");
		return 1;
	}
	/* the PHYSICAL page of the chosen backend, device px */
	if ( strcmp(mode, "ps") == 0 )
	{
		xsc = 8;		/* pinned like pic: 16 units/inch */
		/* A4's usable box inside 36 pt margins: 16 units per inch
		 * is 16/72 px per point, written 2/9 because 523 * 16 * 8
		 * does not fit a 16-bit int */
		pw = 523 * XSC * 2 / 9;
		ph = 770 * XSC * 2 / 9;
	}
	else if ( strcmp(mode, "hpgl") == 0 )
	{
		pw = 10870 / 4;		/* A4 plotting area, HP-GL units  */
		ph = 7600 / 4;		/* ... at 4 plotter units per px  */
	}
	else if ( widef )
	{
		pw = PGLONG;		/* -wide: the head runs down the  */
		ph = MAXDOTS;		/* drawing's y instead            */
	}
	else
	{
		pw = MAXDOTS;		/* the head's printable width     */
		ph = PGLONG;
	}
	fw = pw;			/* the full CONTENT box: the page */
	fh = ph - TLABH;		/* less the seam label's margin   */
	ovl = XSC;			/* one grid unit of overlap       */
	w = (gx1 - gx0 + 2) * XSC;
	h = (gy1 - gy0 + 2) * XSC;
	sx = fw - ovl;
	sy = fh - ovl;
	if ( sx < ovl || sy < ovl )	/* a page smaller than its seam   */
	{
		fprintf(stderr, "vellum: -tile: the page is too small\n");
		return 1;
	}
	tnc = w <= fw ? 1 : (w - ovl + sx - 1) / sx;
	tnr = h <= fh ? 1 : (h - ovl + sy - 1) / sy;
	if ( tilen )			/* how many pages, and stop */
	{
		printf("%d page%s: %d row%s of %d\n", tnr * tnc,
		       tnr * tnc == 1 ? "" : "s", tnr, tnr == 1 ? "" : "s",
		       tnc);
		return 0;
	}
	r = 0;
	/* row-major, so the pile tapes up in reading order */
	for ( trow = 0; trow < tnr; trow++ )
		for ( tcol = 0; tcol < tnc; tcol++ )
		{
			/* the LAST row and column are trimmed to what is
			 * left of the drawing: nothing tapes to the outside
			 * edge, so a full blank page there is paper (and,
			 * for the Epson, minutes) spent on nothing */
			tpw = w - tcol * sx;
			if ( tpw > fw )
				tpw = fw;
			tph = h - trow * sy;
			if ( tph > fh )
				tph = fh;
			xorgx = (gx0 - 1) * XSC + tcol * sx;
			xorgy = (gy0 - 1) * XSC + trow * sy;
			xclipon = 1;
			xclipx0 = 0;
			xclipy0 = 0;
			xclipx1 = tpw;
			xclipy1 = tph;
			if ( strcmp(mode, "ps") == 0 )
				r |= psone(tpw, tph + TLABH);
			else if ( strcmp(mode, "hpgl") == 0 )
				r |= hpone(tpw, tph + TLABH);
			else
				r |= prpage(tpw, tph + TLABH);
		}
	xclipon = 0;
	return r;
}

/* ================================================================== */
/* -dxf: DXF (R10 entity subset) out -- the interchange release        */
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

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "vellum: nothing to export\n");
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
/* -check: prove the set (VELLUM.md sec. 26) -- the netlist's checks   */
/* promoted to a first-class pass and aimed at make(1): one finding    */
/* per line on stdout, exit status = the finding count, so a Makefile  */
/* gates on it the way cc gates on -Werror.                            */
/* ================================================================== */

int	checkf;			/* -check: report findings, print no nets */
int	chkn;			/* finding count = the exit status        */
char	*chksheet = "";		/* file name for the report lines         */

/* One finding: "file: message".  a/b ride printf %s/%d holes. */
static
chk(msg, a, b)
char *msg, *a, *b;
{
	printf("%s: ", chksheet);
	printf(msg, a, b);
	printf("\n");
	chkn++;
	return 0;
}

/* set-wide designators, for the across-sheets duplicate check */
#define	MAXDES	200
char	desnm[MAXDES][NAMEL];
char	dessh[MAXDES][14];
int	ndes;

/* per-sheet: duplicate designators (across the SET) and designated
 * parts with no value (the BOM's "-" rows, at the source) */
static
chkparts()
{
	register DOBJ *o;
	register int i, k;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
			continue;
		for ( k = 0; k < ndes; k++ )
			if ( strcmp(desnm[k], o->o_name) == 0 )
				break;
		if ( k < ndes )
			chk("duplicate designator %s (also in %s)",
			    o->o_name, dessh[k]);
		else if ( ndes < MAXDES )
		{
			strcpy(desnm[ndes], o->o_name);
			strncpy(dessh[ndes], chksheet, 13);
			dessh[ndes][13] = 0;
			ndes++;
		}
		if ( o->o_val[0] == 0 )
			chk("%s has no value", o->o_name, 0);
	}
	return 0;
}

/* per-sheet: dangling wire ends -- an endpoint touching no pin, no
 * other wire and no net-name marker is the classic drawing slip */
static
chkwires()
{
	register DOBJ *o;
	register int i, j;
	int e, px, py, ok;
	char pt[16];

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_WIRE )
			continue;
		for ( e = 0; e < 2; e++ )
		{
			px = e ? o->o_x2 : o->o_x;
			py = e ? o->o_y2 : o->o_y;
			ok = pinat(px, py);
			for ( j = 0; j < nobj && !ok; j++ )
			{
				if ( j == i )
					continue;
				if ( obj[j].o_type == OT_WIRE &&
				     xonwire(&obj[j], px, py) )
					ok = 1;
				else if ( obj[j].o_type == OT_NNAME &&
					  obj[j].o_x == px && obj[j].o_y == py )
					ok = 1;
			}
			if ( !ok )
			{
				sprintf(pt, "%d,%d", px, py);
				chk("dangling wire end at %s", pt, 0);
			}
		}
	}
	return 0;
}

/* per-sheet: symbol codes no loaded library defines.  loadfile SKIPS
 * unknown Y lines by design, so a hand-edited or generated file loses
 * parts silently -- this is where that surfaces.  Re-reads the raw
 * file: the skipped lines are, by definition, not in the model. */
static
chkcodes(fn)
char *fn;
{
	register FILE *fp;
	char lb[220];
	char *p, *t;

	if ( (fp = fopen(fn, "r")) == (FILE *)0 )
		return 0;
	while ( fgets(lb, sizeof(lb), fp) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 || t[0] != 'Y' || t[1] != 0 )
			continue;
		if ( (t = tok(&p)) == 0 )
			continue;
		if ( symbycode(t) < 0 )
			chk("unknown symbol %s (line dropped)", t, 0);
	}
	fclose(fp);
	return 0;
}

/* ================================================================== */
/* -net: union-find over wire endpoints, junctions and pins           */
/* ================================================================== */

short	wnet[MAXOBJ];		/* wire object -> net root (union-find)   */

/* nfind/nunion/xonwire/xpinpos and netbuild() below are NOT static: the
 * v5 asking modes (-len, -spice) are the same question asked twice, and
 * a second union-find would be a second answer (VELLUM.md sec. 46). */
nfind(i)
{
	while ( wnet[i] != i )
		i = wnet[i] = wnet[wnet[i]];
	return i;
}

nunion(a, b)
{
	a = nfind(a);
	b = nfind(b);
	if ( a != b )
		wnet[a] = b;
	return 0;
}

/* Is grid point (px,py) ON wire o (either leg)? */
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
#define	MAXNNAME (MAXOBJ / 4)	/* named nets one sheet can carry         */

/* Named nets MERGE across the sheet set (sec. 19): a net named VBUS on
 * sheet 1 is the same conductor as VBUS on sheet 3.  Their pins pool
 * here and print after the last sheet; unnamed nets stay sheet-local. */
#define	MAXMNET	24
char	mnname[MAXMNET][10];
char	mnpins[MAXMNET][120];
short	mncnt[MAXMNET];
short	mno[MAXMNET], mnp[MAXMNET];	/* ERC (v4.4): typed-pin counts   */
short	mni[MAXMNET], mnt[MAXMNET];	/* over the whole merged net      */
int	nmnet;

/* The ERC verdicts on one net's typed-pin counts (VELLUM.md sec. 38):
 * two outputs conflict, an output on a power net drives a rail, a net
 * whose typed pins are all inputs is undriven.  The rules fire ONLY
 * between typed pins -- an old library produces silence, not noise,
 * and a half-typed one checks exactly as far as it is typed. */
static
erc1(nm, no, npw, ni, nt)
char *nm;
{
	if ( no >= 2 )
		chk("net %s: driver conflict (%d outputs)", nm, no);
	if ( no >= 1 && npw >= 1 )
		chk("net %s: output drives a power rail", nm, 0);
	if ( nt >= 1 && ni == nt )
		chk("net %s: undriven (typed pins all inputs)", nm, 0);
	return 0;
}

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

/* After the last sheet: the merged named nets, and their pin check.
 * In -check mode nothing lists; a named net with fewer than two pins
 * across the WHOLE set is the finding -- which is also the bus rule
 * (sec. 28): a bus bit name appearing exactly once is a typo. */
static
donetend()
{
	register int i;

	for ( i = 0; i < nmnet; i++ )
	{
		if ( checkf )
		{
			if ( mncnt[i] < 2 )
				chk("named net %s has fewer than 2 pins in the set",
				    mnname[i], 0);
			erc1(mnname[i], (int)mno[i], (int)mnp[i],
			     (int)mni[i], (int)mnt[i]);
			continue;
		}
		printf("%s:%s\n", mnname[i], mnpins[i]);
		if ( mncnt[i] < 2 )
			fprintf(stderr,
				"vellum: warning: net %s has %d pin\n",
				mnname[i], mncnt[i]);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* netbuild(): the CONNECTIVITY, with nothing said about it -- wires    */
/* unioned, pins attached, nets named.  -net reports it, -check judges  */
/* it, and the v5 modes -len and -spice ask it their own questions      */
/* (VELLUM.md sec. 44 rule 1: every new verb is machinery that ships).  */
/* The results are FILE-SCOPE, not donet's locals, exactly so.          */
/* ------------------------------------------------------------------ */

short	pobj[MAXNPIN], ppin[MAXNPIN], pnet[MAXNPIN];
int	np;			/* pins found on this sheet               */
char	nnm[MAXNNAME][10];	/* net root -> name (GND/VCC/N)           */
short	nroot[MAXNNAME];
int	nnames;

netbuild()
{
	register DOBJ *o;
	register int i, j;
	int k, gx, gy, px, py;

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
	return 0;
}

static
donet(sheet)
{
	register DOBJ *o;
	register int i, j;
	int k;
	int to, tp, ti, tt;		/* ERC typed-pin counts, per net */
	int nid, cnt;
	short outed[MAXOBJ];

	netbuild();
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
					mno[m] = mnp[m] = 0;
					mni[m] = mnt[m] = 0;
					nmnet++;
				}
			}
		}
		else if ( !checkf )
		{
			if ( nsheets > 1 )
				printf("NET-%d.%d:", sheet, nid);
			else
				printf("NET-%d:", nid);
		}
		cnt = 0;
		to = tp = ti = tt = 0;
		for ( j = 0; j < np; j++ )
		{
			if ( pnet[j] != r )
				continue;
			o = &obj[pobj[j]];
			cnt++;		/* EVERY pin counts toward the
					 * single-pin test: a feeder segment
					 * from a breaker to a designator-less
					 * bus tap is two connections, not a
					 * loose end */
			if ( checkf )
			{
				/* ERC counts EVERY pin's type -- a GND
				 * stencil's 'p' names the rail even though
				 * it is never a listed pin */
				register int pt;

				pt = pintyp[PINSLOT(&symtab[o->o_sym],
						    (int)ppin[j])];
				if ( pt )
				{
					tt++;
					if ( pt == 'o' )	to++;
					else if ( pt == 'p' )	tp++;
					else if ( pt == 'i' )	ti++;
				}
			}
			if ( symtab[o->o_sym].sy_pfx[0] == 0 )
				continue;	/* ... but a designator-less
						 * stencil (rails, taps,
						 * off-page markers) only NAMES
						 * or carries the net -- it is
						 * never a LISTED pin */
			pinstr(o, (int)ppin[j], pb);
			if ( k < nnames )
			{
				if ( m >= 0 &&
				     strlen(mnpins[m]) + strlen(pb) <
				     sizeof(mnpins[0]) )
					strcat(mnpins[m], pb);
			}
			else if ( !checkf )
				printf("%s", pb);
		}
		if ( k < nnames )
		{
			if ( m >= 0 )
			{
				mncnt[m] += cnt;
				mno[m] += to;
				mnp[m] += tp;
				mni[m] += ti;
				mnt[m] += tt;
			}
		}
		else if ( checkf )
		{
			sprintf(pb, "NET-%d", nid);
			if ( cnt < 2 )
				chk("net %s has fewer than 2 pins", pb, 0);
			erc1(pb, to, tp, ti, tt);
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
			if ( checkf )
			{
				char nb[8];

				sprintf(nb, "%d", ppin[i] + 1);
				chk("%s pin %s unconnected",
				    o->o_name[0] ? o->o_name : "?", nb);
			}
			else
				fprintf(stderr,
					"vellum: warning: %s pin %d unconnected\n",
					o->o_name[0] ? o->o_name : "?",
					ppin[i] + 1);
		}
	return 0;
}

/* ================================================================== */
/* -renum: canonical designators (VELLUM.md sec. 38) -- symbols sorted */
/* by (y, x), numbered per prefix from -base N (default 1), the whole  */
/* drawing re-emitted through fmtobj to stdout.  Designators live      */
/* nowhere else in the format (nets are positional), so the rewrite    */
/* cannot dangle a reference.  ONE sheet per run: a set renumbers      */
/* sheet by sheet with -base carrying the count across -- the          */
/* Makefile owns the set, as always.                                   */
/* ================================================================== */

static
dorenum(base)
{
	register DOBJ *o;
	register int i, j;
	short ix[MAXOBJ];
	int n, k, t;
	char lb[220];

	n = 0;
	for ( i = 0; i < nobj; i++ )
		if ( obj[i].o_type == OT_SYM && obj[i].o_name[0] )
			ix[n++] = i;
	for ( i = 1; i < n; i++ )		/* insertion sort by (y, x) */
		for ( j = i; j > 0; j-- )
		{
			register DOBJ *a, *b;

			a = &obj[ix[j - 1]];
			b = &obj[ix[j]];
			if ( a->o_y < b->o_y ||
			     (a->o_y == b->o_y && a->o_x <= b->o_x) )
				break;
			t = ix[j];  ix[j] = ix[j - 1];  ix[j - 1] = t;
		}
	for ( i = 0; i < n; i++ )
	{
		register char *pfx;
		int cnt;

		o = &obj[ix[i]];
		pfx = symtab[o->o_sym].sy_pfx;
		if ( pfx[0] == 0 )
			continue;
		cnt = 0;
		for ( j = 0; j < i; j++ )
			if ( strcmp(symtab[obj[ix[j]].o_sym].sy_pfx,
				    pfx) == 0 )
				cnt++;
		sprintf(o->o_name, "%.3s%d", pfx, base + cnt);
	}
	/* re-emit the whole drawing (velfile writefile's shape, stdout) */
	printf("vellum1\n");
	if ( unum != 1 || uname[0] )
		printf("U %d %s\n", unum, uname[0] ? uname : "-");
	for ( i = 0; i < nobj; i++ )
	{
		if ( obj[i].o_grp &&
		     (i == 0 || obj[i - 1].o_grp != obj[i].o_grp) )
		{
			k = 0;
			for ( j = i; j < nobj &&
				     obj[j].o_grp == obj[i].o_grp; j++ )
				k++;
			printf("G %d\n", k);
		}
		fmtobj(i, lb);
		if ( lb[0] )
			printf("%s\n", lb);
	}
	return 0;
}

/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

/* Is `m' one of the modes this binary knows?  (The editor forwards ANY
 * lowercase -mode here without a relink -- the v2.5 decision that keeps
 * paying, and the one that makes every v5 verb cost the editor zero.) */
static
knownmode(m)
char *m;
{
	static char *mv[] = {
		"print", "pic", "net", "hpgl", "bom", "check", "ps", "dxf",
		"renum", "diff", "len", "where", "spice", "symsheet",
		"mksym", "symcheck", "book",
		(char *)0
	};
	register char **p;

	for ( p = mv; *p; p++ )
		if ( strcmp(*p, m) == 0 )
			return 1;
	return 0;
}

/* Every file argument is a SHEET; shell globs are the set syntax
 * (sec. 19): -print emits a form feed between sheets, -net merges
 * named nets across them, -bom yields ONE parts list for the design.
 * The v5 modes (sec. 44) take their own shapes -- two revisions for
 * -diff, a pattern then sheets for -where, one .sym for -symsheet --
 * and are dispatched from the same loop. */
velxport(argc, argv)
char **argv;
{
	register char *mode;
	register int i;
	int r, first, rbase, markf, v5;
	char *wpat;

	rbase = 1;
	markf = 0;
	wpat = "";
	mode = (char *)0;
	/* a bare "-" is a FILE (stdin), not an option (sec. 49) */
	for ( i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++ )
	{
		if ( strcmp(argv[i], "-wide") == 0 )
			widef = 1;
		else if ( strcmp(argv[i], "-fit") == 0 )
			fitf = 1;
		else if ( strcmp(argv[i], "-mark") == 0 )
			markf = 1;
		else if ( strcmp(argv[i], "-tile") == 0 )
			tilef = 1;
		else if ( strcmp(argv[i], "-n") == 0 )
			tilen = 1;
		else if ( strcmp(argv[i], "-scale") == 0 && i + 1 < argc )
		{
			xsc = atoi(argv[++i]);
			mksc = xsc;	/* -mksym DIVIDES by it (sec. 55) */
		}
		else if ( strcmp(argv[i], "-base") == 0 && i + 1 < argc )
			rbase = atoi(argv[++i]);
		/* -mksym carries its CODE, then more options, then the
		 * sketch: the stencil's name is not a file (sec. 55) */
		else if ( strcmp(argv[i], "-mksym") == 0 && i + 1 < argc )
		{
			mode = "mksym";
			mkcode = argv[++i];
		}
		else if ( strcmp(argv[i], "-pfx") == 0 && i + 1 < argc )
			mkpfx = argv[++i];
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
				fprintf(stderr,
				    "vellum: -type wants NAME=t\n");
				return 1;
			}
		}
		else if ( mode == (char *)0 )
			mode = argv[i] + 1;
		else
			mode = "?";
	}
	if ( mksc < 1 || mksc > 32 )
		mksc = 1;
	if ( xsc < 2 || xsc > 32 )
		xsc = 8;
	/* -tile with no backend named is the page COUNT (sec. 58) */
	if ( tilef && mode == (char *)0 && tilen )
		mode = "print";
	nsheets = argc - i;
	/* -where takes its PATTERN first, then the sheets */
	if ( mode != (char *)0 && strcmp(mode, "where") == 0 && nsheets >= 2 )
	{
		wpat = argv[i++];
		nsheets--;
	}
	if ( mode == (char *)0 || nsheets < 1 || !knownmode(mode) ||
	     ((strcmp(mode, "renum") == 0 || strcmp(mode, "mksym") == 0 ||
	       strcmp(mode, "symsheet") == 0) && nsheets != 1) ||
	     (strcmp(mode, "diff") == 0 && nsheets != 2) ||
	     (strcmp(mode, "where") == 0 && wpat[0] == 0) )
	{
		fprintf(stderr,
	"usage: vellum -print|-pic|-net|-hpgl|-bom|-check|-ps|-dxf|-len|-spice [-wide] [-fit] [-scale N] file.d ...\n");
		fprintf(stderr,
	"       vellum -renum [-base N] file.d > out.d\n");
		fprintf(stderr,
	"       vellum -diff [-mark] old.d new.d\n");
		fprintf(stderr,
	"       vellum -where STR file.d ...\n");
		fprintf(stderr,
	"       vellum -symsheet lib.sym > card.d\n");
		fprintf(stderr,
	"       vellum -mksym CODE [-pfx P] [-org x,y] [-scale n] [-type NAME=t] file.d >> lib.sym\n");
		fprintf(stderr,
	"       vellum -symcheck lib.sym ...\n");
		fprintf(stderr,
	"       vellum -book sheet.d ... > contents.d\n");
		fprintf(stderr,
	"       vellum -tile [-n] -print|-ps|-hpgl big.d\n");
		return 1;
	}
	/* -tile means "as many pages as it takes" and -fit means "make
	 * this ONE page": that is the opposite instruction, and it is an
	 * error in one line rather than a silent precedence rule. */
	if ( tilef && fitf )
	{
		fprintf(stderr,
		    "vellum: -tile and -fit are opposite instructions\n");
		return 1;
	}
	if ( tilef && strcmp(mode, "print") != 0 && strcmp(mode, "ps") != 0 &&
	     strcmp(mode, "hpgl") != 0 )
	{
		fprintf(stderr,
		    "vellum: -tile applies to -print, -ps and -hpgl\n");
		return 1;
	}
	/* -symcheck reads symbol LIBRARIES, not drawings, and it reads
	 * ONLY the ones it is given: the standard set is not loaded
	 * underneath, so "defined in both" means these files (sec. 56) */
	if ( strcmp(mode, "symcheck") == 0 )
	{
		for ( ; i < argc; i++ )
			dosymcheck(argv[i]);
		return dosymcheckend();
	}
	/* -symsheet reads a symbol LIBRARY, not a drawing, and it reads
	 * only the one it is given: the card is THAT library's card */
	if ( strcmp(mode, "symsheet") == 0 )
		return dosymsheet(argv[i]);
	checkf = strcmp(mode, "check") == 0;
	v5 = strcmp(mode, "len") == 0 || strcmp(mode, "where") == 0 ||
	     strcmp(mode, "spice") == 0;
	/* -tile pins the scale its backend pins, before the first page */
	if ( tilef && strcmp(mode, "ps") == 0 )
		xsc = 8;
	loadsyms();
	if ( strcmp(mode, "spice") == 0 )
		printf("* vellum -spice\n");
	r = 0;
	first = i;
	for ( ; i < argc; i++ )
	{
		if ( (argv[i][0] == '-' && argv[i][1] == 0 ?
		      loadstdin() : loadfile(argv[i])) < 0 )
		{
			fprintf(stderr, "vellum: cannot open %s\n", argv[i]);
			return 1;
		}
		/* -len adds no millimetres to inches: a set whose sheets
		 * disagree about the unit is refused in one line (sec. 46),
		 * and that disagreement is already a -check finding.  Only
		 * -len: -where and -spice never multiply by a unit, and a
		 * -diff across a unit change REPORTS it (dodiff) instead of
		 * refusing to look. */
		if ( strcmp(mode, "len") == 0 )
		{
			static int u0;
			static char un0[UNAMEL];

			if ( i == first )
			{
				u0 = unum;
				strcpy(un0, uname);
			}
			else if ( unum != u0 || strcmp(uname, un0) != 0 )
			{
				fprintf(stderr,
		"vellum: %s: units U %d %s disagree with the set's\n",
					argv[i], unum, uname);
				return 1;
			}
		}
		if ( strcmp(mode, "diff") == 0 )
		{
			/* the OLD revision is shadowed whole, the NEW one
			 * stays live: the only mode that holds two */
			if ( i == first )
			{
				dsave();
				continue;
			}
			velv5diff(argv[i], markf);
			return velv5end(mode);
		}
		if ( v5 )
		{
			velv5(mode, i - first + 1, argv[i], wpat);
			continue;
		}
		if ( checkf )
		{
			/* the whole -check pass: parts, wires, dropped Y
			 * lines, units agreement, then the net checks */
			static int u0;
			static char un0[UNAMEL];
			register int k, hasw;

			chksheet = argv[i];
			if ( i == first )
			{
				u0 = unum;
				strcpy(un0, uname);
			}
			else if ( unum != u0 || strcmp(uname, un0) != 0 )
			{
				char ub[UNAMEL + 8];

				sprintf(ub, "%d %s", unum, uname);
				chk("units U %s disagree with the set's",
				    ub, 0);
			}
			chkparts();
			chkcodes(argv[i]);
			/* the CONDUCTOR checks (dangling ends, nets, pins)
			 * apply only to a sheet that draws circuits -- one
			 * that contains WIRES.  A diagram sheet (flowchart,
			 * floor plan, structure chart) uses stencils whose
			 * pins are attachment points, not terminals, and
			 * flagging those is noise, not findings. */
			hasw = 0;
			for ( k = 0; k < nobj; k++ )
				if ( obj[k].o_type == OT_WIRE )
				{
					hasw = 1;
					break;
				}
			if ( hasw )
			{
				chkwires();
				donet(i - first + 1);
			}
		}
		else if ( tilef )
			r |= dotile(mode);
		else if ( strcmp(mode, "print") == 0 )
			r |= doprint();
		else if ( strcmp(mode, "pic") == 0 )
		{
			xsc = 8;	/* pic is pinned: 16 units/inch */
			r |= dopic();
		}
		else if ( strcmp(mode, "hpgl") == 0 )
			r |= dohpgl();
		else if ( strcmp(mode, "ps") == 0 )
		{
			xsc = 8;	/* pinned like pic: 16 units/inch */
			r |= dops();
		}
		else if ( strcmp(mode, "mksym") == 0 )
			r |= domksym(argv[i]);
		else if ( strcmp(mode, "book") == 0 )
			dobook(i - first + 1, argv[i]);
		else if ( strcmp(mode, "dxf") == 0 )
		{
			xsc = 4;	/* grid units out, quarter-unit exact */
			r |= dodxf();
		}
		else if ( strcmp(mode, "renum") == 0 )
			r |= dorenum(rbase);
		else if ( strcmp(mode, "bom") == 0 )
			bomsheet();
		else
			donet(i - first + 1);
	}
	if ( pspage )
		printf("%%%%Trailer\n%%%%Pages: %d\n", pspage);
	if ( dxopen )
		printf("0\nENDSEC\n0\nEOF\n");
	if ( checkf )
	{
		chksheet = "set";
		donetend();
		if ( chkn == 0 )
			fprintf(stderr, "vellum: check clean\n");
		return chkn > 254 ? 254 : chkn;
	}
	if ( v5 )
		return velv5end(mode);
	if ( strcmp(mode, "bom") == 0 )
		dobomout();
	else if ( strcmp(mode, "net") == 0 )
		donetend();
	return r;
}
