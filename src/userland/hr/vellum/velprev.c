/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * velprev.c - Vellum's print PREVIEW window (/usr/vellum/lib/velprev):
 * an ordinary hrgui window that draws the PRINTABLE extent of one .d
 * sheet -- layprn applied, fit-to-window scale -- through a cl_*
 * backend behind the SAME walker (velwalk.c) that feeds the Epson
 * bands, so what the window shows is what the paper gets (VELLUM.md
 * sec. 25).  Spawned by the editor's Print dialog (Preview button),
 * and runnable by hand: velprev file.d.
 *
 * Links velbase + velfile + velwalk + velgfx and the gfx shared
 * library; nothing of the editor.  The page is static, so the repaint
 * story is simple: one painter, run on expose/resize -- a viewer, not
 * an editor, so the full pass IS the damage answer.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "vellum.h"

HRAPP	me = { "Preview", "printer.icn", 660, 508, HRF_STRETCH, 0, 0, 0 };

int	mywid;
int	contw, conth;
int	gotext;			/* the drawing has a printable extent      */
int	pgx0, pgy0, pgx1, pgy1;	/* ... which is this, grid units           */

/* ---- the cl_* backend: velgfx's styled primitives under the walk's
 * current style ---- */
int	vfl;			/* current OF_STYLE|OF_BOLD                */

static
v_style(fl)
{
	vfl = fl;
	stpat(fl);
	return 0;
}

static
v_line(x0, y0, x1, y1)
{
	sline(x0, y0, x1, y1, 0, vfl);
	return 0;
}

/* walker fill values (0 blk / 1 wht / 2 gray / 3 hatch) -> velgfx
 * rowspan's (0 blk / 1 wht / 3 gray / 4 hatch) */
static
v_span(x0, x1, y, val)
{
	rowspan(x0, x1, y, val == 2 ? 3 : val == 3 ? 4 : val);
	return 0;
}

static
v_box(x0, y0, x1, y1, fill)
{
	register int y;

	if ( fill >= 0 )
		for ( y = y0; y <= y1; y++ )
			v_span(x0, x1, y, fill);
	v_line(x0, y0, x1, y0);
	v_line(x1, y0, x1, y1);
	v_line(x1, y1, x0, y1);
	v_line(x0, y1, x0, y0);
	return 0;
}

static
v_circle(cx, cy, r, fill)
{
	register int y, k;
	int xo;

	if ( r <= 0 )
		return 0;
	if ( fill >= 0 )
		for ( y = cy - r; y <= cy + r; y++ )
		{
			k = y - cy;
			if ( k < 0 ) k = -k;
			xo = (int)isqrt((long)r * r - (long)k * k);
			v_span(cx - xo, cx + xo, y, fill);
		}
	scirc(cx, cy, r, 0, vfl);
	return 0;
}

static
v_text(x, y, sz, s)
char *s;
{
	cl_ptextt(fontslot(sz), x, y, s);
	return 0;
}

static
v_vtext(x, y, sz, s)
char *s;
{
	vtext(fontslot(sz), x, y, s);
	return 0;
}

XB	prevxb = { v_line, v_box, v_circle, v_text, v_span, v_style,
		   v_vtext, (int (*)())0, (int (*)())0 };

/* THE painter: white the content, pick the largest print scale that fits
 * the window (8 like the printer, else 4, else 2), page outline, walk. */
static
paint()
{
	int w, h, px1, py1;

	cl_fillrect(0, 0, contw, conth, 1);
	if ( !gotext )
	{
		cl_ptext(SHM_FUI, 8, 8, "Nothing printable");
		return 0;
	}
	w = pgx1 - pgx0 + 2;
	h = pgy1 - pgy0 + 2;
	for ( xsc = 8; xsc > 2; xsc /= 2 )
		if ( w * xsc <= contw && h * xsc <= conth )
			break;
	xorgx = (pgx0 - 1) * xsc;
	xorgy = (pgy0 - 1) * xsc;
	/* the paper's edge, so the extent reads as a page */
	px1 = w * xsc;
	py1 = h * xsc;
	cl_lpat(0xaaaa);
	cl_line(0, py1, px1, py1, 0);
	cl_line(px1, 0, px1, py1, 0);
	cl_lpat(0xffff);
	xwalk(&prevxb);
	return 0;
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int dirty;

	loadsyms();
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);
	contw = me.ha_w;
	conth = me.ha_h;
	if ( argc < 2 || loadfile(argv[1]) < 0 )
		gotext = 0;
	else
		gotext = xextent(&pgx0, &pgy0, &pgx1, &pgy1);

	dirty = 1;
	for (;;)
	{
		cl_refresh();
		if ( dirty && cl_mapped() && !cl_frozen() )
		{
			cl_dropped();		/* clear: we repaint whole anyway */
			cl_begin();
			paint();
			cl_end();
			dirty = 0;
		}
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
			switch ( e.wm_type )
			{
			case E_EXPOSE:
				dirty = 1;
				break;
			case E_RESIZE:
				contw = e.wm_arg[0];
				conth = e.wm_arg[1];
				dirty = 1;
				break;
			case E_KEY:
				if ( (e.wm_arg[0] & 0xff) == 'q' ||
				     (e.wm_arg[0] & 0xff) == 0x1b )
					exit(0);
				break;
			case E_QUIT:
				exit(0);
			}
		if ( hr_evover(mywid) )
			dirty = 1;
	}
}
