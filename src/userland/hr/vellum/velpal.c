/*
 * velpal.c - Vellum's palette-bank painter (/usr/vellum/lib/velpal):
 * the editor's last big severable surface, seceded (VELLUM.md sec. 39)
 * on the zdock widget pattern.  The editor forks us with the dock-cell
 * contract (-W wid,x0,y0,x1,y1,edpid -- the palette rect of its OWN
 * window) followed by its loaded library PATHS in order, so our symbol
 * table gets the editor's exact group indices; cl_subinit turns the
 * rect into a clgfx sub-surface and every cl_* primitive clips inside
 * the editor's published visible rects.
 *
 * The protocol is the 12-byte sync block in the GDS tail (shmem.h
 * SHM_VELPAL): the editor writes {lib, row, armed} and bumps vp_gen
 * (vp_fullgen for an expose), then POKES us with SIGALRM.  We repaint
 * BY GENERATION, never by suspicion: a full bank pass only when the
 * group/row/fullgen moved (or a draw was dropped to a freeze), an
 * arming change repaints exactly the two cells whose state flipped --
 * the damage discipline, kept across the process split.  We never
 * self-alarm, so the editor's pokes are the only SIGALRM here and the
 * zdock re-install trap does not apply.  A dead velpal costs a blank
 * bank, never a wedged editor: it respawns us on the next palette
 * damage (vp_pid is our liveness stamp -- we are double-forked, so
 * init reaps and a failed poke means really dead).
 *
 * Coordinates are CELL coordinates: (0,0) = the palette's top-left,
 * cl_ch() = its height.  Links velbase + velfile + velgfx + the gfx
 * shared library; nothing of the editor.
 */
#include <stdio.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrwidg.h"
#include "vellum.h"

/* the editor's palette layout (vellum.c; kept in step by hand) */
#define	SCW	32		/* symbol cell                            */
#define	SCH	28
#define	LHDR	14		/* header cell: the library-group name    */
#define	ABAR	14		/* arrow bar at the palette foot          */
#define	SHGRP	(nlib)		/* the shapes pseudo-group's index        */
#define	PC_ARC	(NSHAPE + NCONNS)
#define	PC_NET	(NSHAPE + NCONNS + 1)
#define	PC_DIM	(NSHAPE + NCONNS + 2)
#define	NPCELL	(NSHAPE + NCONNS + 3)

short	palidx[MAXSYM];		/* the bank: symtab indices of the group  */
int	npal;
int	palsh;			/* 1 = the shapes group is showing        */
int	vlib, vrow, varm;	/* what the bank currently SHOWS          */
int	sfullgen, sgen;		/* generations as last painted            */

int	pokes;
int	strikes;

static
poke()
{
	signal(SIGALRM, poke);	/* FIRST (V7 one-shot); the editor is the
				 * only SIGALRM source in this process */
	pokes = 1;
}

/* the group's view (the editor's palview, against OUR symtab) */
static
palview(lib)
{
	register int i;

	palsh = (lib == SHGRP);
	npal = 0;
	if ( palsh )
		npal = NPCELL;
	else
		for ( i = 0; i < nsym; i++ )
			if ( symtab[i].sy_lib == lib )
				palidx[npal++] = i;
	return 0;
}

static
palrows()
{
	return (npal + 1) / 2;
}

static
palvis()
{
	register int v;

	v = (cl_ch() - ABAR - LHDR) / SCH;
	if ( v < 1 )
		v = 1;
	return v;
}

/* one cell frame; on = pressed (inverted) */
static
palcell(x, y, w, h, on)
{
	cl_line(x, y, x + w - 1, y, 0);
	cl_line(x + w - 1, y, x + w - 1, y + h - 1, 0);
	cl_line(x + w - 1, y + h - 1, x, y + h - 1, 0);
	cl_line(x, y + h - 1, x, y, 0);
	if ( on )
		cl_fillrect(x + 1, y + 1, x + w - 1, y + h - 1, 2);
	return 0;
}

/* a scroll arrow: a small filled triangle */
static
palarrow(x, y, down)
{
	register int i;
	int cx, ty;

	cx = x + SCW / 2;
	ty = y + (ABAR - 5) / 2;
	for ( i = 0; i < 5; i++ )
		cl_line(cx - (down ? 4 - i : i), ty + i,
			cx + (down ? 4 - i : i), ty + i, 0);
	return 0;
}

/* symbol si at (rot,mir) around pixel (ox,oy), ppq px per q unit --
 * vellum.c drawsymat's twin (each binary carries its own copy; the
 * canvas needs the editor's for placed symbols anyway) */
static
drawsymat(si, rot, mir, ox, oy, ppq)
{
	register short *p;
	int x0, y0, x1, y1;
	char tb[2];

	p = symtab[si].sy_ops;
	while ( *p != SEND )
	{
		if ( *p == SE )
		{
			txq(p[1], p[2], rot, mir, ox, oy, ppq, &x0, &y0);
			txq(p[3], p[4], rot, mir, ox, oy, ppq, &x1, &y1);
			cl_line(x0, y0, x1, y1, 0);
			p += 5;
		}
		else if ( *p == SC )
		{
			txq(p[1], p[2], rot, mir, ox, oy, ppq, &x0, &y0);
			cl_circle(x0, y0, p[3] * ppq, 0);
			p += 4;
		}
		else if ( *p == SA )
		{
			int a0, a1, t;

			txq(p[1], p[2], rot, mir, ox, oy, ppq, &x0, &y0);
			a0 = p[4];
			a1 = p[5];
			if ( mir )
			{
				t = a0;
				a0 = 180 - a1;
				a1 = 180 - t;
			}
			a0 -= 90 * (rot & 3);
			a1 -= 90 * (rot & 3);
			arcline(x0, y0, p[3] * ppq, a0, a1, 0);
			p += 6;
		}
		else			/* ST: preview scale draws it too */
		{
			if ( ppq >= 2 )
			{
				txq(p[1], p[2], rot, mir, ox, oy, ppq,
				    &x0, &y0);
				tb[0] = p[3];
				tb[1] = 0;
				cl_ptextt(SHM_FICON, x0, y0, tb);
			}
			p += 4;
		}
	}
	return 0;
}

/* ONE bank cell (bank position k): a symbol preview, or -- shapes
 * group -- a mini parametric shape, connector glyph, arc or net cell */
static
palbank1(k, arm)
{
	register SYMDEF *s;
	int x, y, ox, oy;

	if ( k < vrow * 2 || k >= (vrow + palvis()) * 2 || k >= npal )
		return 0;
	x = (k & 1) * SCW;
	y = LHDR + ((k >> 1) - vrow) * SCH;
	cl_fillrect(x, y, x + SCW, y + SCH, 1);
	if ( palsh )
	{
		if ( k < NSHAPE )
			shapeoutline(k, x + 5, y + 6, x + SCW - 6,
				     y + SCH - 7, 0);
		else if ( k < NSHAPE + NCONNS )
		{
			int cs, x0, y0, x1, y1;

			cs = k - NSHAPE;
			x0 = x + 5;   y0 = y + SCH - 9;
			x1 = x + SCW - 6;   y1 = y + 8;
			if ( cs == CS_HV || cs == CS_HARROW )
			{
				cl_line(x0, y0, x1, y0, 0);
				cl_line(x1, y0, x1, y1, 0);
			}
			else
				cl_line(x0, y0, x1, y1, 0);
			if ( cs == CS_ARROW )
				arrowhead(x1, y1, x1 - x0, y1 - y0);
			else if ( cs == CS_HARROW )
				arrowhead(x1, y1, 0, y1 - y0);
		}
		else if ( k == PC_ARC )
			arcline(x + 4, y + SCH - 6, 22, 10, 80, 0);
		else if ( k == PC_NET )
			cl_ptext(SHM_FICON, x + (SCW - 18) / 2,
				 y + (SCH - 8) / 2, "Net");
		else			/* Dim: a sample dimension */
		{
			int ym;

			ym = y + SCH / 2 + 4;
			cl_line(x + 5, ym - 4, x + 5, ym + 4, 0);
			cl_line(x + SCW - 6, ym - 4, x + SCW - 6, ym + 4, 0);
			cl_line(x + 5, ym, x + SCW - 6, ym, 0);
			arrowhead(x + 5, ym, -8, 0);
			arrowhead(x + SCW - 6, ym, 8, 0);
			cl_ptextt(SHM_FICON, x + (SCW - 12) / 2, y + 3, "12");
		}
		palcell(x, y, SCW, SCH, arm == 1000 + k);
		return 0;
	}
	s = &symtab[palidx[k]];
	ox = x + (SCW - (s->sy_x1 - s->sy_x0)) / 2 - s->sy_x0;
	oy = y + (SCH - (s->sy_y1 - s->sy_y0)) / 2 - s->sy_y0;
	drawsymat(palidx[k], 0, 0, ox, oy, 1);
	palcell(x, y, SCW, SCH, palidx[k] == arm);
	return 0;
}

/* bank cell of an ARM CODE in the current view, or -1 */
static
palcellof(code)
{
	register int i;

	if ( palsh )
		return (code >= 1000) ? code - 1000 : -1;
	if ( code < 0 || code >= 1000 )
		return -1;
	for ( i = 0; i < npal; i++ )
		if ( palidx[i] == code )
			return i;
	return -1;
}

/* the whole palette: header, bank, arrows, right border */
static
drawpal()
{
	register int k;
	int y, w, vis, maxr, ch;

	ch = cl_ch();
	vis = palvis();
	maxr = palrows() - vis;
	if ( maxr < 0 )
		maxr = 0;
	if ( vrow > maxr )
		vrow = maxr;
	cl_fillrect(0, 0, PALW, ch, 1);
	{
		register char *gn;

		gn = palsh ? "shapes" : (nlib ? libname[vlib] : "-");
		w = strlen(gn) * 6;
		cl_ptext(SHM_FICON, (PALW - w) / 2, (LHDR - 8) / 2, gn);
	}
	palcell(0, 0, PALW, LHDR, 0);
	for ( k = vrow * 2; k < npal && k < (vrow + vis) * 2; k++ )
		palbank1(k, varm);
	y = ch - ABAR;
	palarrow(0, y, 0);
	palarrow(SCW, y, 1);
	palcell(0, y, SCW, ABAR, 0);
	palcell(SCW, y, SCW, ABAR, 0);
	cl_line(PALW - 1, 0, PALW - 1, ch - 1, 0);
	return 0;
}

/* one poke's worth of repaint: full by GENERATION (group/row/fullgen
 * moved, or a draw was dropped to a freeze/unmap), else exactly the
 * two cells an arming change flipped */
static
repaint()
{
	register HRVELPAL *v;
	int full, oldarm;

	v = hr_velpal();
	full = v->vp_fullgen != sfullgen || v->vp_lib != vlib ||
	       v->vp_row != vrow || cl_dropped();
	oldarm = varm;
	if ( v->vp_lib != vlib )
	{
		vlib = v->vp_lib;
		palview(vlib);
	}
	vrow = v->vp_row;
	varm = v->vp_arm;
	sfullgen = v->vp_fullgen;
	sgen = v->vp_gen;
	cl_begin();
	if ( full )
		drawpal();
	else if ( varm != oldarm )
	{
		if ( oldarm >= 0 )
			palbank1(palcellof(oldarm), varm);
		if ( varm >= 0 )
			palbank1(palcellof(varm), varm);
	}
	cl_end();
	return 0;
}

main(argc, argv)
char **argv;
{
	register int i;

	signal(SIGALRM, poke);		/* FIRST: the editor may poke at once */
	if ( hr_wopen(&argc, argv) < 0 )
		exit(1);		/* not started by the editor */
	for ( i = 1; i < argc; i++ )	/* the editor's libraries, ITS order */
		loadlib(argv[i]);
	vlib = -1;			/* nothing shown yet: first poke is full */
	varm = -1;
	sfullgen = -1;
	hr_velpal()->vp_pid = getpid();	/* ready: the editor may poke/probe */
	pokes = 1;			/* paint once without waiting */
	for (;;)
	{
		if ( pokes )
		{
			pokes = 0;
			if ( !hr_wlive() )
			{
				if ( ++strikes >= 2 )
					exit(0);
			}
			else
			{
				strikes = 0;
				cl_refresh();
				if ( cl_mapped() && !cl_frozen() )
					repaint();
			}
		}
		if ( !pokes )
			pause();
	}
}
