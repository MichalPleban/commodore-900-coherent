/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * velplot.c - velplot: a drawing onto a PAGE, on whatever device the
 * shop owns.
 *
 *	velplot [-T lp|ps|hpgl] [-wide] [-fit] [-tile [-n]] [-scale N]
 *		file.d ...
 *
 * One object WALKER (velwalk, shared with the preview window) feeds a
 * 9-function backend struct; this file holds the three PAGE backends
 * and the tiling driver that runs one of them once per page.  Scale:
 * one grid unit is XSC device px, 8 by default -- the Epson's dot,
 * and 16 grid units to the inch on the laser and the plotter.
 *
 * The three devices differ only in the backend struct they hand the
 * walker, which is why -tile, -fit, -wide and the crop marks are
 * written once here and not three times.
 */
#include <stdio.h>
#include "vellum.h"

extern char	*malloc();

/* ================================================================== */
/* -T lp: raster into 24-row bands, ESC * 39 to stdout               */
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
		fprintf(stderr, "velplot: nothing to print\n");
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
	"velplot: print: %d dots wide, the head prints %d -- use -tile or -fit\n",
			r, MAXDOTS);
	return prpage((gx1 - gx0 + 2) * XSC, (gy1 - gy0 + 2) * XSC);
}

/* ================================================================== */
/* -T hpgl: the third walker backend (pen plotters).  Device px x 4 = */
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
		fprintf(stderr, "velplot: nothing to plot\n");
		return 1;
	}
	xorgx = (gx0 - 1) * XSC;
	xorgy = (gy0 - 1) * XSC;
	return hpone((gx1 - gx0 + 2) * XSC, (gy1 - gy0 + 2) * XSC);
}

/* ================================================================== */
/* -T ps: PostScript Level 1 -- the laser release (VELLUM.md sec. 35). */
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
		fprintf(stderr, "velplot: nothing to print\n");
		return 1;
	}
	xorgx = (gx0 - 1) * XSC;
	xorgy = (gy0 - 1) * XSC;
	return psone((gx1 - gx0 + 2) * XSC, (gy1 - gy0 + 2) * XSC);
}

/* ================================================================== */
/* -tile: the drawing across as many pages as it takes (sec. 58)      */
/* ================================================================== */

dotile(dev)
char *dev;
{
	int gx0, gy0, gx1, gy1, pw, ph, fw, fh, w, h, ovl, sx, sy, r;

	if ( !xextent(&gx0, &gy0, &gx1, &gy1) )
	{
		fprintf(stderr, "velplot: nothing to print\n");
		return 1;
	}
	/* the PHYSICAL page of the chosen backend, device px */
	if ( strcmp(dev, "ps") == 0 )
	{
		xsc = 8;		/* pinned like pic: 16 units/inch */
		/* A4's usable box inside 36 pt margins: 16 units per inch
		 * is 16/72 px per point, written 2/9 because 523 * 16 * 8
		 * does not fit a 16-bit int */
		pw = 523 * XSC * 2 / 9;
		ph = 770 * XSC * 2 / 9;
	}
	else if ( strcmp(dev, "hpgl") == 0 )
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
		fprintf(stderr, "velplot: -tile: the page is too small\n");
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
			if ( strcmp(dev, "ps") == 0 )
				r |= psone(tpw, tph + TLABH);
			else if ( strcmp(dev, "hpgl") == 0 )
				r |= hpone(tpw, tph + TLABH);
			else
				r |= prpage(tpw, tph + TLABH);
		}
	xclipon = 0;
	return r;
}

/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

static
usage()
{
	fprintf(stderr,
	    "usage: velplot [-T lp|ps|hpgl] [-wide] [-fit] [-scale N] file.d ...\n");
	fprintf(stderr,
	    "       velplot -tile [-n] [-T lp|ps|hpgl] big.d ...\n");
	return 2;
}

main(argc, argv)
char **argv;
{
	register int i;
	register char *dev;
	int r;

	velprog = "velplot";
	dev = "lp";
	for ( i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++ )
	{
		if ( strcmp(argv[i], "-T") == 0 && i + 1 < argc )
			dev = argv[++i];
		else if ( strcmp(argv[i], "-wide") == 0 )
			widef = 1;
		else if ( strcmp(argv[i], "-fit") == 0 )
			fitf = 1;
		else if ( strcmp(argv[i], "-tile") == 0 )
			tilef = 1;
		else if ( strcmp(argv[i], "-n") == 0 )
			tilen = 1;
		else if ( strcmp(argv[i], "-scale") == 0 && i + 1 < argc )
			xsc = atoi(argv[++i]);
		else
			exit(usage());
	}
	if ( strcmp(dev, "lp") != 0 && strcmp(dev, "ps") != 0 &&
	     strcmp(dev, "hpgl") != 0 )
	{
		fprintf(stderr, "velplot: -T wants lp, ps or hpgl\n");
		exit(2);
	}
	if ( xsc < 2 || xsc > 32 )
		xsc = 8;
	if ( i >= argc )
		exit(usage());
	/* -tile means "as many pages as it takes" and -fit means "make
	 * this ONE page": that is the opposite instruction, and it is an
	 * error in one line rather than a silent precedence rule. */
	if ( tilef && fitf )
	{
		fprintf(stderr,
		    "velplot: -tile and -fit are opposite instructions\n");
		exit(2);
	}
	/* the pinned scales, before the first page: the laser and the
	 * plotter measure in inches, not in the Epson's dots */
	if ( strcmp(dev, "ps") == 0 )
		xsc = 8;
	nsheets = argc - i;
	loadsyms();
	r = 0;
	for ( ; i < argc; i++ )
	{
		if ( loadsheet(argv[i]) < 0 )
			exit(1);
		if ( tilef )
			r |= dotile(dev);
		else if ( strcmp(dev, "ps") == 0 )
			r |= dops();
		else if ( strcmp(dev, "hpgl") == 0 )
			r |= dohpgl();
		else
			r |= doprint();
	}
	if ( pspage )
		printf("%%%%Trailer\n%%%%Pages: %d\n", pspage);
	exit(r);
}
