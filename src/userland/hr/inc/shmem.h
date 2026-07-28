/*
 * shmem.h - hrgui shared VRAM-tail layout (GUI.md sec 1.3 / 2.9 / 3.6).
 *
 * The framebuffer card decodes 128 KB but the visible 1024x800 1bpp bitmap uses
 * only 100 KB; the tail of segment 0x3B (offsets 0x9000..0xFFFF, 28 KB) is real
 * RAM mapped USER-ACCESSIBLE into every process (attribute 0x00, exactly like
 * the bitmap itself -- GUI.md sec 1.2), so it is the one shared memory this V7
 * kernel has.  hrgui uses it for READ-MOSTLY shared data only:
 *   - the system font(s), loaded once by the server from /usr/hr/fonts/*.hf, so
 *     the server AND every direct-render client blit glyphs from a single copy
 *     with no relink and no kernel trap;
 *   - the per-window clip descriptors, so a client can clip its own drawing to
 *     the visible parts of its window (direct-render, Model A).
 * Pixels go straight to the framebuffer and control/events stay on pipes;
 * neither travels through here.  Budget (of 28 KB): 2 font slots 12 KB + globals
 * + 14 descriptors ~1.6 KB = ~14 KB, leaving ~14 KB spare.
 */
#ifndef HRSHMEM_H
#define HRSHMEM_H

#define HRTAIL		((char *)0x3b009000L)	/* base of the 28 KB free tail */
#define HRTAILSZ	0x7000

/* ---- system fonts (server loads a .hf file into each slot) ---------------- *
 * A .hf file IS this struct: header words first/nch/cellw/cellh, then
 * nch*cellh glyph words (bit15 = leftmost pixel, set = ink; raw gall.c bits,
 * white-on-black -> blit L_NSRC for black-on-white). */
#define SHM_FONT0	0x0000		/* gallant.hf 12x25 terminal   (4758 B) */
#define SHM_FONT1	0x1800		/* gacha.hf    9x16 UI chrome   (3048 B) */
#define SHM_FONT2	0x2800		/* sail.hf     6x8  icon labels (1528 B) */
/* fonts occupy 0x0000..0x3000; globals + descriptors follow.  Symbolic uses: */
#define SHM_FTERM	SHM_FONT0	/* terminal content */
#define SHM_FUI		SHM_FONT1	/* title bars, menus */
#define SHM_FICON	SHM_FONT2	/* minimized-icon labels */

typedef struct {
	short		first;		/* first char code                 */
	short		nch;		/* number of glyphs                */
	short		cellw;		/* glyph cell width  px            */
	short		cellh;		/* glyph cell height px = words/glyph */
	unsigned short	bits[1];	/* nch*cellh glyph words follow    */
} HRFONT;

#define hr_font(slot)	((HRFONT *)(HRTAIL + (slot)))

/* ---- global / cursor state ----------------------------------------------- */
#define SHM_GLOB	0x3000
#define HR_MAGIC	0x4846		/* 'HF' -- set by the server after init */
typedef struct {
	short	magic;			/* HR_MAGIC once the tail is initialised */
	short	curx, cury;		/* cursor hotspot, framebuffer coords   */
	short	curon;			/* 1 = cursor currently drawn           */
} HRGLOB;
#define hr_glob()	((HRGLOB *)(HRTAIL + SHM_GLOB))

/* ---- per-window surface descriptor (the clip contract, GUI.md 2.9) -------- *
 * The server rewrites surf[wid] on every geometry / z-order change, bumping seq
 * to odd while writing and back to even when done; a client reads {seq, fields,
 * seq} and retries while seq is odd or changed. */
#define SHM_SURF	0x3100
#define SHM_MAXVIS	12		/* max disjoint visible rects per window */
typedef struct { short x0, y0, x1, y1; } HRRECT;
typedef struct {
	short	seq;			/* seqlock: odd while server writing */
	short	mapped;			/* 1 = live direct-render window     */
	short	ox, oy;			/* content origin, fb coords (ox 16-aligned) */
	short	cw, ch;			/* content size px                   */
	short	nvis;			/* number of valid vis[] rects       */
	HRRECT	vis[SHM_MAXVIS];	/* visible content sub-rects, fb coords */
} HRSURF;
#define hr_surf(wid)	((HRSURF *)(HRTAIL + SHM_SURF) + (wid))

#endif /* HRSHMEM_H */
