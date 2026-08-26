/*
 * vellum.c - Vellum, the ZView technical drawing tool (editor unit).
 *
 * The drawing board of this workstation (see VELLUM.md): schematics,
 * block diagrams, flowcharts, network maps, structure charts -- with
 * clean printed output and full membership in the Unix pipeline.  A
 * direct-render client (GUI.md Model A) split over several translation
 * units (one module outgrew both the assembler's fix-up tables and,
 * together, the 64 K text segment):
 *
 *   vellum.c   this file: the window, palette/toolbar/canvas chrome,
 *              tools and gestures, damage machinery, undo, main loop
 *   velbase.c  the MODEL: object list, symbol pools, geometry (cl_-free)
 *   velgfx.c   styled drawing helpers (dash/dot/bold, shapes, arrows)
 *   velfile.c  the plain-text "vellum1" format and symbol libraries
 *   velcmd.c   editing commands (z-order, groups, align, clipboard,
 *              frame stamp) and the Settings / Style dialogs
 *   veldlg.c   the file/confirm/text/properties/library dialog stubs
 *
 * The editor DRAWS and does nothing else: rendering to paper, netlist
 * extraction, checking, diffing, measuring and the stencil converter
 * are separate TOOLS over the same libvellum.a (velplot velpic veldxf
 * velnet velcheck veldiff velinfo velsym), headless so they run where
 * there is no bitmap card, and small so none of them carries code it
 * never runs.  The editor's Print item spawns velplot, and Make Symbol
 * spawns velsym, so a page or a stencil made from the board is
 * byte-identical to one made from a Makefile.
 *
 * The drawing is a flat object list in GRID units: symbols (rot/mir,
 * auto designators, values), H-V wires with pin snap and junction dots,
 * lines, boxes, circles, sized multi-line text, stretchable labeled
 * SHAPES with attachment points, CONNECTORS that stay attached (with
 * arrowhead styles), polylines, arcs and net-name markers -- each with
 * an optional line style (dashed/dotted), fill (white/gray/black,
 * composing in z-order), width-2 bold, and one of four layers (drawing/
 * annotation/frame/construction; visibility and printing per layer).
 * The canvas PANS over the sheet (160x120 or A4 preset): scrollbars,
 * arrow keys, middle-drag, or 'v' = zoom-to-fit.  A middle CLICK
 * pastes at the pointer (HRF_MIDBTN carries the raw middle button).
 *
 * REPAINTS ARE DAMAGE-BASED (the law on a 6 MHz machine): flush() is
 * the only painter, actions declare what they touched, chrome repaints
 * by state comparison, and a full canvas pass happens only for VIEW
 * changes (pan/zoom/expose-all/undo diffs damage per object).
 *
 * The file format is PLAIN TEXT in grid units, one object per line
 * (see velfile.c and the manual page), so a drawing can be diffed,
 * generated, clipboard-pasted as text, or repaired like anything else
 * on this system.
 */
#include <stdio.h>
#include <types.h>
#include <dir.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"
#include "hrsbar.h"
#include "vellum.h"

/* ---- geometry (GRID/PALW/CTOP/TBH/CANY live in vellum.h now: the
 * model layer's grid<->px transforms use them) ---- */
#define	SCW	32		/* symbol cell                            */
#define	SCH	28
#define	TBW	38		/* toolbar cell width (19 cells = 722 --  */
				/* the sheet cells moved to the file bar) */
#define	LHDR	14		/* palette header cell: the library name  */
#define	SHCW	20		/* one sheet-set cell on the file bar     */
/* (STH / SBW moved to vellum.h: veldlg.c's Find centres its hit with them) */


/* ---- tools ---- */
#define	T_SEL	0
#define	T_WIRE	1
#define	T_LINE	2
#define	T_BOX	3
#define	T_CIRC	4
#define	T_TEXT	5
#define	T_DEL	6
#define	NTOOL	7		/* the basic tools (cell index == tool)   */
/* Rot/Mir/Name/Dup are armable TOOLS too, Eagle-fashion: arm the cell,
 * then CLICK the object to act on -- and keep clicking others.  (While a
 * symbol is being PLACED, Rot/Mir instead turn the pending ghost.)  The
 * r/m/n/d keys still act directly on the current selection. */
#define	T_ROT	7
#define	T_MIR	8
#define	T_NAME	9
#define	T_DUP	10
/* palette cell indices (rows of 2); Grid sits between the two tool groups */
#define	C_GRID	7		/* grid-dot toggle                        */
#define	C_ROT	8
#define	C_MIR	9
#define	C_NAME	10
#define	C_DUP	11
#define	C_ZIN	12		/* zoom                                   */
#define	C_ZOUT	13
#define	C_LIB	14		/* load / switch a symbol library         */
#define	C_EDIT	15		/* open the CURRENT library in zsym       */
#define	C_FRONT	16		/* z-order: selection to the top          */
#define	C_BACK	17		/* ... to the bottom                      */
#define	C_LAYER	18		/* cycles the ACTIVE layer (label L0..L3) */
#define	NCELL	19		/* the < > sheet cells live on the FILE   */
				/* bar now: the sheet is a property of    */
				/* the file, and the toolbar got roomier  */

int	tool	= T_SEL;
int	cursym	= -1;		/* >= 0: a symbol is armed for placing    */
int	curshape = -1;		/* >= 0: a SHAPE kind armed (drag a rect) */
int	curconn	= -1;		/* >= 0: a connector style armed          */
int	arcarm;			/* the Arc cell armed (two-stage gesture) */
int	netarm;			/* the Net cell armed (click + name)      */
int	dimarm;			/* the Dim cell armed (drag point-point)  */
int	placerot, placemir;	/* orientation the next placement gets    */
int	gridon	= 1;

/* the pending-arc second stage: centre+radius fixed, sweeping the end */
int	arcpend;
int	arccx, arccy, arcr, arca0;

/* the Line tool's chain: the object index being EXTENDED into a
 * polyline (-1 = none), valid only while its pool block ends the pool */
int	lchain	= -1;

/* the connector endpoints' attachments as snapped (press / release) */
int	ka_obj	= -1, ka_pt;	/* anchor/press end                       */
int	kb_obj	= -1, kb_pt;	/* current/release end                    */

/* middle-button: press point (pan drag vs click-paste) */
int	midpx, midpy, panvx, panvy;

/* ---- the window ---- */

HRAPP	me = { "Vellum", "vellum.icn", 0, 0,
	       HRF_STRETCH | HRF_CONFIRM | HRF_TRACK | HRF_MIDBTN, 0, 0,
	       HRM_NEW | HRM_OPEN | HRM_SAVE | HRM_CUT | HRM_COPY |
	       HRM_PASTE | HRM_SETTINGS | HRM_HELP | HRM_PRINT | HRM_SEARCH };

int	mywid;
int	contw, conth;		/* granted content size, px               */
int	lastgx, lastgy;		/* last pointer grid position (status)    */
char	fname[FNLEN];		/* current file ("" = untitled)           */
int	statdirty;

/* ---- drag / rubber-band state ---- */
#define	DR_NONE	0
#define	DR_TOOL	1		/* creating something (see dtool)         */
#define	DR_MOVE	2		/* moving the whole selection             */
#define	DR_PLACE 3		/* placing the armed symbol               */
#define	DR_SREC	4		/* Sel drag on empty canvas: rect select  */
#define	DR_HSB	5		/* dragging the horizontal thumb          */
#define	DR_VSB	6		/* dragging the vertical thumb            */
#define	DR_RSZ	7		/* dragging a shape's corner handle       */
#define	DR_PAN	8		/* middle-drag: panning the view          */
#define	DR_PANP	9		/* middle press, not yet moved (a middle  */
				/* CLICK is Paste; a middle DRAG pans)    */
#define	DR_END	10		/* dragging an endpoint / vertex grip     */
int	drag	= DR_NONE;
int	sbgrab;			/* thumb drags: press offset in the thumb */
int	ende;			/* DR_END: endpoint (0/1) or poly vertex  */
int	endrb;			/* DR_END: the rubber kind (wire routes)  */

/* what a DR_TOOL drag is creating */
#define	DT_WIRE	0
#define	DT_LINE	1
#define	DT_BOX	2
#define	DT_CIRC	3
#define	DT_SHAPE 4
#define	DT_CONN	5
#define	DT_ARC	6
#define	DT_DIM	7
int	dtool;
int	rszc;			/* DR_RSZ: which corner (0 tl 1 tr 2 bl 3 br) */

/* ---- the one-level undo snapshot: EDITOR-ONLY, and sized to the
 * DRAWING rather than to MAXOBJ.  As three fixed arrays it was 19 248
 * bytes of bss -- a whole second model, 400 objects and both pools --
 * and this machine allocates a process's data at exec, so an empty
 * editor paid all of it.  A real drawing runs to a few dozen objects,
 * so the snapshot is ONE heap block laid out
 *
 *	[unobj DOBJs][uppuse shorts of ppool][utpuse bytes of tpool]
 *
 * and rebuilt per commit.  Every offset in it is even (sizeof(DOBJ)
 * is, and the pool is shorts), so the derived pointers stay aligned.
 * A failed allocation costs the UNDO and never the drawing: uvalid
 * goes to 0 and undo() is a no-op, which is exactly the state the
 * editor is in before its first commit anyway.
 *
 * The packing itself is velsnap.c's, in the library: it is a model
 * operation with nothing graphical in it, so a plain test program can
 * link it and prove a round trip without a window. ---- */
static char	*usnap;		/* the pre-image block, or 0             */
static int	unobj;		/* objects in it ...                     */
static int	uppuse, utpuse;	/* ... and each pool's use               */

/* ---- damage bookkeeping: NOTHING repaints wholesale on a 6 MHz machine.
 * Every action declares what it touched; flush() (the only painter the
 * main loop calls) repaints exactly that -- one accumulated canvas rect,
 * plus whichever chrome pieces actually changed state (per CELL for the
 * toolbar and the palette highlight, by comparison against what is on
 * screen, so arming a tool repaints two cells and nothing else). ---- */
int	ddcanv;			/* the whole canvas view changed (pan/zoom) */
int	ddrect;			/* 1 = drx.. hold an accumulated damage rect */
int	drx0, dry0, drx1, dry1;
int	ddpal;			/* full palette bank (scroll / library)     */
int	ddtbar;			/* full toolbar                             */
int	ddtop;			/* file bar (name change; star is auto)     */
int	ddsb;			/* scrollbars (thumb moved)                 */
int	shownmod = -1;		/* modified-star state the top bar shows    */
int	showncur = -2;		/* palette cell currently highlighted       */
int	shownrow = -1;		/* palrow the bank was drawn at             */
char	tbon[NCELL];		/* toolbar cell states as last drawn        */
int	tbvalid;		/* tbon[] reflects the screen               */
int	s_valid;		/* 0 = the status bar SURFACE needs a full  */
				/* redraw (first paint / expose over it);   */
				/* its per-field shown copies sit with      */
				/* drawstat below                           */

viewdirty()
{
	/* The whole-canvas repaint draws every visible object WHOLE, and an
	 * object half off the canvas spills over the chrome (cl_* clips to
	 * the window, not the canvas rect) -- so a view change repaints the
	 * chrome too; flush() paints it AFTER the canvas, covering spill. */
	ddcanv = 1;
	ddsb = 1;
	ddtbar = 1;
	ddpal = 1;
	ddtop = 1;
	statdirty = 1;
	s_valid = 0;
	return 0;
}

static
alldirty()
{
	viewdirty();
	ddpal = 1;
	ddtbar = 1;
	ddtop = 1;
	tbvalid = 0;
	shownrow = -1;
	shownmod = -1;
	s_valid = 0;
	return 0;
}

/* Accumulate a canvas damage rect (px). */
dmg(x0, y0, x1, y1)
{
	if ( ddrect )
	{
		if ( x0 < drx0 ) drx0 = x0;
		if ( y0 < dry0 ) dry0 = y0;
		if ( x1 > drx1 ) drx1 = x1;
		if ( y1 > dry1 ) dry1 = y1;
	}
	else
	{
		drx0 = x0;  dry0 = y0;  drx1 = x1;  dry1 = y1;
		ddrect = 1;
	}
	return 0;
}

/* Damage an object PLUS everything drawn around it: the selection ring
 * (-3..+4) and the designator/value labels (small font beside or above/
 * below the body -- up to ~7 glyphs, hence the generous right margin). */
static
dmgpad(x0, y0, x1, y1)
{
	dmg(x0 - 5, y0 - 12, x1 + 48, y1 + 12);
	return 0;
}

dmgobj(i)
{
	int x0, y0, x1, y1;

	objpbox(i, &x0, &y0, &x1, &y1);
	dmgpad(x0, y0, x1, y1);
	return 0;
}

/* Damage the whole selection's boxes (before clearing/moving them). */
dmgsel()
{
	int x0, y0, x1, y1;

	if ( selbbox(&x0, &y0, &x1, &y1) )
		dmgpad(x0, y0, x1, y1);
	return 0;
}
int	wanchor;		/* 1 = a wire start is anchored (the      */
int	wax, way;		/* click-click idiom); its grid point     */

/* ---- the placement ghost, DEFERRED: motion only records where it should
 * be; flush() erases the old one BY REPAINT (idempotent, so a clip change
 * between frames cannot leave fragments -- the XOR erase could and did:
 * re-XORing after a raise/cover inverted pixels the draw never touched)
 * and paints the new one after all canvas painting, under the clip that
 * is current NOW. ---- */
int	ghpos;			/* a ghost position is known              */
int	ghgx, ghgy;		/* where the ghost belongs, grid          */
int	ghdrawn;		/* one is on screen, at ghx0..ghy1        */
int	ghx0, ghy0, ghx1, ghy1;

/* Damage the anchor cross's cell (shared: drop and place). */
static
dmganchor()
{
	dmg(gtopx(wax) - 6, gtopy(way) - 6, gtopx(wax) + 6, gtopy(way) + 6);
	return 0;
}

/* Drop a pending wire anchor, damaging its cross so it is erased. */
static
killanchor()
{
	if ( wanchor )
	{
		dmganchor();
		wanchor = 0;
	}
	return 0;
}
static	clearmodel(), killrun();

int	dgx, dgy;		/* drag start, grid                       */
int	cgx, cgy;		/* drag current, grid                     */
int	rubon;			/* an XOR figure is on screen             */
int	rbx0, rby0, rbx1, rby1;	/* what the XOR figure was drawn with, px */
int	mvx, mvy;		/* DR_MOVE: press offset from o_x,o_y     */


/* ------------------------------------------------------------------ */
/* the symbol library                                                 */
/*                                                                    */
/* Shapes live in QUARTER-grid units (4 q = 1 grid unit = 8 px), as a */
/* flat short array of ops:                                           */
/*   SE x0 y0 x1 y1   line segment                                    */
/*   SC cx cy r       circle                                          */
/*   ST x y ch        one small-font char (canvas scale only)         */
/*   SEND             end                                             */
/* The pin ends sit on WHOLE grid units (multiples of 4 q) so a wire  */
/* drawn on the grid meets them exactly.                              */
/* ------------------------------------------------------------------ */


char	laylbl[3] = "L0";	/* the C_LAYER cell shows the ACTIVE layer */
char	*toolnm[] = { "Sel", "Wire", "Line", "Box", "Circ", "Text", "Del",
		      "Grid", "Rot", "Mir", "Name", "Dup", "In", "Out",
		      "Lib", "Edit", "Fr", "Bk", laylbl };

/* ------------------------------------------------------------------ */
/* small math                                                         */
/* ------------------------------------------------------------------ */


/* px -> grid, rounding to the nearest multiple of the SNAP pitch
 * (gridstep units -- Settings can coarsen it to 2). */
static
pxtog(p, org, vo, lim)
{
	register int v, s;

	s = gridstep * gsc;
	v = vo * gsc + (p - org);
	if ( v < 0 ) v = 0;
	v = (v + s / 2) / s * gridstep;
	if ( v > lim ) v = lim;
	return v;
}

static
pxtogx(px)
{
	return pxtog(px, PALW, voxg, SHW);
}

static
pxtogy(py)
{
	return pxtog(py, CANY, voyg, SHH);
}

static	canvh(), cright();

/* Clamp the pan origin onto the sheet (shared: zoom, fit, Settings). */
clampvo()
{
	if ( voxg > SHW - 8 ) voxg = SHW - 8;
	if ( voyg > SHH - 8 ) voyg = SHH - 8;
	if ( voxg < 0 ) voxg = 0;
	if ( voyg < 0 ) voyg = 0;
	return 0;
}

/* Change the zoom, keeping the view CENTRE where it was.  1 on change. */
static
zoomto(n)
{
	int cx, cy;

	if ( n < 2 ) n = 2;
	if ( n > 16 ) n = 16;
	if ( n == gsc )
		return 0;
	cx = voxg + (cright() - PALW) / (2 * gsc);
	cy = voyg + (canvh() - CANY) / (2 * gsc);
	gsc = n;
	voxg = cx - (cright() - PALW) / (2 * gsc);
	voyg = cy - (canvh() - CANY) / (2 * gsc);
	clampvo();
	viewdirty();
	return 1;
}

/* Union GRID bbox of the selection (sel = 1) or of every layer-visible
 * object (sel = 0); 0 = nothing qualified. */
static
ugbox(sel, px0, py0, px1, py1)
int *px0, *py0, *px1, *py1;
{
	register int i;
	int x0, y0, x1, y1, got;

	got = 0;
	for ( i = 0; i < nobj; i++ )
	{
		if ( sel ? !osel[i] : !layvis[obj[i].o_layer] )
			continue;
		objgbox(i, &x0, &y0, &x1, &y1);
		if ( !got )
		{
			*px0 = x0;  *py0 = y0;  *px1 = x1;  *py1 = y1;
			got = 1;
		}
		else
		{
			if ( x0 < *px0 ) *px0 = x0;
			if ( y0 < *py0 ) *py0 = y0;
			if ( x1 > *px1 ) *px1 = x1;
			if ( y1 > *py1 ) *py1 = y1;
		}
	}
	return got;
}

/* Zoom-to-fit ('v'): the largest zoom and the pan that show the whole
 * drawing's extent. */
static
zoomfit()
{
	int ux0, uy0, ux1, uy1, w, h, z;

	if ( !ugbox(0, &ux0, &uy0, &ux1, &uy1) )
		return 0;
	ux0 -= 2;  uy0 -= 2;  ux1 += 2;  uy1 += 2;
	if ( ux0 < 0 ) ux0 = 0;
	if ( uy0 < 0 ) uy0 = 0;
	if ( ux1 > SHW ) ux1 = SHW;
	if ( uy1 > SHH ) uy1 = SHH;
	w = ux1 - ux0;
	h = uy1 - uy0;
	for ( z = 16; z > 2; z /= 2 )
		if ( (cright() - PALW) / z >= w && (canvh() - CANY) / z >= h )
			break;
	gsc = z;
	voxg = ux0 - ((cright() - PALW) / z - w) / 2;
	voyg = uy0 - ((canvh() - CANY) / z - h) / 2;
	clampvo();
	viewdirty();
	return 1;
}

/* ------------------------------------------------------------------ */
/* primitive drawing                                                  */
/* ------------------------------------------------------------------ */

/* Circles go through the ENGINE primitive (clgfx cl_circle), never a local
 * bare-cl_point loop: only a bracketed primitive coordinates with the
 * driver's cursor sprite -- bare points under the sprite were stomped by
 * the save-under restore (the "cursor erases the symbol" bug). */
/* XOR rect outline with no corner pixel plotted twice. */
static
xrect(x0, y0, x1, y1)
{
	register int t;

	if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
	if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
	cl_line(x0, y0, x1, y0, 2);
	if ( y1 > y0 )
		cl_line(x0, y1, x1, y1, 2);
	if ( y1 > y0 + 1 )
	{
		cl_line(x0, y0 + 1, x0, y1 - 1, 2);
		if ( x1 > x0 )
			cl_line(x1, y0 + 1, x1, y1 - 1, 2);
	}
	return 0;
}

/* An L-routed wire, horizontal leg first; mode as cl_line.  The vertical
 * leg starts one pixel past the corner so an XOR draw plots it once. */
static
lwire(x0, y0, x1, y1, mode)
{
	if ( y0 == y1 || x0 == x1 )
		cl_line(x0, y0, x1, y1, mode);
	else
	{
		cl_line(x0, y0, x1, y0, mode);
		cl_line(x1, y0 + (y1 > y0 ? 1 : -1), x1, y1, mode);
	}
	return 0;
}

/* Draw symbol si with orientation (rot,mir) at pixel (ox,oy), ppq px per
 * q unit (gsc/4 on the canvas, 1 in the palette previews). */
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
			if ( mir )	/* mirror flips angles, keeps CCW */
			{
				t = a0;
				a0 = 180 - a1;
				a1 = 180 - t;
			}
			a0 -= 90 * (rot & 3);	/* case-1 txq turn = -90 deg */
			a1 -= 90 * (rot & 3);
			arcline(x0, y0, p[3] * ppq, a0, a1, 0);
			p += 6;
		}
		else			/* ST: canvas scale only */
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

/* ------------------------------------------------------------------ */
/* junction dots                                                      */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* repainting                                                         */
/* ------------------------------------------------------------------ */

static
canvh()
{
	return conth - STH - SBW;	/* above the horizontal scrollbar */
}

/* The sheet's edge, drawn dotted as a construction guide (visible with
 * the construction layer, never printed).  The clipped variant draws
 * only the pieces inside a damage rect, with the dot phase anchored to
 * the edge's own start so partial repaints mesh with the rest. */
static
sheetclip(x0, y0, x1, y1)
{
	int e0x, e0y, e1x, e1y, a, b;

	e0x = gtopx(0);    e0y = gtopy(0);
	e1x = gtopx(SHW);  e1y = gtopy(SHH);
	cl_lpat(0xaaaa);
	a = x0 > e0x ? x0 : e0x;
	b = x1 < e1x ? x1 : e1x;
	if ( a <= b )
	{
		a = e0x + ((a - e0x + 1) & ~1);	/* keep the dot parity */
		if ( e0y >= y0 && e0y <= y1 && a <= b )
			cl_line(a, e0y, b, e0y, 0);
		if ( e1y >= y0 && e1y <= y1 && a <= b )
			cl_line(a, e1y, b, e1y, 0);
	}
	a = y0 > e0y ? y0 : e0y;
	b = y1 < e1y ? y1 : e1y;
	if ( a <= b )
	{
		a = e0y + ((a - e0y + 1) & ~1);
		if ( e0x >= x0 && e0x <= x1 && a <= b )
			cl_line(e0x, a, e0x, b, 0);
		if ( e1x >= x0 && e1x <= x1 && a <= b )
			cl_line(e1x, a, e1x, b, 0);
	}
	cl_lpat(0xffff);
	return 0;
}

static
cright()
{
	return contw - SBW;		/* left of the vertical scrollbar */
}

/* One object onto the canvas (skips those wholly outside the view, and
 * every object on an invisible layer). */
static
drawobj(i)
{
	register DOBJ *o;
	int bx0, by0, bx1, by1, x0, y0, x1, y1, t, fl, fill;
	long dx, dy, r;

	o = &obj[i];
	if ( !layvis[o->o_layer] )
		return 0;
	objpbox(i, &bx0, &by0, &bx1, &by1);
	if ( bx1 < PALW || bx0 > contw || by1 < 0 || by0 > canvh() )
		return 0;
	fl = o->o_flags;
	switch ( o->o_type )
	{
	case OT_SYM:
		drawsymat(o->o_sym, o->o_rot, o->o_mir,
			  gtopx(o->o_x), gtopy(o->o_y),
			  gsc >= 4 ? gsc / 4 : 1);
		/* Designator and value: above and below a horizontal body, but
		 * BESIDE a vertical one (odd rotation) -- stacked vertical parts
		 * sit close, and above/below labels of neighbours collide. */
		{
			register int lx, ny, vy;

			if ( o->o_rot & 1 )
			{
				lx = bx1 + 3;
				ny = by0 + 2;
				vy = by0 + 11;
			}
			else
			{
				lx = bx0;
				ny = by0 - 9;
				vy = by1 + 2;
			}
			if ( o->o_name[0] )
				cl_ptextt(SHM_FICON, lx, ny, o->o_name);
			if ( o->o_val[0] )
				cl_ptextt(SHM_FICON, lx, vy, o->o_val);
		}
		break;

	case OT_WIRE:
		lwire(gtopx(o->o_x), gtopy(o->o_y),
		      gtopx(o->o_x2), gtopy(o->o_y2), 0);
		break;

	case OT_LINE:
		stpat(fl);
		sline(gtopx(o->o_x), gtopy(o->o_y),
		      gtopx(o->o_x2), gtopy(o->o_y2), 0, fl);
		cl_lpat(0xffff);
		break;

	case OT_BOX:
	case OT_SHAPE:
		x0 = gtopx(o->o_x);   y0 = gtopy(o->o_y);
		x1 = gtopx(o->o_x2);  y1 = gtopy(o->o_y2);
		if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
		if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
		if ( fl & OF_FILL )
			fillshape(o->o_type == OT_BOX ? SH_BOX : o->o_sym,
				  x0, y0, x1, y1, fillval(fl));
		if ( o->o_type == OT_SHAPE )
		{
			shapeoutline(o->o_sym, x0, y0, x1, y1, fl);
			shlabel(oval(o), x0, y0, x1, y1);
			break;
		}
		stpat(fl);
		sline(x0, y0, x1, y0, 0, fl);
		sline(x1, y0, x1, y1, 0, fl);
		sline(x1, y1, x0, y1, 0, fl);
		sline(x0, y1, x0, y0, 0, fl);
		cl_lpat(0xffff);
		break;

	case OT_CIRC:
		dx = (long)(o->o_x2 - o->o_x) * gsc;
		dy = (long)(o->o_y2 - o->o_y) * gsc;
		r = isqrt(dx * dx + dy * dy);
		x0 = gtopx(o->o_x);
		y0 = gtopy(o->o_y);
		if ( fl & OF_FILL )
			fillshape(SH_CIRC, x0 - (int)r, y0 - (int)r,
				  x0 + (int)r, y0 + (int)r, fillval(fl));
		scirc(x0, y0, (int)r, 0, fl);
		break;

	case OT_ARC:
		arcst(gtopx(o->o_x), gtopy(o->o_y), o->o_x2 * gsc,
		      OA0(o), OA1(o), 0, fl);
		break;

	case OT_TEXT:
		{
			register char *s, *e;
			char lbuf[TVMAX];
			int fs, ly;

			fs = fontslot(o->o_rot);
			if ( fl & OF_VERT )	/* turned 90: glyph transpose */
			{
				vtext(fs, gtopx(o->o_x), gtopy(o->o_y),
				      oval(o));
				break;
			}
			ly = gtopy(o->o_y);
			s = oval(o);
			for (;;)
			{
				for ( e = s, t = 0; *e && *e != '|'; e++ )
					lbuf[t++] = *e;
				lbuf[t] = 0;
				cl_ptextt(fs, gtopx(o->o_x), ly, lbuf);
				if ( *e == 0 )
					break;
				s = e + 1;
				ly += (o->o_rot <= 0) ? 8 :
				      (o->o_rot == 1) ? 15 : 16;
			}
		}
		break;

	case OT_CONN:
		x0 = gtopx(o->o_x);   y0 = gtopy(o->o_y);
		x1 = gtopx(o->o_x2);  y1 = gtopy(o->o_y2);
		stpat(fl);
		if ( o->o_sym == CS_HV || o->o_sym == CS_HARROW )
		{
			if ( y0 == y1 || x0 == x1 )
				sline(x0, y0, x1, y1, 0, fl);
			else
			{
				sline(x0, y0, x1, y0, 0, fl);
				sline(x1, y0, x1, y1, 0, fl);
			}
		}
		else
			sline(x0, y0, x1, y1, 0, fl);
		cl_lpat(0xffff);
		if ( o->o_sym == CS_ARROW )
			arrowhead(x1, y1, x1 - x0, y1 - y0);
		else if ( o->o_sym == CS_HARROW )
		{
			if ( y1 != y0 )
				arrowhead(x1, y1, 0, y1 - y0);
			else
				arrowhead(x1, y1, x1 - x0, 0);
		}
		break;

	case OT_POLY:
		{
			int pxy[2 * PMAXPT];

			for ( t = 0; t < o->o_sym; t++ )
			{
				pxy[2*t] = gtopx(ppool[o->o_x2 + 2*t]);
				pxy[2*t + 1] = gtopy(ppool[o->o_x2 + 2*t + 1]);
			}
			if ( fl & OF_FILL )	/* v4: filled polylines */
				fillpoly(pxy, (int)o->o_sym, fillval(fl));
			if ( (fl & OF_SMOOTH) && o->o_sym >= 3 )
			{
				drawspline(pxy, (int)o->o_sym, fl);
				break;
			}
			stpat(fl);
			for ( t = 1; t < o->o_sym; t++ )
				sline(pxy[2*t - 2], pxy[2*t - 1],
				      pxy[2*t], pxy[2*t + 1], 0, fl);
			cl_lpat(0xffff);
		}
		break;

	case OT_NNAME:
		x0 = gtopx(o->o_x);
		y0 = gtopy(o->o_y);
		cl_fillrect(x0 - 1, y0 - 1, x0 + 2, y0 + 2, 0);
		cl_ptextt(SHM_FICON, x0 + 3, y0 - 9, o->o_name);
		break;

	case OT_DIM:
		{
			char db[DIMLBL];

			dimlbl(o, db);
			dimdraw(gtopx(o->o_x), gtopy(o->o_y),
				gtopx(o->o_x2), gtopy(o->o_y2), db);
		}
		break;
	}
	return 0;
}

/* The dotted (50% gray) outline round one selected object; a shape that
 * is the SINGLE selection also gets 5x5 inverted corner HANDLES -- the
 * resize grips (hit-tested before body picks). */
static
selbox1(i)
{
	int x0, y0, x1, y1, hx, hy, k;

	objpbox(i, &x0, &y0, &x1, &y1);
	x0 -= 3;  y0 -= 3;  x1 += 4;  y1 += 4;
	cl_fillrect(x0, y0, x1, y0 + 1, 3);
	cl_fillrect(x0, y1 - 1, x1, y1, 3);
	cl_fillrect(x0, y0 + 1, x0 + 1, y1 - 1, 3);
	cl_fillrect(x1 - 1, y0 + 1, x1, y1 - 1, 3);
	if ( obj[i].o_type == OT_SHAPE && nsel == 1 && selobj == i )
	{
		objpbox(i, &x0, &y0, &x1, &y1);
		for ( k = 0; k < 4; k++ )
		{
			hx = (k & 1) ? x1 : x0;
			hy = (k & 2) ? y1 : y0;
			cl_fillrect(hx - 2, hy - 2, hx + 3, hy + 3, 2);
		}
	}
	else if ( nsel == 1 && selobj == i )
	{
		register DOBJ *o;

		/* endpoint / vertex grips: dragging one re-routes and
		 * RE-SNAPS (sec. 15) -- drawn like the shape handles */
		o = &obj[i];
		switch ( o->o_type )
		{
		case OT_LINE:
		case OT_WIRE:
		case OT_CONN:
		case OT_DIM:
			for ( k = 0; k < 2; k++ )
			{
				hx = gtopx(k ? o->o_x2 : o->o_x);
				hy = gtopy(k ? o->o_y2 : o->o_y);
				cl_fillrect(hx - 2, hy - 2, hx + 3, hy + 3, 2);
			}
			break;
		case OT_POLY:
			for ( k = 0; k < o->o_sym; k++ )
			{
				hx = gtopx(ppool[o->o_x2 + 2*k]);
				hy = gtopy(ppool[o->o_x2 + 2*k + 1]);
				cl_fillrect(hx - 2, hy - 2, hx + 3, hy + 3, 2);
			}
			break;
		}
	}
	return 0;
}

/* Union pixel bbox of the whole selection; 0 when nothing is selected. */
static
selbbox(bx0, by0, bx1, by1)
int *bx0, *by0, *bx1, *by1;
{
	register int i;
	int x0, y0, x1, y1, got;

	got = 0;
	for ( i = 0; i < nobj; i++ )
	{
		if ( !osel[i] )
			continue;
		objpbox(i, &x0, &y0, &x1, &y1);
		if ( !got )
		{
			*bx0 = x0;  *by0 = y0;  *bx1 = x1;  *by1 = y1;
			got = 1;
		}
		else
		{
			if ( x0 < *bx0 ) *bx0 = x0;
			if ( y0 < *by0 ) *by0 = y0;
			if ( x1 > *bx1 ) *bx1 = x1;
			if ( y1 > *by1 ) *by1 = y1;
		}
	}
	return got;
}

static	repaint_rect();

static
drawcanvas()
{
	/* The whole view IS one big damage rect: the clipped workhorse
	 * below repaints it exactly (grid, sheet edge, objects, junction
	 * dots, anchor, selection) -- ONE code path, not two copies.  The
	 * chrome-spill flags it raises are already set by viewdirty(). */
	repaint_rect(PALW, CANY, cright(), canvh());
	return 0;
}

/* Repaint ONE canvas rectangle: white it, re-dot the grid inside it, and
 * redraw whatever overlaps it (an overlapping object is redrawn whole --
 * every draw here is idempotent, so pixels outside the rect just repaint
 * what is already there).  This is the workhorse that replaced the
 * everything-repaints scheme: placing a symbol costs its own bbox now. */
static
repaint_rect(x0, y0, x1, y1)
{
	register int i;
	int st, gx0, gx1, gy, py, px, nx, jr;
	int bx0, by0, bx1, by1;

	if ( x0 < PALW ) x0 = PALW;
	if ( y0 < CANY ) y0 = CANY;
	if ( x1 > cright() ) x1 = cright();
	if ( y1 > canvh() ) y1 = canvh();
	if ( x0 >= x1 || y0 >= y1 )
		return 0;
	cl_fillrect(x0, y0, x1, y1, 1);
	if ( gridon )
	{
		st = (gsc >= GRID) ? 1 : (gsc >= 4) ? 2 : 4;
		if ( st < gridstep )
			st = gridstep;
		gx0 = voxg + (x0 - PALW + gsc - 1) / gsc;
		gx0 = ((gx0 + st - 1) / st) * st;
		gx1 = voxg + (x1 - 1 - PALW) / gsc;
		if ( gx1 > SHW )
			gx1 = SHW;
		nx = (gx1 - gx0) / st + 1;
		gy = voyg + (y0 - CANY + gsc - 1) / gsc;
		gy = ((gy + st - 1) / st) * st;
		if ( nx > 0 )
			for ( ; gy <= SHH; gy += st )
			{
				py = gtopy(gy);
				if ( py >= y1 )
					break;
				cl_dotrow(gtopx(gx0), py, nx, st * gsc);
			}
	}
	if ( layvis[3] )
		sheetclip(x0, y0, x1 - 1, y1 - 1);
	for ( i = 0; i < nobj; i++ )
	{
		objpbox(i, &bx0, &by0, &bx1, &by1);
		if ( bx1 + 48 < x0 || bx0 - 5 > x1 ||
		     by1 + 12 < y0 || by0 - 12 > y1 )
			continue;
		drawobj(i);
		if ( osel[i] )
			selbox1(i);
		/* an object drawn WHOLE that reaches past the canvas rect
		 * spilled onto the chrome: flag those pieces so flush()
		 * repaints them (after us) to cover it */
		if ( bx0 - 5 < PALW )
			ddpal = 1;
		if ( by0 - 12 < CANY )
		{
			ddtbar = 1;
			if ( by0 - 12 < CTOP )
				ddtop = 1;
		}
		if ( bx1 + 48 > cright() || by1 + 12 > canvh() )
			ddsb = 1;
		if ( by1 + 12 > canvh() + SBW )
		{
			statdirty = 1;
			s_valid = 0;
		}
	}
	jr = gsc / 4;
	for ( i = 0; i < njunc; i++ )
	{
		px = gtopx(juncx[i]);
		py = gtopy(juncy[i]);
		if ( px + jr < x0 || px - jr > x1 ||
		     py + jr < y0 || py - jr > y1 )
			continue;
		cl_fillrect(px - jr, py - jr, px + jr, py + jr, 0);
	}
	if ( wanchor )
	{
		px = gtopx(wax);
		py = gtopy(way);
		if ( px + 4 >= x0 && px - 4 <= x1 &&
		     py + 4 >= y0 && py - 4 <= y1 )
		{
			cl_line(px - 4, py - 4, px + 4, py + 4, 0);
			cl_line(px - 4, py + 4, px + 4, py - 4, 0);
		}
	}
	return 0;
}

/* One palette cell frame; on = draw it pressed (inverted). */
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

/* The symbol bank shows ONE LIBRARY GROUP at a time: palidx[] is the
 * view (symtab indices of the current group), the header cell names the
 * group and a click on it switches to the next, and the bank still
 * SCROLLS for small windows (palrow + the arrow cells at its foot).
 *
 * Group nlib is the virtual "shapes" group: the parametric S shapes,
 * the four connector styles, the Arc and the Net cells -- discovered
 * the same way symbols are. */
#define	ABAR	14		/* arrow bar height at the palette foot   */
#define	SHGRP	(nlib)		/* the shapes pseudo-group's index        */
#define	PC_ARC	(NSHAPE + NCONNS)	/* its cell layout                */
#define	PC_NET	(NSHAPE + NCONNS + 1)
#define	PC_DIM	(NSHAPE + NCONNS + 2)
#define	NPCELL	(NSHAPE + NCONNS + 3)
int	palrow;
short	palidx[MAXSYM];		/* the bank: symtab indices of curlib     */
int	npal;
int	palsh;			/* 1 = the shapes group is showing        */

palview()
{
	register int i;

	palsh = (curlib == SHGRP);
	npal = 0;
	if ( palsh )
		npal = NPCELL;
	else
		for ( i = 0; i < nsym; i++ )
			if ( symtab[i].sy_lib == curlib )
				palidx[npal++] = i;
	palrow = 0;
	return 0;
}

/* What is armed from the palette, as ONE code for the state compare
 * (symbols: the symtab index; shapes group entries: >= 1000). */
static
armcode()
{
	if ( cursym >= 0 )	return cursym;
	if ( curshape >= 0 )	return 1000 + curshape;
	if ( curconn >= 0 )	return 1000 + NSHAPE + curconn;
	if ( arcarm )		return 1000 + PC_ARC;
	if ( netarm )		return 1000 + PC_NET;
	if ( dimarm )		return 1000 + PC_DIM;
	return -1;
}

/* Disarm every palette arming (tool changes, ESC). */
static
disarm()
{
	cursym = -1;
	curshape = -1;
	curconn = -1;
	arcarm = 0;
	netarm = 0;
	dimarm = 0;
	arcpend = 0;
	return 0;
}

/* End the Line tool's polyline chain. */
static
endchain()
{
	lchain = -1;
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

	v = (conth - STH - ABAR - CANY - LHDR) / SCH;
	if ( v < 1 )
		v = 1;
	return v;
}

/* ---- the palette BANK -------------------------------------------------
 * Painted in place: the bank was a separate process (velpal) for one
 * release, on the zdock widget pattern, when the editor's text was
 * believed to be against a hard 64 K wall; `ld -L' dissolved that, and
 * the helper cost a SECOND parsed copy of every stencil library plus a
 * shared-memory sync block, so it is back where it is drawn.  Its cell
 * coordinates were the palette rect's own -- here every y is a WINDOW y:
 * the bank starts at CANY and its foot is the arrow bar above the status
 * line.  The damage discipline is unchanged (flush()): a full bank pass
 * only when ddpal or the row moved, and an arming change repaints
 * exactly the two cells whose state flipped. ---- */

/* the palette rect's height */
static
palh()
{
	return conth - STH - CANY;
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

/* ONE bank cell (bank position k), armed state `arm': a symbol preview,
 * or -- shapes group -- a mini parametric shape, connector glyph, arc,
 * net or dimension cell.  A k outside the visible rows is a no-op, so a
 * palcellof() miss (-1) costs nothing. */
static
palbank1(k, arm)
{
	register SYMDEF *s;
	int x, y, ox, oy;

	if ( k < palrow * 2 || k >= (palrow + palvis()) * 2 || k >= npal )
		return 0;
	x = (k & 1) * SCW;
	y = CANY + LHDR + ((k >> 1) - palrow) * SCH;
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
	int y, w, vis, maxr, ch, arm;

	ch = palh();
	vis = palvis();
	maxr = palrows() - vis;
	if ( maxr < 0 )
		maxr = 0;
	if ( palrow > maxr )
		palrow = maxr;
	arm = armcode();
	cl_fillrect(0, CANY, PALW, CANY + ch, 1);
	{
		register char *gn;

		gn = palsh ? "shapes" : (nlib ? libname[curlib] : "-");
		w = strlen(gn) * 6;
		cl_ptext(SHM_FICON, (PALW - w) / 2, CANY + (LHDR - 8) / 2, gn);
	}
	palcell(0, CANY, PALW, LHDR, 0);
	for ( k = palrow * 2; k < npal && k < (palrow + vis) * 2; k++ )
		palbank1(k, arm);
	y = CANY + ch - ABAR;
	palarrow(0, y, 0);
	palarrow(SCW, y, 1);
	palcell(0, y, SCW, ABAR, 0);
	palcell(SCW, y, SCW, ABAR, 0);
	cl_line(PALW - 1, CANY, PALW - 1, CANY + ch - 1, 0);
	return 0;
}

/* Is toolbar cell i shown pressed right now?  (The C_LAYER cell is not
 * a toggle: its "state" is the active-layer NUMBER, so a layer change
 * makes flush()'s state comparison repaint the cell's label.) */
static
tbstate(i)
{
	register int at;

	if ( i == C_GRID )
		return gridon;
	if ( i == C_LAYER )
		return curlayer;
	at = (i < NTOOL) ? i : (i >= C_ROT && i <= C_DUP) ? i - 1 : -1;
	return at >= 0 && at == tool && cursym < 0 && curshape < 0 &&
	       curconn < 0 && !arcarm && !netarm && !dimarm;
}

/* Draw ONE toolbar cell in state `on' (background wiped first, so a cell
 * repaints in place when only its state changed). */
static
tbcell(i, on)
{
	int x, w;

	if ( i == C_LAYER )
	{
		laylbl[1] = '0' + curlayer;
		on = 0;
	}
	x = i * TBW;
	cl_fillrect(x, CTOP, x + TBW, CTOP + TBH, 1);
	w = strlen(toolnm[i]) * 6;
	cl_ptext(SHM_FICON, x + (TBW - w) / 2, CTOP + (TBH - 8) / 2,
		 toolnm[i]);
	palcell(x, CTOP, TBW, TBH, on);
	return 0;
}

/* The toolbar: one row of NCELL cells under the file bar. */
static
drawtbar()
{
	register int i;

	cl_fillrect(0, CTOP, contw, CANY, 1);
	for ( i = 0; i < NCELL; i++ )
		tbcell(i, tbstate(i));
	cl_line(0, CANY - 1, contw - 1, CANY - 1, 0);
	return 0;
}

/* ---- the canvas scrollbars (right + bottom): proportional thumb over a
 * gray track; drag the thumb, or click the track to page. ---- */

static
sbgeom(vert, tx, tl, thx, thw)
int *tx, *tl, *thx, *thw;
{
	int vw, total, pos;

	*tx = vert ? CANY : PALW;
	*tl = vert ? canvh() - CANY : cright() - PALW;
	total = vert ? SHH : SHW;
	pos = vert ? voyg : voxg;
	vw = *tl / gsc;
	*thw = (long)*tl * vw / total;
	if ( *thw < SBW ) *thw = SBW;
	if ( *thw > *tl ) *thw = *tl;
	*thx = *tx + (long)(*tl - *thw) * pos /
	       (total - vw > 0 ? total - vw : 1);
	return vw;
}

/* A scrollbar thumb: white body + 1-px border inside (x0,y0)-(x1,y1). */
static
thumb(x0, y0, x1, y1)
{
	cl_fillrect(x0, y0, x1, y1, 1);
	cl_line(x0, y0, x1 - 1, y0, 0);
	cl_line(x0, y1 - 1, x1 - 1, y1 - 1, 0);
	cl_line(x0, y0, x0, y1 - 1, 0);
	cl_line(x1 - 1, y0, x1 - 1, y1 - 1, 0);
	return 0;
}

static
drawsb()
{
	int tx, tl, thx, thw;

	sbgeom(0, &tx, &tl, &thx, &thw);
	cl_fillrect(tx, canvh(), tx + tl, canvh() + SBW, 3);
	thumb(thx, canvh() + 1, thx + thw, canvh() + SBW - 1);

	sbgeom(1, &tx, &tl, &thx, &thw);
	cl_fillrect(cright(), tx, contw, tx + tl, 3);
	thumb(cright() + 1, thx, contw - 1, thx + thw);

	cl_fillrect(cright(), canvh(), contw, canvh() + SBW, 1);  /* corner */
	return 0;
}

/* The title bar over the canvas: the FILE, like zedit's status line --
 * kept apart from the tool/coordinate chatter below on purpose. */
static
drawtop()
{
	char t[56];

	cl_fillrect(0, 0, contw, CTOP, 1);
	cl_line(0, CTOP - 1, contw - 1, CTOP - 1, 0);
	sprintf(t, "%s%s", fname[0] ? fname : "(untitled)",
		modified ? " *" : "");
	cl_ptext(SHM_FUI, 5, 1, t);
	/* the < > sheet-set cells, at the bar's right edge: the sheet is
	 * a property of the FILE, so they live on the file line (v4) */
	cl_ptext(SHM_FUI, contw - 2 * SHCW + 6, 0, "<");
	cl_ptext(SHM_FUI, contw - SHCW + 6, 0, ">");
	palcell(contw - 2 * SHCW, 0, SHCW, CTOP - 1, 0);
	palcell(contw - SHCW, 0, SHCW, CTOP - 1, 0);
	return 0;
}

/* The status line, in FIELDS -- tool name, pointer position, the rest
 * (hint / selection / zoom / pan) -- each compared against what is on
 * screen and repainted alone: a moving pointer rewrites the seven glyphs
 * of the position cell, nothing else. */
#define	STF_NM	5		/* field origins, px                      */
#define	STF_CO	68
#define	STF_RS	140
char	s_nm[12];		/* what each field currently shows        */
char	s_co[16];
char	s_rest[96];		/* (s_valid lives with the damage flags)  */
char	scmsg[10];		/* transient one-shot note ("rounded")    */

static
drawstat()
{
	char nm[12], co[16], rest[96];
	register int i;
	char *h;
	int by;

	/* what is armed / active: ONE ladder feeding the name field AND the
	 * hint -- per state a base hint and (slot 2k+1) the anchored-run
	 * variant wanchor (the arc: arcpend) switches to */
	{
		static char *hint2[] = {
			"click places (Rot/Mir turn it)", (char *)0,
			"drag its box, then type the label", (char *)0,
			"drag or click ends; snaps to shapes",
			"now click the far end",
			"drag centre to the start point",
			"click where the arc ends",
			"click the point to name", (char *)0,
			"drag point to point; snaps",
			"now click the far point",
			"click picks, drag moves", (char *)0,
			"drag pin to pin, or click each end",
			"now click the far end",
			"drag or click ends; runs chain",
			"next point ends here",
			"drag corner to corner", (char *)0,
			"drag centre to edge", (char *)0,
			"click where the text goes", (char *)0,
			"click what should go", (char *)0,
			"click what to turn", (char *)0,
			"click what to flip", (char *)0,
			"click what to rename", (char *)0,
			"click what to copy", (char *)0,
		};
		register char *an;
		register int k;

		an = (char *)0;
		if ( cursym >= 0 )	  { k = 0;  an = symtab[cursym].sy_code; }
		else if ( curshape >= 0 ) { k = 1;  an = shname[curshape]; }
		else if ( curconn >= 0 )  { k = 2;  an = csname[curconn]; }
		else if ( arcarm )	  { k = 3;  an = "arc"; }
		else if ( netarm )	  { k = 4;  an = "net"; }
		else if ( dimarm )	  { k = 5;  an = "dim"; }
		else			  k = 6 + tool;
		h = hint2[2 * k + 1];
		if ( h == (char *)0 || !(k == 3 ? arcpend : wanchor) )
			h = hint2[2 * k];
		if ( an )
			sprintf(nm, "+%.7s", an);
		else
			/* cell label for the tool: the Grid cell sits between
			 * the two tool groups, so the ids skew by one */
			sprintf(nm, "%.7s",
				toolnm[tool < NTOOL ? tool : tool + 1]);
	}
	if ( drag == DR_END ||
	     (drag == DR_TOOL && (dtool == DT_WIRE || dtool == DT_LINE ||
	      dtool == DT_CONN || dtool == DT_DIM)) )
	{
		/* the run's length TIMES the sheet unit: measuring without
		 * committing a dimension (VELLUM.md sec. 30) */
		long dxl, dyl, dl;

		dxl = cgx - dgx;	if ( dxl < 0 ) dxl = -dxl;
		dyl = cgy - dgy;	if ( dyl < 0 ) dyl = -dyl;
		dl = (dyl == 0) ? dxl : (dxl == 0) ? dyl :
		     isqrt(dxl * dxl + dyl * dyl);
		sprintf(co, "%ld %.4s", dl * unum, uname);
	}
	else
		sprintf(co, "%3d,%-3d", lastgx, lastgy);
	strcpy(rest, h);
	if ( nsel > 1 )
		sprintf(rest + strlen(rest), "  (%d sel)", nsel);
	if ( gsc != GRID )
		strcat(rest, gsc > GRID ? "  x2" : "  x1/2");
	if ( voxg || voyg )
		sprintf(rest + strlen(rest), "  [%d,%d]", voxg, voyg);
	if ( curlayer )
		sprintf(rest + strlen(rest), "  L%d", curlayer);
	if ( scmsg[0] )
	{
		/* a one-shot note (scale's "rounded"): shown until the
		 * next status repaint replaces it */
		sprintf(rest + strlen(rest), "  %s", scmsg);
		scmsg[0] = 0;
	}

	by = conth - STH;
	{
		/* the three fields, table-driven: each compared against the
		 * shown copy and repainted alone (or all, on a fresh surface) */
		static int forg[4] = { STF_NM, STF_CO, STF_RS, 0 };
		char *news[3], *shown[3];
		int full;

		news[0] = nm;    shown[0] = s_nm;
		news[1] = co;    shown[1] = s_co;
		news[2] = rest;  shown[2] = s_rest;
		forg[3] = contw + 2;
		full = !s_valid;
		if ( full )
		{
			cl_fillrect(0, by, contw, conth, 1);
			cl_line(0, by, contw - 1, by, 0);
			s_valid = 1;
		}
		for ( i = 0; i < 3; i++ )
		{
			if ( !full && strcmp(news[i], shown[i]) == 0 )
				continue;
			if ( !full )
				cl_fillrect(forg[i], by + 1, forg[i + 1] - 2,
					    conth, 1);
			cl_ptext(SHM_FUI, forg[i], by + 2, news[i]);
			strcpy(shown[i], news[i]);
		}
	}
	return 0;
}

/* THE painter: repaint exactly what the accumulated damage says, and the
 * chrome pieces whose state no longer matches the screen.  Everything the
 * main loop shows goes through here, once per event batch. */
static
flush()
{
	register int i;
	int on, gw, gx0, gy0, gx1, gy1;

	/* The deferred ghost: take the one on screen off EVERY flush, by
	 * damage-repaint of its recorded rect, and repaint it fresh below.
	 * Unconditional on purpose -- a conditional keep ("same place, no
	 * overlap") accumulated edge cases under bursty motion (overlapping
	 * inverted fills stranded symmetric-difference fragments); a ~35x15
	 * repaint per batch is nothing, and the recorded rect always covers
	 * every pixel the (possibly clipped) fill actually touched. */
	gw = (cursym >= 0 && ghpos);
	if ( gw )
		sympbox(cursym, placerot, placemir, ghgx, ghgy,
			&gx0, &gy0, &gx1, &gy1);
	if ( ghdrawn )
	{
		dmg(ghx0, ghy0, ghx1 + 1, ghy1 + 1);
		ghdrawn = 0;
	}
	if ( ddcanv || ddrect )
		ruboff();	/* erase any XOR figure before repainting */
	if ( ddcanv )
	{
		drawcanvas();
		ddcanv = 0;
		ddrect = 0;
	}
	else if ( ddrect )
	{
		repaint_rect(drx0, dry0, drx1, dry1);
		ddrect = 0;
	}
	if ( gw && !ghdrawn )
	{
		cl_fillrect(gx0, gy0, gx1 + 1, gy1 + 1, 2);
		ghx0 = gx0;  ghy0 = gy0;  ghx1 = gx1;  ghy1 = gy1;
		ghdrawn = 1;
	}
	if ( ddpal || shownrow != palrow )
	{
		drawpal();
		shownrow = palrow;
		showncur = armcode();
		ddpal = 0;
	}
	else if ( showncur != armcode() )
	{
		on = armcode();		/* exactly the two cells that flipped */
		if ( showncur >= 0 )
			palbank1(palcellof(showncur), on);
		if ( on >= 0 )
			palbank1(palcellof(on), on);
		showncur = on;
	}
	if ( ddtbar || !tbvalid )
	{
		drawtbar();
		for ( i = 0; i < NCELL; i++ )
			tbon[i] = tbstate(i);
		tbvalid = 1;
		ddtbar = 0;
	}
	else
		for ( i = 0; i < NCELL; i++ )
		{
			on = tbstate(i);
			if ( tbon[i] != on )
			{
				tbcell(i, on);
				tbon[i] = on;
			}
		}
	if ( ddtop || modified != shownmod )
	{
		drawtop();
		shownmod = modified;
		ddtop = 0;
	}
	if ( ddsb )
	{
		drawsb();
		ddsb = 0;
	}
	if ( statdirty )
	{
		drawstat();
		statdirty = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* the selection set                                                  */
/* ------------------------------------------------------------------ */

/* Make object i the ONLY selection -- a member of a persistent GROUP
 * selects the whole group (sec. 5: grouping changes what a click
 * selects, nothing else). */
static
selone(i)
{
	register int j;

	selclear();
	if ( obj[i].o_grp )
	{
		for ( j = 0; j < nobj; j++ )
			if ( obj[j].o_grp == obj[i].o_grp )
			{
				osel[j] = 1;
				nsel++;
			}
		selobj = (nsel == 1) ? i : -1;
		return 0;
	}
	osel[i] = 1;
	nsel = 1;
	selobj = i;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the object list                                                    */
/* ------------------------------------------------------------------ */

delobj(i)
{
	register int j;
	register DOBJ *o;

	o = &obj[i];
	tvfree(o);			/* a pooled value's block goes too */
	if ( o->o_type == OT_POLY )	/* free its point-pool block */
	{
		int base, len;

		base = o->o_x2;
		len = 2 * o->o_sym;
		for ( j = base; j + len < ppuse; j++ )
			ppool[j] = ppool[j + len];
		ppuse -= len;
		for ( j = 0; j < nobj; j++ )
			if ( obj[j].o_type == OT_POLY && obj[j].o_x2 > base )
				obj[j].o_x2 -= len;
	}
	if ( osel[i] )
		nsel--;
	for ( j = i; j < nobj - 1; j++ )
	{
		obj[j] = obj[j + 1];
		osel[j] = osel[j + 1];
	}
	nobj--;
	for ( j = 0; j < nobj; j++ )	/* connector refs follow the shift */
	{
		o = &obj[j];
		if ( o->o_type != OT_CONN )
			continue;
		if ( ACOBJ(o, 0) == i )		ACOBJ(o, 0) = -1;
		else if ( ACOBJ(o, 0) > i )	ACOBJ(o, 0)--;
		if ( ACOBJ(o, 1) == i )		ACOBJ(o, 1) = -1;
		else if ( ACOBJ(o, 1) > i )	ACOBJ(o, 1)--;
	}
	if ( selobj == i )
		selobj = -1;
	else if ( selobj > i )
		selobj--;
	if ( lchain == i )
		lchain = -1;
	else if ( lchain > i )
		lchain--;
	modified = 1;
	rejunc();
	return 0;
}

/* Next free designator for prefix p: scan what is used, go one higher. */
static
nextdes(p, out)
char *p, *out;
{
	register int i, n;
	int hi, pl;

	pl = strlen(p);
	if ( pl == 0 )
	{
		out[0] = 0;
		return 0;
	}
	hi = 0;
	for ( i = 0; i < nobj; i++ )
	{
		if ( obj[i].o_type != OT_SYM )
			continue;
		if ( strncmp(obj[i].o_name, p, pl) != 0 )
			continue;
		n = atoi(obj[i].o_name + pl);
		if ( n > hi )
			hi = n;
	}
	sprintf(out, "%s%d", p, hi + 1);
	return 0;
}

/* Place an armed symbol at grid (gx,gy).  Returns the new index or -1. */
static
placesym(gx, gy)
{
	register DOBJ *o;

	if ( nobj >= MAXOBJ || cursym < 0 )
		return -1;
	o = &obj[nobj];
	o->o_type = OT_SYM;
	o->o_sym = cursym;
	o->o_rot = placerot;
	o->o_mir = placemir;
	o->o_x = gx;
	o->o_y = gy;
	o->o_x2 = o->o_y2 = 0;
	o->o_flags = 0;
	o->o_layer = curlayer;
	o->o_grp = 0;
	nextdes(symtab[cursym].sy_pfx, o->o_name);
	o->o_val[0] = 0;
	nobj++;
	modified = 1;
	return nobj - 1;
}

addobj(type, x, y, x2, y2)
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
	o->o_flags = 0;
	o->o_layer = curlayer;
	o->o_grp = 0;
	for ( i = 0; i < NAMEL; i++ )	/* whole field: the ON() overlay */
		o->o_name[i] = 0;
	o->o_val[0] = 0;
	if ( type == OT_CONN )
	{
		ACOBJ(o, 0) = -1;
		ACOBJ(o, 1) = -1;
	}
	if ( type == OT_TEXT )
		o->o_rot = 2;		/* default size: the 9x16 UI font */
	nobj++;
	modified = 1;
	if ( type == OT_WIRE )
		rejunc();
	return nobj - 1;
}

/* ------------------------------------------------------------------ */
/* hit testing                                                        */
/* ------------------------------------------------------------------ */

/* Distance from pixel (px,py) to segment (x0,y0)-(x1,y1) <= 3? */
static
nearseg(px, py, x0, y0, x1, y1)
{
	int bx0, by0, bx1, by1, t;
	long cross, len;

	bx0 = x0;  bx1 = x1;
	if ( bx1 < bx0 ) { t = bx0; bx0 = bx1; bx1 = t; }
	by0 = y0;  by1 = y1;
	if ( by1 < by0 ) { t = by0; by0 = by1; by1 = t; }
	if ( px < bx0 - 3 || px > bx1 + 3 || py < by0 - 3 || py > by1 + 3 )
		return 0;
	cross = (long)(px - x0) * (y1 - y0) - (long)(py - y0) * (x1 - x0);
	if ( cross < 0 )
		cross = -cross;
	len = isqrt((long)(x1 - x0) * (x1 - x0) +
		    (long)(y1 - y0) * (y1 - y0));
	if ( len == 0 )
		len = 1;
	return cross <= 3 * len;
}

/* Topmost object under canvas pixel (px,py), or -1. */
static
pick(px, py)
{
	register int i;
	register DOBJ *o;
	int x0, y0, x1, y1;
	long dx, dy, r, d;

	for ( i = nobj - 1; i >= 0; i-- )
	{
		o = &obj[i];
		if ( !layvis[o->o_layer] )
			continue;
		switch ( o->o_type )
		{
		case OT_SYM:
		case OT_TEXT:
		case OT_SHAPE:
		case OT_NNAME:
			objpbox(i, &x0, &y0, &x1, &y1);
			if ( px >= x0 - 2 && px <= x1 + 2 &&
			     py >= y0 - 2 && py <= y1 + 2 )
				return i;
			break;

		case OT_POLY:
			{
				register int k;

				for ( k = 1; k < o->o_sym; k++ )
					if ( nearseg(px, py,
					     gtopx(ppool[o->o_x2 + 2*k - 2]),
					     gtopy(ppool[o->o_x2 + 2*k - 1]),
					     gtopx(ppool[o->o_x2 + 2*k]),
					     gtopy(ppool[o->o_x2 + 2*k + 1])) )
						return i;
			}
			break;

		case OT_ARC:
			{
				int a, a0, a1;

				r = (long)o->o_x2 * gsc;
				dx = px - gtopx(o->o_x);
				dy = py - gtopy(o->o_y);
				d = isqrt(dx * dx + dy * dy);
				if ( d < r - 3 || d > r + 3 )
					break;
				a = iangle((int)dx, (int)-dy);
				a0 = OA0(o);
				a1 = OA1(o);
				while ( a1 <= a0 )
					a1 += 360;
				while ( a < a0 )
					a += 360;
				if ( a <= a1 )
					return i;
			}
			break;

		case OT_WIRE:
		case OT_CONN:
		case OT_LINE:
		case OT_DIM:
		case OT_BOX:
			/* the two-point family shares its transforms; the
			 * ROUTE decides which segments get the hit test */
			x0 = gtopx(o->o_x);   y0 = gtopy(o->o_y);
			x1 = gtopx(o->o_x2);  y1 = gtopy(o->o_y2);
			if ( o->o_type == OT_BOX )
			{
				if ( nearseg(px, py, x0, y0, x1, y0) ||
				     nearseg(px, py, x1, y0, x1, y1) ||
				     nearseg(px, py, x1, y1, x0, y1) ||
				     nearseg(px, py, x0, y1, x0, y0) )
					return i;
			}
			else if ( o->o_type == OT_WIRE ||
				  (o->o_type == OT_CONN &&
				   (o->o_sym == CS_HV ||
				    o->o_sym == CS_HARROW)) )
			{
				if ( nearseg(px, py, x0, y0, x1, y0) ||
				     nearseg(px, py, x1, y0, x1, y1) )
					return i;
			}
			else if ( nearseg(px, py, x0, y0, x1, y1) )
				return i;
			break;

		case OT_CIRC:
			dx = (long)(o->o_x2 - o->o_x) * gsc;
			dy = (long)(o->o_y2 - o->o_y) * gsc;
			r = isqrt(dx * dx + dy * dy);
			dx = px - gtopx(o->o_x);
			dy = py - gtopy(o->o_y);
			d = isqrt(dx * dx + dy * dy);
			if ( d >= r - 3 && d <= r + 3 )
				return i;
			break;
		}
	}
	return -1;
}

/* ------------------------------------------------------------------ */
/* pins                                                               */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* attachment points: shapes expose their four edge midpoints and four */
/* corners, symbols their pins -- the shape equivalent of pins, which  */
/* is what connectors snap to and stay attached to.  The old separate  */
/* pin scanner is THIS scanner with symonly set: symbol pins ARE their */
/* attachment points (velbase attpos), so one loop serves both.        */
/* ------------------------------------------------------------------ */

/* Nearest attachment point within reach of canvas pixel (px,py); with
 * symonly only SYMBOL PINS qualify (the Wire tool's snap).  Fills grid
 * position and owner; 1 = found.  The reach is just under one grid
 * unit, so a wire aimed near a pin lands ON it while a wire drawn
 * beside a symbol is left alone. */
static
snapat2(px, py, pgx, pgy, pobj, ppt, symonly)
int *pgx, *pgy, *pobj, *ppt;
{
	register int i, k;
	int n, gx, gy, dx, dy, d, best, got;

	best = (gsc - 1) * (gsc - 1) + 1;
	got = 0;
	for ( i = 0; i < nobj; i++ )
	{
		if ( !layvis[obj[i].o_layer] )
			continue;
		if ( symonly && obj[i].o_type != OT_SYM )
			continue;
		n = natt(i);
		for ( k = 0; k < n; k++ )
		{
			attpos(i, k, &gx, &gy);
			dx = px - gtopx(gx);
			dy = py - gtopy(gy);
			/* range-reject BEFORE squaring: dx*dx of a far
			 * point overflows 16 bits (negative!) and would
			 * beat every honest distance */
			if ( dx < -15 || dx > 15 || dy < -15 || dy > 15 )
				continue;
			d = dx * dx + dy * dy;
			if ( d < best )
			{
				best = d;
				*pgx = gx;  *pgy = gy;
				*pobj = i;  *ppt = k;
				got = 1;
			}
		}
	}
	return got;
}

static
snapatt(px, py, pgx, pgy, pobj, ppt)
int *pgx, *pgy, *pobj, *ppt;
{
	return snapat2(px, py, pgx, pgy, pobj, ppt, 0);
}

static
snappin(px, py, pgx, pgy)
int *pgx, *pgy;
{
	int i, k;

	return snapat2(px, py, pgx, pgy, &i, &k, 1);
}

/* After anything moved / turned / resized: pull every attached connector
 * endpoint onto its target's attachment point, damaging old and new
 * extents -- the "connectors stay attached" rule. */
reroute()
{
	register DOBJ *o;
	register int i;
	int e, gx, gy, ch;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_CONN )
			continue;
		ch = 0;
		for ( e = 0; e < 2; e++ )
		{
			if ( ACOBJ(o, e) < 0 )
				continue;
			if ( ACOBJ(o, e) >= nobj ||
			     attpos((int)ACOBJ(o, e), (int)ACPT(o, e),
				    &gx, &gy) < 0 )
			{
				ACOBJ(o, e) = -1;
				continue;
			}
			if ( e == 0 && (gx != o->o_x || gy != o->o_y) )
			{
				if ( !ch ) { dmgobj(i);  ch = 1; }
				o->o_x = gx;
				o->o_y = gy;
			}
			else if ( e == 1 &&
				  (gx != o->o_x2 || gy != o->o_y2) )
			{
				if ( !ch ) { dmgobj(i);  ch = 1; }
				o->o_x2 = gx;
				o->o_y2 = gy;
			}
		}
		if ( ch )
		{
			dmgobj(i);
			modified = 1;
		}
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* moving, z-order, groups, undo                                      */
/* ------------------------------------------------------------------ */

/* Delete the whole selection (the Del key and velcmd's Cut share it). */
delsel()
{
	register int j;

	snapshot();
	for ( j = nobj - 1; j >= 0; j-- )
		if ( osel[j] )
		{
			dmgobj(j);
			delobj(j);
		}
	selclear();
	reroute();
	statdirty = 1;
	return 1;
}

/* Move object i by (dx,dy) grid units (a polyline moves its pool too). */
moveobj(i, dx, dy)
{
	register DOBJ *o;
	register int k;

	o = &obj[i];
	o->o_x += dx;
	o->o_y += dy;
	switch ( o->o_type )
	{
	case OT_SYM:
	case OT_TEXT:
	case OT_NNAME:
	case OT_ARC:			/* o_x2 is the radius */
		break;
	case OT_POLY:
		for ( k = 0; k < o->o_sym; k++ )
		{
			ppool[o->o_x2 + 2*k] += dx;
			ppool[o->o_x2 + 2*k + 1] += dy;
		}
		break;
	default:
		o->o_x2 += dx;
		o->o_y2 += dy;
		break;
	}
	modified = 1;
	return 0;
}

/* Move object index i to index j, shifting the span between and fixing
 * every reference (connector attachments, the selection) -- the
 * primitive under Front/Back and group compaction. */
objmove(i, j)
{
	DOBJ t;
	char ts;
	register int k;
	register DOBJ *o;
	int e, v;

	if ( i == j )
		return 0;
	t = obj[i];
	ts = osel[i];
	if ( i < j )
		for ( k = i; k < j; k++ )
		{
			obj[k] = obj[k + 1];
			osel[k] = osel[k + 1];
		}
	else
		for ( k = i; k > j; k-- )
		{
			obj[k] = obj[k - 1];
			osel[k] = osel[k - 1];
		}
	obj[j] = t;
	osel[j] = ts;
	for ( k = 0; k < nobj; k++ )
	{
		o = &obj[k];
		if ( o->o_type != OT_CONN )
			continue;
		for ( e = 0; e < 2; e++ )
		{
			v = ACOBJ(o, e);
			if ( v < 0 )
				continue;
			if ( v == i )
				ACOBJ(o, e) = j;
			else if ( i < j && v > i && v <= j )
				ACOBJ(o, e) = v - 1;
			else if ( i > j && v >= j && v < i )
				ACOBJ(o, e) = v + 1;
		}
	}
	if ( selobj == i )
		selobj = j;
	else if ( i < j && selobj > i && selobj <= j )
		selobj--;
	else if ( i > j && selobj >= j && selobj < i )
		selobj++;
	modified = 1;
	return 0;
}

/* Byte copy (no memcpy in this libc's K&R corner) -- ONE
 * loop, shared by every copy site below: PCC emits a fat loop per
 * open-coded copy, and the editor's text bytes are the scarce one. */
static
bmove(d, sp, n)
register char *d, *sp;
register int n;
{
	while ( n-- > 0 )
		*d++ = *sp++;
	return 0;
}

/* Byte-compare two objects (no memcmp in this libc's K&R corner). */
static
objdiff(a, b)
DOBJ *a, *b;
{
	register char *p, *q;
	register int n;

	p = (char *)a;
	q = (char *)b;
	for ( n = sizeof(DOBJ); n > 0; n-- )
		if ( *p++ != *q++ )
			return 1;
	return 0;
}

/* Snapshot the object table and both pools -- called before every
 * mutating commit.  The new block is built BEFORE the old one is
 * released, so a failed allocation leaves the previous snapshot
 * standing rather than half a new one. */
snapshot()
{
	register char *b;

	if ( (b = usnappack()) == (char *)0 )
	{
		uvalid = 0;		/* no undo, rather than a wrong one */
		return 0;
	}
	if ( usnap != (char *)0 )
		free(usnap);
	usnap = b;
	unobj = nobj;
	uppuse = ppuse;
	utpuse = tpuse;
	uvalid = 1;
	return 0;
}

/* One-level undo: damage exactly the objects that DIFFER between the
 * live list and the snapshot (both versions' boxes), then swap the two
 * -- swapping again redoes.  Never a wholesale repaint. */
static
undo()
{
	register int i;
	register char *old;
	char *b;
	int n, x0, y0, x1, y1;
	int on, opp, otp;

	if ( !uvalid || usnap == (char *)0 )
		return 0;
	/* Pack the CURRENT model FIRST: it becomes the snapshot, so
	 * undoing again redoes -- and it is the only step here that can
	 * fail, so failing it leaves the drawing untouched. */
	if ( (b = usnappack()) == (char *)0 )
		return 0;
	old = usnap;			/* the version being restored */
	on = unobj;
	opp = uppuse;
	otp = utpuse;
	dmgsel();			/* the selection rings come off */
	n = nobj > on ? nobj : on;
	for ( i = 0; i < n; i++ )
	{
		if ( i < nobj && i < on &&
		     !objdiff(&obj[i], &USOBJ(old)[i]) )
			continue;
		if ( i < nobj )
		{
			objpbox2(&obj[i], ppool, tpool, &x0, &y0, &x1, &y1);
			dmgpad(x0, y0, x1, y1);
		}
		if ( i < on )
		{
			objpbox2(&USOBJ(old)[i], USPP(old, on),
				 USTP(old, on, opp), &x0, &y0, &x1, &y1);
			dmgpad(x0, y0, x1, y1);
		}
	}
	unobj = nobj;			/* the counts of b, the new snapshot */
	uppuse = ppuse;
	utpuse = tpuse;
	usnaprestore(old, on, opp, otp);
	free(old);
	usnap = b;
	selclear();
	killrun();		/* object indices changed under the run */
	modified = 1;
	rejunc();
	statdirty = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* the XOR rubber figure                                              */
/* ------------------------------------------------------------------ */

/* The figure's KIND is recorded when it is drawn (rbmode), so the erase
 * redraws exactly what is on screen -- which is what lets the same
 * machinery serve both drag rubbers and the buttonless HOVER GHOST
 * (HRF_TRACK motion) that floats under the cursor while placing. */
#define	RB_WIRE	1
#define	RB_LINE	2
#define	RB_RECT	3
#define	RB_CIRC	4
/* (The placement ghost is NOT one of these any more: it is deferred and
 * painted/erased by flush() itself -- see ghpos/ghdrawn above -- because
 * an XOR erase is only valid while the window's clip is unchanged, and a
 * raise or cover between frames left inverted fragments behind.) */
int	rbmode;

static
rubdraw()
{
	switch ( rbmode )
	{
	case RB_WIRE:
		lwire(rbx0, rby0, rbx1, rby1, 2);
		break;
	case RB_LINE:
		cl_line(rbx0, rby0, rbx1, rby1, 2);
		break;
	case RB_RECT:
		xrect(rbx0, rby0, rbx1, rby1);
		break;
	case RB_CIRC:
		{
			long dx, dy;

			dx = rbx1 - rbx0;
			dy = rby1 - rby0;
			cl_circle(rbx0, rby0, (int)isqrt(dx * dx + dy * dy), 2);
		}
		break;
	}
	return 0;
}

/* Remove the rubber figure if one is up. */
static
ruboff()
{
	if ( rubon )
	{
		rubdraw();
		rubon = 0;
	}
	return 0;
}

/* The rubber's pixels became unreliable (an expose revealed part of the
 * area under it): FORGET the figure and damage its extent so the repaint
 * wipes any surviving fragments.  Never XOR-erase after an expose -- the
 * revealed pixels no longer hold what we drew, and re-XORing would print
 * the figure onto fresh content instead of removing it. */
static
rubdmg()
{
	int x0, y0, x1, y1;
	long dx, dy, r;

	if ( !rubon )
		return 0;
	if ( rbmode == RB_CIRC )
	{
		dx = rbx1 - rbx0;
		dy = rby1 - rby0;
		r = isqrt(dx * dx + dy * dy) + 2;
		dmg(rbx0 - (int)r, rby0 - (int)r,
		    rbx0 + (int)r, rby0 + (int)r);
	}
	else
	{
		x0 = rbx0 < rbx1 ? rbx0 : rbx1;
		x1 = (rbx0 > rbx1 ? rbx0 : rbx1) + 1;
		y0 = rby0 < rby1 ? rby0 : rby1;
		y1 = (rby0 > rby1 ? rby0 : rby1) + 1;
		dmg(x0 - 1, y0 - 1, x1 + 1, y1 + 1);
	}
	rubon = 0;
	return 0;
}

/* Replace the rubber figure: erase the old one, draw this one. */
static
rubput(mode, x0, y0, x1, y1)
{
	ruboff();
	rbmode = mode;
	rbx0 = x0;
	rby0 = y0;
	rbx1 = x1;
	rby1 = y1;
	rubdraw();
	rubon = 1;
	return 0;
}

/* Compute the rubber for the current drag at (cgx,cgy) and put it up. */
static
rubset()
{
	int x0, y0, x1, y1, dx, dy, m;

	if ( drag == DR_TOOL )
	{
		switch ( dtool )
		{
		case DT_WIRE:
			m = RB_WIRE;
			break;
		case DT_CONN:
			m = (curconn == CS_HV || curconn == CS_HARROW)
			    ? RB_WIRE : RB_LINE;
			break;
		case DT_BOX:
		case DT_SHAPE:
			m = RB_RECT;
			break;
		case DT_CIRC:
			m = RB_CIRC;
			break;
		default:		/* DT_LINE, DT_ARC's radius leg */
			m = RB_LINE;
			break;
		}
		rubput(m, gtopx(dgx), gtopy(dgy), gtopx(cgx), gtopy(cgy));
	}
	else if ( drag == DR_SREC || drag == DR_RSZ )
		rubput(RB_RECT, gtopx(dgx), gtopy(dgy),
		       gtopx(cgx), gtopy(cgy));
	else if ( drag == DR_END )
		rubput(endrb, gtopx(dgx), gtopy(dgy),
		       gtopx(cgx), gtopy(cgy));
	else if ( drag == DR_PLACE )
	{
		ghgx = cgx;		/* the DEFERRED ghost: flush paints it */
		ghgy = cgy;
		ghpos = 1;
	}
	else	/* DR_MOVE: the whole selection's union bbox */
	{
		if ( !selbbox(&x0, &y0, &x1, &y1) )
			return 0;
		dx = (cgx - dgx) * gsc;
		dy = (cgy - dgy) * gsc;
		rubput(RB_RECT, x0 + dx, y0 + dy, x1 + dx, y1 + dy);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* commands                                                           */
/* ------------------------------------------------------------------ */

/* Empty the model (New, and a fresh sheet of a set). */
static
clearmodel()
{
	selclear();
	nobj = 0;
	njunc = 0;
	ppuse = 0;
	tpuse = 0;
	uvalid = 0;
	return 0;
}

/* Drop the pending run state (anchor, chain, arc sweep). */
static
killrun()
{
	killanchor();
	endchain();
	arcpend = 0;
	return 0;
}

static
donew()
{
	if ( modified && !confirm("Discard unsaved changes?") )
		return 0;
	clearmodel();
	killrun();
	fname[0] = 0;
	modified = 0;
	voxg = voyg = 0;
	viewdirty();
	ddtop = 1;
	return 0;
}

static
doopen()
{
	if ( modified && !confirm("Discard unsaved changes?") )
		return 0;
	killrun();
	filedlg(0);
	return 0;
}

static
dosave()
{
	if ( fname[0] && savefile(fname) == 0 )
		return 0;
	filedlg(1);
	return 0;
}

/* Duplicate the SELECTION offset by (dx,dy); the copies become the
 * selection, ready to be dragged into place.  A copied polyline gets
 * its own pool block; a copied connector re-attaches to the COPY of its
 * target (or detaches if the target was not copied); a copied group
 * gets a fresh group id.  snap = 0 skips the undo snapshot (the ARRAY
 * loop snapshots once for the whole array). */
dodup2(dx, dy, snap)
{
	register DOBJ *o;
	register int i;
	int n0, j, k, e, t;
	short cmap[MAXOBJ];		/* original index -> copy index   */
	char gmap[128];			/* group id -> fresh id (0 = not  */
					/* mapped yet)                    */

	if ( nsel == 0 )
		return 0;
	if ( snap )
		snapshot();
	dmgsel();			/* the old boxes come off */
	n0 = nobj;
	for ( i = 0; i < n0; i++ )
		cmap[i] = -1;
	for ( i = 0; i < 128; i++ )
		gmap[i] = 0;
	for ( i = 0; i < n0 && nobj < MAXOBJ; i++ )
	{
		if ( !osel[i] )
			continue;
		if ( obj[i].o_type == OT_POLY &&
		     ppuse + 2 * obj[i].o_sym > PPOOL )
			continue;	/* no pool room for its points */
		obj[nobj] = obj[i];
		o = &obj[nobj];
		cmap[i] = nobj;
		if ( o->o_type == OT_POLY )
		{
			t = o->o_x2;
			o->o_x2 = ppuse;
			for ( k = 0; k < 2 * o->o_sym; k++ )
				ppool[ppuse + k] = ppool[t + k];
			ppuse += 2 * o->o_sym;
		}
		if ( o->o_grp )
		{
			if ( gmap[o->o_grp & 127] == 0 )
				gmap[o->o_grp & 127] = newgid();
			o->o_grp = gmap[o->o_grp & 127];
		}
		if ( o->o_val[0] == 1 )
		{
			/* a pooled value: the copy gets its OWN block, or
			 * two objects would free one block twice */
			char tb[TVMAX];

			strcpy(tb, oval(o));
			o->o_val[0] = 0;
			setoval(o, tb);
		}
		nobj++;
		moveobj(nobj - 1, dx, dy);
		if ( o->o_type == OT_SYM )
			nextdes(symtab[o->o_sym].sy_pfx, o->o_name);
	}
	if ( nobj == n0 )
		return 0;
	for ( j = n0; j < nobj; j++ )	/* copied conns follow the copies */
	{
		o = &obj[j];
		if ( o->o_type != OT_CONN )
			continue;
		for ( e = 0; e < 2; e++ )
		{
			t = ACOBJ(o, e);
			if ( t < 0 )
				continue;
			ACOBJ(o, e) = (t < n0 && cmap[t] >= 0) ? cmap[t]
							       : -1;
		}
	}
	for ( j = 0; j < n0; j++ )
		osel[j] = 0;
	nsel = 0;
	for ( j = n0; j < nobj; j++ )
	{
		osel[j] = 1;
		nsel++;
	}
	selobj = (nsel == 1) ? n0 : -1;
	modified = 1;
	rejunc();
	reroute();
	dmgsel();			/* the copies and their boxes   */
	statdirty = 1;
	return 1;
}

static
dodup()
{
	return dodup2(2, 2, 1);
}

/* Array duplicate (VELLUM.md sec. 30): the selection repeated on an
 * nx x ny lattice at `pitch' grid units -- pin headers, terminal
 * strips, parking rows.  ONE undo snapshot covers the whole array. */
doarray(nx, ny, pitch)
{
	register int i, j, k;
	char sel0[MAXOBJ];
	int n0;

	if ( nsel == 0 || nx < 1 || ny < 1 || (nx == 1 && ny == 1) )
		return 0;
	snapshot();
	n0 = nobj;
	for ( k = 0; k < n0; k++ )
		sel0[k] = osel[k];
	for ( j = 0; j < ny; j++ )
		for ( i = 0; i < nx; i++ )
		{
			if ( i == 0 && j == 0 )
				continue;
			/* re-select the ORIGINALS: each copy offsets from
			 * the source row, not from the previous copy */
			selclear();
			for ( k = 0; k < n0; k++ )
				if ( sel0[k] )
				{
					osel[k] = 1;
					nsel++;
					selobj = k;
				}
			if ( nsel != 1 )
				selobj = -1;
			if ( !dodup2(i * pitch, j * pitch, 0) )
				return 1;	/* table full: keep what fits */
		}
	return 1;
}

/* One grid point through the selection transform: a quarter turn
 * CLOCKWISE about (cx,cy), or -- mir -- a mirror about its vertical
 * axis.  Integer-exact both ways (sec. 15). */
static
xf1(px, py, cx, cy, mir)
short *px, *py;
{
	register int x, y;

	x = *px;
	y = *py;
	if ( mir )
		*px = 2 * cx - x;
	else
	{
		*px = cx - (y - cy);
		*py = cy + (x - cx);
	}
	return 0;
}

static
a360(v)
{
	v %= 360;
	if ( v < 0 )
		v += 360;
	return v;
}

/* Rotate (mir = 0) or mirror (mir = 1) the WHOLE selection about its
 * grid bbox centre: shapes swap their rect, lines/boxes/polylines/
 * connectors map coordinates, arcs shift their angles with the symbol
 * math, text keeps its anchor. */
static
xformsel(mir)
{
	register DOBJ *o;
	register int i;
	int k, t, cx, cy;
	int ux0, uy0, ux1, uy1;

	if ( nsel == 0 )
		return 0;
	snapshot();
	dmgsel();				/* where it all was */
	if ( !ugbox(1, &ux0, &uy0, &ux1, &uy1) )
		return 0;
	cx = (ux0 + ux1) / 2;
	cy = (uy0 + uy1) / 2;
	for ( i = 0; i < nobj; i++ )
	{
		if ( !osel[i] )
			continue;
		o = &obj[i];
		if ( o->o_type == OT_POLY )
		{
			for ( k = 0; k < o->o_sym; k++ )
				xf1(&ppool[o->o_x2 + 2*k],
				    &ppool[o->o_x2 + 2*k + 1], cx, cy, mir);
			o->o_x = ppool[o->o_x2];
			o->o_y = ppool[o->o_x2 + 1];
			continue;
		}
		xf1(&o->o_x, &o->o_y, cx, cy, mir);
		switch ( o->o_type )
		{
		case OT_SYM:
			if ( mir )
			{	/* Mx . R(r) . M(m) = R(-r) . M(!m) */
				o->o_mir ^= 1;
				o->o_rot = (4 - o->o_rot) & 3;
			}
			else
				o->o_rot = (o->o_rot + 1) & 3;
			break;
		case OT_TEXT:
		case OT_NNAME:
			break;			/* text keeps its anchor */
		case OT_ARC:
			if ( mir )
			{
				t = OA0(o);
				OA0(o) = a360(180 - OA1(o));
				OA1(o) = a360(180 - t);
			}
			else
			{
				OA0(o) = a360(OA0(o) - 90);
				OA1(o) = a360(OA1(o) - 90);
			}
			break;
		default:
			xf1(&o->o_x2, &o->o_y2, cx, cy, mir);
			break;
		}
	}
	modified = 1;
	rejunc();
	reroute();
	dmgsel();				/* where it all is */
	statdirty = 1;
	return 1;
}

/* ---- scale selection (VELLUM.md sec. 39): '*' doubles about the
 * selection's bbox corner (exact), '/' halves -- odd coordinates round
 * and the status line says so: integer honesty over silent drift, the
 * same bargain as quarter-turn-only rotation. ---- */

static
sc1(p, org, up, podd)
short *p;
int *podd;
{
	register int d;

	d = *p - org;
	if ( up )
		*p = org + d * 2;
	else
	{
		if ( d & 1 )
			*podd = 1;
		*p = org + (d >= 0 ? d / 2 : -((1 - d) / 2));
	}
	return 0;
}

static
scalesel(up)
{
	register DOBJ *o;
	register int i;
	int k, odd, ux0, uy0, ux1, uy1;

	if ( nsel == 0 || !ugbox(1, &ux0, &uy0, &ux1, &uy1) )
		return 0;
	snapshot();
	dmgsel();				/* where it all was */
	odd = 0;
	for ( i = 0; i < nobj; i++ )
	{
		if ( !osel[i] )
			continue;
		o = &obj[i];
		sc1(&o->o_x, ux0, up, &odd);
		sc1(&o->o_y, uy0, up, &odd);
		switch ( o->o_type )
		{
		case OT_POLY:
			for ( k = 0; k < o->o_sym; k++ )
			{
				sc1(&ppool[o->o_x2 + 2*k], ux0, up, &odd);
				sc1(&ppool[o->o_x2 + 2*k + 1], uy0, up, &odd);
			}
			o->o_x = ppool[o->o_x2];
			o->o_y = ppool[o->o_x2 + 1];
			break;
		case OT_ARC:
			sc1(&o->o_x2, 0, up, &odd);	/* the radius */
			if ( o->o_x2 < 1 )
				o->o_x2 = 1;
			break;
		case OT_SYM:
		case OT_TEXT:
		case OT_NNAME:
			break;		/* anchors only: bodies are fixed */
		default:
			sc1(&o->o_x2, ux0, up, &odd);
			sc1(&o->o_y2, uy0, up, &odd);
			break;
		}
	}
	if ( !up && odd )
		strcpy(scmsg, "rounded");
	modified = 1;
	rejunc();
	reroute();
	dmgsel();				/* where it all is  */
	statdirty = 1;
	return 1;
}

/* Rotate / mirror: a single selected symbol turns in place, ANY other
 * selection turns about its bbox centre, a pending placement turns its
 * ghost. */
static
dorm(mir)
{
	if ( nsel == 1 && selobj >= 0 && obj[selobj].o_type == OT_SYM )
	{
		snapshot();
		dmgobj(selobj);			/* where it was */
		if ( mir )
			obj[selobj].o_mir ^= 1;
		else
			obj[selobj].o_rot = (obj[selobj].o_rot + 1) & 3;
		dmgobj(selobj);			/* where it is  */
		modified = 1;
		rejunc();	/* the pins moved with the body */
		reroute();	/* ... and any attached connector */
		return 1;
	}
	if ( nsel > 0 )
		return xformsel(mir);
	if ( cursym >= 0 )
	{
		if ( mir )
			placemir ^= 1;
		else
			placerot = (placerot + 1) & 3;
	}
	return 0;
}

static
dorot()
{
	return dorm(0);
}

static
domir()
{
	return dorm(1);
}

/* Switch to a basic tool, dropping every arming, anchor and chain. */
static
settool(t)
{
	tool = t;
	disarm();
	killanchor();
	endchain();
	return 1;
}

/* ---- sheet sets (sec. 19): a sheet is a file, a drawing is a SET of
 * numbered files (amp1.d, amp2.d ...).  Split the current name into
 * <prefix><number><suffix>; 0 = no numeric suffix. ---- */
sheetsplit(pre, pn, suf)
char *pre, *suf;
int *pn;
{
	register int i, d0, dot;

	dot = -1;
	for ( i = 0; fname[i]; i++ )
		if ( fname[i] == '.' )
			dot = i;
	if ( dot < 0 )
		dot = i;		/* no extension: digits at the end */
	d0 = dot;
	while ( d0 > 0 && fname[d0 - 1] >= '0' && fname[d0 - 1] <= '9' )
		d0--;
	if ( d0 == dot )
		return 0;
	for ( i = 0; i < d0; i++ )
		pre[i] = fname[i];
	pre[d0] = 0;
	*pn = atoi(fname + d0);
	strcpy(suf, fname + dot);
	return 1;
}

/* Go to sheet n of the set: SAVE the current sheet, load that one.
 * create = 1 may OFFER to create a missing sheet (how a set grows);
 * Find's Sheets jump passes 0 (sec. 27: sheetgo generalized past +-1). */
sheetto(n, create)
{
	char pre[FNLEN], suf[8], nn[FNLEN + 4], m[24];
	int cn, fd, fresh;

	if ( !sheetsplit(pre, &cn, suf) )
		return 0;
	if ( n < 1 || n == cn )
		return 0;
	sprintf(nn, "%s%d%s", pre, n, suf);
	if ( strlen(nn) >= FNLEN )
		return 0;
	fresh = 1;
	if ( (fd = open(nn, 0)) >= 0 )
	{
		close(fd);
		fresh = 0;
	}
	else if ( !create )
		return 0;
	else
	{
		sprintf(m, "Create sheet %d?", n);
		if ( !confirm(m) )
			return 0;
	}
	if ( modified && savefile(fname) < 0 )
		return 0;		/* never walk away from unsaved work */
	if ( fresh )
	{
		clearmodel();
		strcpy(fname, nn);
		savefile(fname);	/* the new sheet exists at once */
	}
	else
	{
		loadfile(nn);
		strcpy(fname, nn);
	}
	killrun();
	voxg = voyg = 0;
	viewdirty();
	ddtop = 1;
	statdirty = 1;
	return 1;
}

/* Next/Prev sheet ('>' / '<'); past the last sheet the walk may create. */
static
sheetgo(dir)
{
	char pre[FNLEN], suf[8];
	int n;

	if ( !sheetsplit(pre, &n, suf) )
		return 0;
	return sheetto(n + dir, dir > 0);
}

static	evmenu();

/* A key on the canvas.  Returns 1 when a repaint is needed. */
static
dokey(c)
{
	c &= 0xff;
	if ( c >= (HRK_F2 & 0xff) && c <= (HRK_F9 & 0xff) )
	{
		/* zedit's function-key set, routed through the menu
		 * dispatcher: F2 Save, F3 Open, F4 New, F5/F6/F7
		 * Cut/Copy/Paste, F8 Settings, F9 Search (F11 Help
		 * below; F10 never arrives -- zvpump turns it into
		 * ^X^C) */
		static short fnmenu[8] = { HRM_SAVE, HRM_OPEN, HRM_NEW,
			HRM_CUT, HRM_COPY, HRM_PASTE, HRM_SETTINGS,
			HRM_SEARCH };

		evmenu(fnmenu[c - (HRK_F2 & 0xff)]);
		return 1;
	}
	switch ( c )
	{
	case 'r':	return dorot();
	case 'm':	return domir();
	case 'd':	return dodup();
	case 'n':	return propdlg();
	case 'x':
	case 0x7f:			/* keypad Del: the whole selection */
		return nsel > 0 ? delsel() : 0;
	case 'g':
		gridon ^= 1;
		ddcanv = 1;
		return 1;
	case '+':
	case '=':
		return zoomto(gsc * 2);
	case '-':
		return zoomto(gsc / 2);
	case 'a':	return searchagain();	/* next hit, no dialog    */
	case 'u':	return undo();
	case 'v':	return zoomfit();
	case '*':	return scalesel(1);
	case '/':	return scalesel(0);
	case 'f':	return tofront();
	case 'y':	return mksymdlg();	/* v6.7: the selection ->
						 * a library stencil       */
	case 'j':	return dogroup();
	case 'J':	return doungroup();
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
		return doalign(c - '0');
	case 's':
	case 'b':
	case 'c':
	case 't':
	case 'e':
		{
			static char tkey[] = "sbcte";
			static char ttool[] = { T_SEL, T_BOX, T_CIRC,
						T_TEXT, T_DEL };
			register char *q;

			for ( q = tkey; *q != c; q++ )
				;
			return settool(ttool[q - tkey]);
		}
	case 'w':	tool = T_WIRE;  disarm();  endchain();  return 1;
	case 'l':	tool = T_LINE;  disarm();  killanchor();  return 1;
	case '<':	return sheetgo(-1);	/* numbered-sheet sets     */
	case '>':	return sheetgo(1);
	case 0x1b:			/* ESC: cancel the armed tool /
					 * placement / wire or line run, back
					 * to Sel -- KEEPING the selection; a
					 * second ESC (already in Sel) clears
					 * that too */
		if ( tool != T_SEL || armcode() >= 0 || wanchor || arcpend )
		{
			settool(T_SEL);
			if ( rubon )
				rubdmg();	/* a pending arc's rubber */
			return 1;
		}
		dmgsel();
		selclear();
		return 1;
	case HRK_HELP & 0xff:		/* F11: the Help dialog, like the menu */
		dohelp();
		return 1;
	case 0x06:			/* ^F/^B/^N/^P (arrows): pan */
	case 0x02:
	case 0x0e:
	case 0x10:
		{
			register int ox, oy;

			ox = voxg;
			oy = voyg;
			voxg += (c == 0x06) ? 4 : (c == 0x02) ? -4 : 0;
			voyg += (c == 0x0e) ? 4 : (c == 0x10) ? -4 : 0;
			clampvo();
			if ( voxg == ox && voyg == oy )
				return 0;
			viewdirty();
			return 1;
		}
	case HRK_CLRHOME & 0xff:	/* Clear/Home: pan to the origin */
		if ( voxg || voyg )
		{
			voxg = voyg = 0;
			viewdirty();
			return 1;
		}
		return 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* mouse                                                              */
/* ------------------------------------------------------------------ */

/* A left press on the TOOLBAR row: arm a tool, act, or zoom. */
static
tbpress(px)
{
	register int i;

	i = px / TBW;
	if ( i >= NCELL )
		return 0;
	switch ( i )
	{
	case C_GRID:
		gridon ^= 1;
		ddcanv = 1;
		break;
	case C_ROT:
	case C_MIR:
		if ( cursym >= 0 )	/* placing: turn the ghost */
		{
			if ( i == C_ROT )
				placerot = (placerot + 1) & 3;
			else
				placemir ^= 1;
			break;
		}
		/* falls through: arm as a click-to-apply tool */
	case C_NAME:
	case C_DUP:
		if ( i == C_DUP && nsel > 0 )
		{
			arraydlg();	/* selection up: the ARRAY dialog */
			break;
		}
		settool(i - 1);		/* C_ROT.. -> T_ROT.. */
		break;
	case C_ZIN:	zoomto(gsc * 2);	break;
	case C_ZOUT:	zoomto(gsc / 2);	break;
	case C_LIB:
	case C_EDIT:
	case C_FRONT:
	case C_BACK:
		{
			static int (*cfn[4])() = { libdlg, doedit, tofront,
						   toback };

			(*cfn[i - C_LIB])();
		}
		break;
	case C_LAYER:
		curlayer = (curlayer + 1) & (NLAYER - 1);
		break;
	default:
		if ( i < NTOOL )
			settool(i);
		break;
	}
	statdirty = 1;
	return 1;
}

/* A left press in the symbol palette: arm a symbol, or page the bank. */
static
palpress(px, py)
{
	register int i;
	int vis, maxr;

	if ( py >= conth - STH - ABAR && py < conth - STH )
	{
		vis = palvis();			/* the scroll arrows */
		maxr = palrows() - vis;
		if ( maxr < 0 )
			maxr = 0;
		if ( px < SCW )
		{
			if ( palrow > 0 )
				palrow--;
		}
		else if ( palrow < maxr )
			palrow++;
		return 1;
	}
	if ( py >= CANY && py < CANY + LHDR )
	{
		/* the header: next group (real libraries, then "shapes") */
		curlib = (curlib + 1) % (nlib + 1);
		palview();
		disarm();		/* the armed thing may be elsewhere */
		killanchor();
		endchain();
		ddpal = 1;
		statdirty = 1;
		return 1;
	}
	if ( py >= CANY + LHDR && py < CANY + LHDR + palvis() * SCH )
	{
		i = (palrow + (py - CANY - LHDR) / SCH) * 2 +
		    (px >= SCW ? 1 : 0);
		if ( i >= 0 && i < npal )
		{
			disarm();
			if ( palsh )
			{
				if ( i < NSHAPE )
					curshape = i;
				else if ( i < NSHAPE + NCONNS )
					curconn = i - NSHAPE;
				else if ( i == PC_ARC )
					arcarm = 1;
				else if ( i == PC_NET )
					netarm = 1;
				else
					dimarm = 1;
			}
			else
			{
				cursym = palidx[i];
				placerot = placemir = 0;
			}
			dmgsel();
			selclear();
			killanchor();
			endchain();
			statdirty = 1;
		}
		return 1;
	}
	return 0;
}

/* Presses on the scrollbars: on the thumb starts a drag, on the track
 * pages.  ONE path for both bars -- the axis picks the pan variable and
 * the sheet extent (the H/V twins were byte-for-byte parallel). */
static
sbpress(vert, p)
{
	register int *vo;
	int tx, tl, thx, thw, vw, tot;

	vw = sbgeom(vert, &tx, &tl, &thx, &thw);
	if ( p >= thx && p < thx + thw )
	{
		drag = vert ? DR_VSB : DR_HSB;
		sbgrab = p - thx;
		return 0;
	}
	vo = vert ? &voyg : &voxg;
	tot = vert ? SHH : SHW;
	*vo += (p < thx) ? -(vw - 4) : vw - 4;
	if ( *vo > tot - vw ) *vo = tot - vw;
	if ( *vo < 0 ) *vo = 0;
	viewdirty();
	return 1;
}

/* Thumb-drag motion: map the thumb position back to a pan origin.
 * Returns 1 when the view actually moved (repaint). */
static
sbmotion(px, py)
{
	register int *vo;
	int tx, tl, thx, thw, v, was, vert, tot;

	vert = drag == DR_VSB;
	v = sbgeom(vert, &tx, &tl, &thx, &thw);
	vo = vert ? &voyg : &voxg;
	tot = vert ? SHH : SHW;
	was = *vo;
	if ( tl - thw > 0 )
		*vo = (long)((vert ? py : px) - sbgrab - tx) * (tot - v) /
		      (tl - thw);
	if ( *vo > tot - v ) *vo = tot - v;
	if ( *vo < 0 ) *vo = 0;
	if ( *vo != was )
		viewdirty();
	return *vo != was;
}

/* Commit the pending arc's second stage: the press picks the END angle. */
static
arccommit(gx, gy)
{
	register int i;
	int a1;

	arcpend = 0;
	rubdmg();			/* the sweep rubber comes off */
	a1 = iangle(gx - arccx, -(gy - arccy));
	if ( a1 == arca0 )
		return 1;		/* zero sweep: cancelled */
	snapshot();
	i = addobj(OT_ARC, arccx, arccy, 0, 0);
	if ( i >= 0 )
	{
		obj[i].o_x2 = arcr;
		OA0(&obj[i]) = arca0;
		OA1(&obj[i]) = a1;
		dmgobj(i);
	}
	statdirty = 1;
	return 1;
}

/* A left press on the canvas. */
static
canvpress(px, py)
{
	int gx, gy, i, k;

	gx = pxtogx(px);
	gy = pxtogy(py);
	if ( cursym < 0 && curconn < 0 && curshape < 0 && !arcarm &&
	     !netarm && tool == T_WIRE )
		snappin(px, py, &gx, &gy);	/* wires snap onto pins */
	lastgx = gx;
	lastgy = gy;
	if ( netarm )
	{
		vbuf[0] = 0;
		if ( textdlg("") )
		{
			snapshot();
			i = addobj(OT_NNAME, gx, gy, 0, 0);
			if ( i >= 0 )
			{
				strncpy(obj[i].o_name, vbuf, NAMEL - 1);
				obj[i].o_name[NAMEL - 1] = 0;
				dmgobj(i);
			}
		}
		return 1;
	}
	if ( arcarm )
	{
		if ( arcpend )
			return arccommit(gx, gy);
		drag = DR_TOOL;		/* stage 1: centre -> radius/start */
		dtool = DT_ARC;
		dgx = cgx = gx;
		dgy = cgy = gy;
		rubset();
		return 0;
	}
	if ( curshape >= 0 )
	{
		drag = DR_TOOL;		/* drag a rect like the Box tool */
		dtool = DT_SHAPE;
		dgx = cgx = gx;
		dgy = cgy = gy;
		rubset();
		return 0;
	}
	if ( dimarm || curconn >= 0 )
	{
		/* the Connector / Dimension tools: the Wire gestures, both
		 * ends snapping to ATTACHMENT points (shape mids/corners,
		 * symbol pins) -- so dimensioning or connecting a shape
		 * edge is two clicks */
		kb_obj = -1;
		if ( snapatt(px, py, &gx, &gy, &i, &k) )
		{
			if ( curconn >= 0 && !wanchor )
			{
				ka_obj = i;
				ka_pt = k;
			}
		}
		else if ( curconn >= 0 && !wanchor )
			ka_obj = -1;
		drag = DR_TOOL;
		dtool = dimarm ? DT_DIM : DT_CONN;
		if ( wanchor )
		{
			dgx = wax;
			dgy = way;
		}
		else
		{
			dgx = gx;
			dgy = gy;
		}
		cgx = gx;
		cgy = gy;
		rubset();
		return 0;
	}
	if ( cursym >= 0 )
	{
		drag = DR_PLACE;
		dgx = cgx = gx;
		dgy = cgy = gy;
		rubset();
		return 0;
	}
	switch ( tool )
	{
	case T_SEL:
		/* a single selected SHAPE: its corner handles resize */
		if ( nsel == 1 && selobj >= 0 &&
		     obj[selobj].o_type == OT_SHAPE )
		{
			int hx0, hy0, hx1, hy1, hx, hy;

			objpbox(selobj, &hx0, &hy0, &hx1, &hy1);
			for ( k = 0; k < 4; k++ )
			{
				hx = (k & 1) ? hx1 : hx0;
				hy = (k & 2) ? hy1 : hy0;
				if ( px >= hx - 3 && px <= hx + 3 &&
				     py >= hy - 3 && py <= hy + 3 )
				{
					register DOBJ *o;
					int rx0, ry0, rx1, ry1, t;

					o = &obj[selobj];
					rx0 = o->o_x;  rx1 = o->o_x2;
					if ( rx1 < rx0 )
					{ t = rx0; rx0 = rx1; rx1 = t; }
					ry0 = o->o_y;  ry1 = o->o_y2;
					if ( ry1 < ry0 )
					{ t = ry0; ry0 = ry1; ry1 = t; }
					drag = DR_RSZ;
					rszc = k;
					/* anchor = the opposite corner */
					dgx = (k & 1) ? rx0 : rx1;
					dgy = (k & 2) ? ry0 : ry1;
					cgx = gx;
					cgy = gy;
					rubset();
					return 0;
				}
			}
		}
		/* a single selected line/wire/connector/dimension: its two
		 * endpoint grips drag; a polyline's vertices drag -- moving
		 * a connector end to a new attachment is a drag, not
		 * delete-and-redraw (sec. 15) */
		if ( nsel == 1 && selobj >= 0 )
		{
			register DOBJ *o;
			int hx, hy, ne;

			o = &obj[selobj];
			ne = 0;
			switch ( o->o_type )
			{
			case OT_LINE:
			case OT_WIRE:
			case OT_CONN:
			case OT_DIM:
				ne = 2;
				break;
			case OT_POLY:
				ne = o->o_sym;
				break;
			}
			for ( k = 0; k < ne; k++ )
			{
				if ( o->o_type == OT_POLY )
				{
					hx = gtopx(ppool[o->o_x2 + 2*k]);
					hy = gtopy(ppool[o->o_x2 + 2*k + 1]);
				}
				else
				{
					hx = gtopx(k ? o->o_x2 : o->o_x);
					hy = gtopy(k ? o->o_y2 : o->o_y);
				}
				if ( px < hx - 3 || px > hx + 3 ||
				     py < hy - 3 || py > hy + 3 )
					continue;
				drag = DR_END;
				ende = k;
				endrb = RB_LINE;
				/* the rubber runs from the anchor: the far
				 * endpoint (or the previous vertex) */
				if ( o->o_type == OT_POLY )
				{
					int a;

					a = k > 0 ? k - 1 : 1;
					dgx = ppool[o->o_x2 + 2*a];
					dgy = ppool[o->o_x2 + 2*a + 1];
				}
				else
				{
					dgx = k ? o->o_x : o->o_x2;
					dgy = k ? o->o_y : o->o_y2;
					if ( o->o_type == OT_WIRE ||
					     (o->o_type == OT_CONN &&
					      (o->o_sym == CS_HV ||
					       o->o_sym == CS_HARROW)) )
						endrb = RB_WIRE;
				}
				cgx = gx;
				cgy = gy;
				kb_obj = -1;
				rubset();
				return 0;
			}
		}
		i = pick(px, py);
		if ( i >= 0 )
		{
			/* on a SELECTED object: move the whole selection;
			 * on an unselected one: it becomes the selection */
			if ( !osel[i] )
			{
				dmgsel();	/* old boxes off */
				selone(i);
				dmgsel();	/* the new boxes on */
			}
			drag = DR_MOVE;
			dgx = cgx = gx;
			dgy = cgy = gy;
			return 1;
		}
		/* empty canvas: drag a rectangle to select what it covers */
		drag = DR_SREC;
		dgx = cgx = gx;
		dgy = cgy = gy;
		rubset();
		return 0;

	case T_WIRE:
	case T_LINE:
		/* An anchored start (a previous click) owns the press: the
		 * rubber runs from the ANCHOR, so the second half of a
		 * click-click gesture gets live feedback while the button
		 * is down, and the release finishes anchor -> here.  (The
		 * Line tool CHAINS the same way the wire run does; the
		 * chain accumulates into ONE polyline.) */
		drag = DR_TOOL;
		dtool = (tool == T_WIRE) ? DT_WIRE : DT_LINE;
		if ( wanchor )
		{
			dgx = wax;
			dgy = way;
		}
		else
		{
			dgx = gx;
			dgy = gy;
		}
		cgx = gx;
		cgy = gy;
		rubset();
		return 0;

	case T_BOX:
	case T_CIRC:
		drag = DR_TOOL;
		dtool = (tool == T_BOX) ? DT_BOX : DT_CIRC;
		dgx = cgx = gx;
		dgy = cgy = gy;
		rubset();
		return 0;

	case T_TEXT:
		vbuf[0] = 0;
		if ( textdlg("") )
		{
			snapshot();
			i = addobj(OT_TEXT, gx, gy, 0, 0);
			if ( i >= 0 )
			{
				setoval(&obj[i], vbuf);
				dmgobj(i);
			}
		}
		return 1;

	case T_DEL:
		i = pick(px, py);
		if ( i >= 0 )
		{
			snapshot();
			dmgobj(i);
			delobj(i);
			reroute();
			return 1;
		}
		return 0;

	/* The click-to-apply actions: hit an object, select it, act on it --
	 * and stay armed, so the next click acts on the next object. */
	case T_ROT:
	case T_MIR:
		i = pick(px, py);
		if ( i < 0 )
			return 0;
		dmgsel();
		selone(i);
		if ( tool == T_ROT )
			dorot();
		else
			domir();
		return 1;

	case T_NAME:
		i = pick(px, py);
		if ( i < 0 )
			return 0;
		dmgsel();
		selone(i);
		dmgobj(i);
		propdlg();
		return 1;

	case T_DUP:
		i = pick(px, py);
		if ( i < 0 )
			return 0;
		dmgsel();
		selone(i);
		dodup();
		return 1;
	}
	return 0;
}

/* Motion while dragging: move the rubber figure. */
static
canvmotion(px, py)
{
	int gx, gy, i, k;

	gx = pxtogx(px);
	gy = pxtogy(py);
	if ( drag == DR_TOOL && dtool == DT_WIRE )
		snappin(px, py, &gx, &gy);	/* the far end snaps too */
	else if ( drag == DR_TOOL && dtool == DT_DIM )
		snapatt(px, py, &gx, &gy, &i, &k);
	else if ( (drag == DR_TOOL && dtool == DT_CONN) ||
		  (drag == DR_END && selobj >= 0 &&
		   obj[selobj].o_type == OT_CONN) )
	{
		/* the far end snaps to attachment points; remember whose */
		if ( snapatt(px, py, &gx, &gy, &i, &k) )
		{
			kb_obj = i;
			kb_pt = k;
		}
		else
			kb_obj = -1;
	}
	else if ( drag == DR_END && selobj >= 0 )
	{
		if ( obj[selobj].o_type == OT_WIRE )
			snappin(px, py, &gx, &gy);
		else if ( obj[selobj].o_type == OT_DIM )
			snapatt(px, py, &gx, &gy, &i, &k);
	}
	lastgx = gx;
	lastgy = gy;
	statdirty = 1;
	if ( drag == DR_NONE || (gx == cgx && gy == cgy) )
		return 0;
	ruboff();
	cgx = gx;
	cgy = gy;
	rubset();
	return 0;
}

/* Buttonless motion (HRF_TRACK): float the placement ghost -- the armed
 * symbol's bounding box, XOR -- under the cursor, or the pending wire's
 * rubber from its anchor; and keep the coordinate readout live. */
static
hovermotion(px, py)
{
	int gx, gy;

	if ( px < PALW || px >= cright() || py < CANY || py >= canvh() )
	{
		ruboff();		/* left the canvas: drop the rubber */
		ghpos = 0;		/* ... and the ghost (flush erases)  */
		return 0;
	}
	gx = pxtogx(px);
	gy = pxtogy(py);
	if ( cursym < 0 && curconn < 0 && !dimarm && tool == T_WIRE &&
	     wanchor )
		snappin(px, py, &gx, &gy);
	else if ( (dimarm || curconn >= 0) && wanchor )
	{
		int i, k;

		if ( snapatt(px, py, &gx, &gy, &i, &k) )
		{
			if ( curconn >= 0 )
			{
				kb_obj = i;
				kb_pt = k;
			}
		}
		else if ( curconn >= 0 )
			kb_obj = -1;
	}
	if ( gx == lastgx && gy == lastgy )
		return 0;		/* still the same grid point */
	lastgx = gx;
	lastgy = gy;
	statdirty = 1;
	if ( cursym >= 0 )
	{
		ghgx = gx;		/* the DEFERRED ghost: flush paints it */
		ghgy = gy;
		ghpos = 1;
	}
	else if ( arcpend )		/* sweeping the pending arc's end */
		rubput(RB_LINE, gtopx(arccx), gtopy(arccy),
		       gtopx(gx), gtopy(gy));
	else if ( (curconn >= 0 || dimarm) && wanchor )
		rubput((curconn == CS_HV || curconn == CS_HARROW)
		       ? RB_WIRE : RB_LINE,
		       gtopx(wax), gtopy(way), gtopx(gx), gtopy(gy));
	else if ( curconn < 0 && !dimarm &&
		  (tool == T_WIRE || tool == T_LINE) && wanchor )
		rubput(tool == T_WIRE ? RB_WIRE : RB_LINE,
		       gtopx(wax), gtopy(way), gtopx(gx), gtopy(gy));
	return 0;
}

/* The zero-length gesture of the wire/line/connector tools: the first
 * click anchors the start, a click back on the anchor cancels it. */
static
reltoggle()
{
	if ( wanchor )
	{
		killanchor();
		endchain();
	}
	else
	{
		wanchor = 1;
		wax = dgx;
		way = dgy;
		dmganchor();
	}
	statdirty = 1;
	return 1;
}

/* Commit a wire segment (the run re-anchors, Eagle-fashion). */
static
relwire()
{
	snapshot();
	addobj(OT_WIRE, dgx, dgy, cgx, cgy);
	dmgobj(nobj - 1);	/* covers both anchor crosses */
	/* the RUN goes on: re-anchor at this wire's end -- unless it
	 * landed on a pin, a finished connection */
	if ( pinat(cgx, cgy) )
		wanchor = 0;
	else
	{
		wanchor = 1;
		wax = cgx;
		way = cgy;
	}
	statdirty = 1;
	return 1;
}

/* Commit a line segment: the Line tool CHAINS like the wire run, and a
 * continued chain folds into ONE polyline (P object). */
static
relline()
{
	register DOBJ *o;
	int ext;

	snapshot();
	ext = 0;
	if ( lchain >= 0 && lchain < nobj )
	{
		o = &obj[lchain];
		if ( o->o_type == OT_LINE &&
		     o->o_x2 == dgx && o->o_y2 == dgy &&
		     ppuse + 6 <= PPOOL )
			ext = 1;
		else if ( o->o_type == OT_POLY &&
			  o->o_x2 + 2 * o->o_sym == ppuse &&
			  o->o_sym < PMAXPT && ppuse + 2 <= PPOOL &&
			  ppool[ppuse - 2] == dgx &&
			  ppool[ppuse - 1] == dgy )
			ext = 2;
	}
	if ( ext == 1 )			/* line -> 3-point polyline */
	{
		o = &obj[lchain];
		ppool[ppuse] = o->o_x;
		ppool[ppuse + 1] = o->o_y;
		ppool[ppuse + 2] = o->o_x2;
		ppool[ppuse + 3] = o->o_y2;
		ppool[ppuse + 4] = cgx;
		ppool[ppuse + 5] = cgy;
		o->o_type = OT_POLY;
		o->o_sym = 3;
		o->o_x2 = ppuse;
		ppuse += 6;
		modified = 1;
	}
	else if ( ext == 2 )		/* append to the polyline */
	{
		o = &obj[lchain];
		ppool[ppuse++] = cgx;
		ppool[ppuse++] = cgy;
		o->o_sym++;
		modified = 1;
	}
	else
	{
		lchain = addobj(OT_LINE, dgx, dgy, cgx, cgy);
		if ( lchain >= 0 )
			dmgobj(lchain);
	}
	if ( ext )			/* damage just the new segment */
	{
		int x0, y0, x1, y1, t;

		x0 = gtopx(dgx);  x1 = gtopx(cgx);
		if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
		y0 = gtopy(dgy);  y1 = gtopy(cgy);
		if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
		dmg(x0 - 6, y0 - 6, x1 + 6, y1 + 6);
	}
	wanchor = 1;			/* the run continues from here */
	wax = cgx;
	way = cgy;
	statdirty = 1;
	return 1;
}

/* Commit a connector: endpoints keep the attachments they snapped to. */
static
relconn()
{
	register DOBJ *o;
	int i;

	snapshot();
	i = addobj(OT_CONN, dgx, dgy, cgx, cgy);
	if ( i >= 0 )
	{
		o = &obj[i];
		o->o_sym = curconn;
		if ( ka_obj >= 0 && ka_obj < i )
		{
			ACOBJ(o, 0) = ka_obj;
			ACPT(o, 0) = ka_pt;
		}
		if ( kb_obj >= 0 && kb_obj < i )
		{
			ACOBJ(o, 1) = kb_obj;
			ACPT(o, 1) = kb_pt;
		}
		dmgobj(i);
	}
	killanchor();
	ka_obj = -1;
	kb_obj = -1;
	statdirty = 1;
	return 1;
}

/* Commit a dimension: empty text = the AUTO label (distance x unit);
 * lands on the annotation layer, whatever layer is active. */
static
reldim()
{
	register int i;

	snapshot();
	i = addobj(OT_DIM, dgx, dgy, cgx, cgy);
	if ( i >= 0 )
	{
		obj[i].o_layer = 1;
		dmgobj(i);
	}
	killanchor();
	statdirty = 1;
	return 1;
}

/* Commit a shape: create it, then ask for the label. */
static
relshape()
{
	register int i;

	snapshot();
	i = addobj(OT_SHAPE, dgx, dgy, cgx, cgy);
	if ( i >= 0 )
	{
		obj[i].o_sym = curshape;
		vbuf[0] = 0;
		if ( textdlg("") )
			setoval(&obj[i], vbuf);
		dmgobj(i);
	}
	statdirty = 1;
	return 1;
}

/* Release: commit what the drag was doing.  Returns 1 to repaint. */
static
canvrelease()
{
	int dx, dy, i;

	if ( drag == DR_NONE )
		return 0;
	ruboff();
	dx = cgx - dgx;
	dy = cgy - dgy;
	i = drag;
	drag = DR_NONE;
	switch ( i )
	{
	case DR_PLACE:
		snapshot();
		i = placesym(cgx, cgy);
		if ( i >= 0 )
			dmgobj(i);
		statdirty = 1;
		return 1;

	case DR_TOOL:
		switch ( dtool )
		{
		case DT_WIRE:
		case DT_LINE:
		case DT_CONN:
		case DT_DIM:
			/* the run gestures share the zero-move anchor toggle */
			if ( dx == 0 && dy == 0 )
				return reltoggle();
			return dtool == DT_WIRE ? relwire() :
			       dtool == DT_LINE ? relline() :
			       dtool == DT_CONN ? relconn() : reldim();
		case DT_SHAPE:
			if ( dx == 0 && dy == 0 )
				return 1;
			return relshape();
		case DT_ARC:
			/* stage 1 done: centre + radius + start angle;
			 * the sweep now follows the pointer, the next
			 * click commits the end angle */
			if ( dx == 0 && dy == 0 )
				return 1;
			arccx = dgx;
			arccy = dgy;
			arcr = (int)isqrt((long)dx * dx + (long)dy * dy);
			if ( arcr < 1 )
				return 1;
			arca0 = iangle(dx, -dy);
			arcpend = 1;
			statdirty = 1;
			return 1;
		}
		if ( dx == 0 && dy == 0 )
			return 1;	/* zero size: nothing to add */
		snapshot();
		addobj(dtool == DT_BOX ? OT_BOX : OT_CIRC,
		       dgx, dgy, cgx, cgy);
		dmgobj(nobj - 1);
		return 1;

	case DR_END:
		if ( selobj >= 0 )
		{
			register DOBJ *o;
			register short *xp;

			o = &obj[selobj];
			if ( o->o_type == OT_POLY )
				xp = &ppool[o->o_x2 + 2*ende];
			else if ( ende )
				xp = &o->o_x2;
			else
				xp = &o->o_x;
			/* a CLICK on a grip (no movement) is a no-op -- it
			 * must not silently detach a connector's end */
			if ( cgx == xp[0] && cgy == xp[1] )
				return 1;
			snapshot();
			dmgobj(selobj);		/* where it was */
			xp[0] = cgx;
			xp[1] = cgy;
			if ( o->o_type == OT_POLY )
			{
				o->o_x = ppool[o->o_x2];
				o->o_y = ppool[o->o_x2 + 1];
			}
			else if ( o->o_type == OT_CONN )
			{
				/* the dragged end RE-SNAPS: re-attach to
				 * what it landed on, or detach */
				if ( kb_obj >= 0 && kb_obj != selobj )
				{
					ACOBJ(o, ende) = kb_obj;
					ACPT(o, ende) = kb_pt;
				}
				else
					ACOBJ(o, ende) = -1;
			}
			dmgobj(selobj);		/* where it is  */
			modified = 1;
			rejunc();
			reroute();
		}
		return 1;

	case DR_RSZ:
		if ( selobj >= 0 && obj[selobj].o_type == OT_SHAPE &&
		     (cgx != dgx && cgy != dgy) )
		{
			snapshot();
			dmgobj(selobj);		/* where it was */
			obj[selobj].o_x = dgx;	/* anchor corner */
			obj[selobj].o_y = dgy;
			obj[selobj].o_x2 = cgx;	/* dragged corner */
			obj[selobj].o_y2 = cgy;
			dmgobj(selobj);		/* where it is  */
			modified = 1;
			reroute();
		}
		return 1;

	case DR_MOVE:
		if ( (dx == 0 && dy == 0) || nsel == 0 )
			return 1;
		snapshot();
		dmgsel();			/* where it all was */
		for ( i = 0; i < nobj; i++ )
			if ( osel[i] )
				moveobj(i, dx, dy);
		dmgsel();			/* where it all is  */
		modified = 1;
		rejunc();
		reroute();
		return 1;

	case DR_SREC:
		if ( dx == 0 && dy == 0 )
		{
			dmgsel();	/* a click on empty canvas: boxes off */
			selclear();
			statdirty = 1;
			return 1;
		}
		/* select every object whose bbox lies inside the rect */
		{
			int rx0, ry0, rx1, ry1, x0, y0, x1, y1, t;

			rx0 = gtopx(dgx);  rx1 = gtopx(cgx);
			ry0 = gtopy(dgy);  ry1 = gtopy(cgy);
			if ( rx1 < rx0 ) { t = rx0; rx0 = rx1; rx1 = t; }
			if ( ry1 < ry0 ) { t = ry0; ry0 = ry1; ry1 = t; }
			dmgsel();		/* the old boxes come off */
			selclear();
			for ( i = 0; i < nobj; i++ )
			{
				objpbox(i, &x0, &y0, &x1, &y1);
				if ( x0 >= rx0 && x1 <= rx1 &&
				     y0 >= ry0 && y1 <= ry1 )
				{
					osel[i] = 1;
					nsel++;
					selobj = i;
				}
			}
			/* a partially-caught GROUP joins whole */
			for ( i = 0; i < nobj; i++ )
			{
				register int j;

				if ( !osel[i] || obj[i].o_grp == 0 )
					continue;
				for ( j = 0; j < nobj; j++ )
					if ( !osel[j] &&
					     obj[j].o_grp == obj[i].o_grp )
					{
						osel[j] = 1;
						nsel++;
					}
			}
			if ( nsel != 1 )
				selobj = -1;
			dmgsel();		/* the new boxes go on    */
			statdirty = 1;
		}
		return 1;

	case DR_HSB:
	case DR_VSB:
		return 0;	/* the view scrolled live during the drag */
	}
	return 1;
}

/* ------------------------------------------------------------------ */
/* the middle button: a DRAG pans the view, a CLICK pastes the         */
/* clipboard at the pointer (paste is a click, not a drag).            */
/* ------------------------------------------------------------------ */

static
midpress(px, py)
{
	if ( px < PALW || px >= cright() || py < CANY || py >= canvh() )
		return 0;
	drag = DR_PANP;
	midpx = px;
	midpy = py;
	panvx = voxg;
	panvy = voyg;
	return 0;
}

static
midmotion(px, py)
{
	int nx, ny, m;

	if ( drag == DR_PANP )
	{
		m = px - midpx;
		if ( m < 0 ) m = -m;
		nx = py - midpy;
		if ( nx < 0 ) nx = -nx;
		if ( m <= 3 && nx <= 3 )
			return 0;
		drag = DR_PAN;
	}
	nx = panvx - (px - midpx) / gsc;
	ny = panvy - (py - midpy) / gsc;
	if ( nx > SHW - 8 ) nx = SHW - 8;
	if ( ny > SHH - 8 ) ny = SHH - 8;
	if ( nx < 0 ) nx = 0;
	if ( ny < 0 ) ny = 0;
	if ( nx != voxg || ny != voyg )
	{
		voxg = nx;
		voyg = ny;
		viewdirty();
	}
	return 0;
}

static
midrelease()
{
	int m;

	m = drag;
	drag = DR_NONE;
	if ( m == DR_PANP )		/* a click: Paste at the pointer */
	{
		dopaste(pxtogx(midpx), pxtogy(midpy), 0);
		statdirty = 1;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* autosave: a 5-minute one-shot alarm; the handler only flags, the    */
/* main loop writes <file>.bak (the V7 one-shot is re-installed FIRST  */
/* in the handler -- the zdock re-arm-window trap).                    */
/* ------------------------------------------------------------------ */

int	wantbak;

static
onalrm()
{
	signal(SIGALRM, onalrm);
	wantbak = 1;
	return 0;
}

static
dobak()
{
	char bk[FNLEN + 6];

	if ( modified && fname[0] )
	{
		sprintf(bk, "%s.bak", fname);
		writefile(bk);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
/* v6.7 (VELLUM.md sec. 60): centre the view on a grid point and SELECT
 * the object there.  Every asking mode already prints "file: ... at
 * x,y"; `vellum +x,y file.d' hands that straight back to the board, so
 * `velinfo -where TODO *.d' stops being a report and becomes a work
 * list.  Topmost hit wins, and a visible layer is a hittable one -- the
 * Find dialog's rule (veldlg.c), because they are the same act. */
static
gotopt(gx, gy)
{
	register int i;
	int x0, y0, x1, y1;

	voxg = gx - (contw - SBW - PALW) / (2 * gsc);
	voyg = gy - (conth - STH - SBW - CANY) / (2 * gsc);
	clampvo();
	for ( i = nobj - 1; i >= 0; i-- )
	{
		if ( !layvis[obj[i].o_layer] )
			continue;
		objgbox(i, &x0, &y0, &x1, &y1);
		if ( gx >= x0 && gx <= x1 && gy >= y0 && gy <= y1 )
		{
			selclear();
			osel[i] = 1;
			nsel = 1;
			selobj = i;
			break;
		}
	}
	statdirty = 1;
	return 0;
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int i, gotoc, gotox, gotoy;

	/* The editor DRAWS.  Everything else the suite does is a tool of
	 * its own -- headless, so it runs where there is no bitmap card
	 * and no window server, which is what a Makefile wants.  A
	 * lowercase option here is someone reaching for the old
	 * do-everything front door: name the tools instead of quietly
	 * doing a different program's job.  (The GUI options hr_open eats
	 * -- -T -I -S -P -H -- are all uppercase and pass through.) */
	if ( argc >= 2 && argv[1][0] == '-' &&
	     argv[1][1] >= 'a' && argv[1][1] <= 'z' )
	{
		fprintf(stderr, "vellum: the editor takes a file, not %s\n",
			argv[1]);
		fprintf(stderr,
	"vellum: try velplot velpic veldxf velnet velcheck veldiff velinfo velsym\n");
		exit(1);
	}

	loadsyms();			/* every library, LIBLIST + user */
	palview();			/* the palette shows curlib      */
	me.ha_w = PALW + 81 * GRID + SBW;   /* 81 x 50 grid units visible */
	me.ha_h = CANY + 50 * GRID + SBW + STH;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	signal(SIGALRM, onalrm);	/* AFTER hr_open: it used SIGALRM */
	alarm(300);			/* the autosave tick */

	/* `+x,y': the grid point to open ON (v6.7, sec. 60).  It is not
	 * an option hr_open eats and not a file, so it is picked out of
	 * argv here and the rest of the line closes over it. */
	gotoc = 0;
	gotox = gotoy = 0;
	for ( i = 1; i < argc; i++ )
		if ( argv[i][0] == '+' && argv[i][1] )
		{
			register char *q;

			q = argv[i] + 1;
			gotox = atoi(q);
			while ( *q && *q != ',' )
				q++;
			gotoy = *q ? atoi(q + 1) : 0;
			gotoc = 1;
			while ( i + 1 < argc )
			{
				argv[i] = argv[i + 1];
				i++;
			}
			argc--;
			break;
		}

	/* An optional file argument (options were consumed by hr_open):
	 * open it, or start empty under that name if it does not exist. */
	if ( argc > 1 && argv[1][0] )
	{
		for ( i = 0; argv[1][i] && i < FNLEN - 1; i++ )
			fname[i] = argv[1][i];
		fname[i] = 0;
		loadfile(fname);
	}
	if ( gotoc )
		gotopt(gotox, gotoy);

	alldirty();			/* drawn below, or by the first loop
					 * pass if a server overlay is up now */
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		cl_begin();
		flush();
		cl_end();
	}
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
			dispatch(&e);
		if ( wantbak )			/* the 5-minute autosave */
		{
			wantbak = 0;
			dobak();
			alarm(300);
		}
		if ( hr_evover(mywid) )		/* fell behind: repaint all */
			alldirty();
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )	/* a draw was lost to a freeze */
				alldirty();
			cl_begin();
			flush();
			cl_end();
		}
	}
}

/* ---- the event handlers, one function each: the big switch bodies
 * live OUT of main so no branch label collects both near and far
 * references (the assembler's fix-up tables mis-size those). ---- */

/* Repaint what was EXPOSED, not the world: an uncover arrives as one
 * partial expose per revealed rect (often in separate batches).
 * Damage the rect; flag only the chrome pieces it actually touches. */
static
evexpose(ep)
register WMSG *ep;
{
	int ex0, ey0, ex1, ey1;

	ex0 = ep->wm_arg[0];
	ey0 = ep->wm_arg[1];
	ex1 = ex0 + ep->wm_arg[2];
	ey1 = ey0 + ep->wm_arg[3];
	if ( ex0 <= 0 && ey0 <= 0 && ex1 >= contw && ey1 >= conth )
	{
		drag = DR_NONE;
		rubon = 0;
		alldirty();
		return 0;
	}
	rubdmg();
	dmg(ex0, ey0, ex1, ey1);
	if ( ey0 < CANY && ey1 > CTOP )
		ddtbar = 1;
	if ( ey0 < CTOP )
		ddtop = 1;
	if ( ex0 < PALW && ey1 > CANY )
		ddpal = 1;
	if ( ex1 > cright() || ey1 > canvh() )
		ddsb = 1;
	if ( ey1 > conth - STH )
	{
		statdirty = 1;
		s_valid = 0;		/* surface hit */
	}
	return 0;
}

static
evbutton(ep)
register WMSG *ep;
{
	int bx, by;

	bx = ep->wm_arg[0];
	by = ep->wm_arg[1];
	if ( ep->wm_arg[2] & EB_LEFT )		/* press */
	{
		if ( by < CTOP )
		{
			/* the file bar: only its sheet-set cells press */
			if ( bx >= contw - 2 * SHCW )
				sheetgo(bx >= contw - SHCW ? 1 : -1);
		}
		else if ( by < CANY )
			tbpress(bx);
		else if ( bx < PALW && by < conth - STH )
			palpress(bx, by);
		else if ( bx >= cright() && by < canvh() )
			sbpress(1, by);
		else if ( by >= canvh() && by < conth - STH &&
			  bx < cright() )
			sbpress(0, bx);
		else if ( by < canvh() && bx < cright() )
		{
			canvpress(bx, by);
			statdirty = 1;
		}
	}
	else if ( ep->wm_arg[2] & EB_MID )
		midpress(bx, by);
	else					/* release */
	{
		if ( drag == DR_PAN || drag == DR_PANP )
			midrelease();
		else
			canvrelease();
	}
	return 0;
}

static
evmenu(code)
{
	/* the HRM_* bits in LSB order; Paste is special-cased (it takes
	 * the pointer position).  HRM_FIND (bit 9) is not ours: an editor
	 * declares Search, which is the one it can act on. */
	static int (*mfn[11])() = { donew, doopen, dosave, docut, docopy,
		(int (*)())0, settingsdlg, dohelp, printdlg, (int (*)())0,
		searchdlg };
	register int i;

	if ( code == HRM_PASTE )
		dopaste(lastgx, lastgy, 1);
	else
		for ( i = 0; i < 11; i++ )
			if ( code == (1 << i) && mfn[i] )
			{
				(*mfn[i])();
				break;
			}
	statdirty = 1;
	return 0;
}

static
dispatch(ep)
register WMSG *ep;
{
	switch ( ep->wm_type )
	{
	case E_EXPOSE:
		evexpose(ep);
		break;

	case E_RESIZE:
		contw = ep->wm_arg[0];
		conth = ep->wm_arg[1];
		drag = DR_NONE;		/* any rubber pixels are gone */
		rubon = 0;
		alldirty();
		break;

	case E_KEY:
		if ( drag == DR_NONE )
			dokey(ep->wm_arg[0]);
		statdirty = 1;
		break;

	case E_BUTTON:
		evbutton(ep);
		break;

	case E_MOTION:
		if ( drag == DR_HSB || drag == DR_VSB )
			sbmotion(ep->wm_arg[0], ep->wm_arg[1]);
		else if ( drag == DR_PAN || drag == DR_PANP )
			midmotion(ep->wm_arg[0], ep->wm_arg[1]);
		else if ( drag != DR_NONE )
			canvmotion(ep->wm_arg[0], ep->wm_arg[1]);
		else
			hovermotion(ep->wm_arg[0], ep->wm_arg[1]);
		break;

	case E_MENU:
		evmenu(ep->wm_arg[0]);
		break;

	case E_QUIT:
		exit(0);
	}
	return 0;
}
