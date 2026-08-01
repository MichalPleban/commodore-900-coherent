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
	short	overlay;		/* 1 = server transient overlay (menu /  */
					/* ghost drag) is up: clients must NOT   */
					/* draw, or they paint over it (it is not */
					/* a layer, so their clip can't exclude  */
					/* it).  They keep ingesting; only the   */
					/* blit is skipped until it clears.      */
	short	stacking;		/* 1 = the server holds the drawing lock */
					/* for a layer/redraw op (its clip changes */
					/* + restacking blits are in flight): a  */
					/* fully-visible client must SUPPRESS its */
					/* lock-free fast path and draw under the */
					/* global lock instead, so it serialises  */
					/* with the server rather than racing it. */
					/* (Unlike overlay it does not skip the   */
					/* draw -- it only forces the locked path.) */
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

/* ---- global drawing lock (GUI.md race fix), futex-style ------------------- *
 * A binary lock word at SHM_LOCK (0 = free, 0xFFFF = held) taken with the Z8001
 * TSET (hrtas.s hr_tas) serialising every framebuffer / clip / z-order writer:
 * a client draw primitive, a server layout/redraw op.  The uncontended case is
 * pure userland -- NO system call; only on real contention does a waiter trap
 * into the hr driver to block (CIOMLOCK) and a releaser to wake it (CIOMUNLOCK).
 * SHM_WAIT is the kernel-maintained count of blocked waiters: hr_unlock reads it
 * and skips the wake syscall when it is 0.  SHM_OWNER is the holder's pid,
 * stamped so the driver's watchdog can reclaim the lock if a client dies holding
 * it.  The kernel hr driver mirrors these three addresses (it cannot include
 * this header) -- keep in sync.  Past the SHM_SURF clip table (14 windows *
 * ~110 B = ~0x604, ending ~0x3704), so 0x3800.. is clear. */
#define SHM_LOCK	0x3800		/* futex word: 0 free / 0xFFFF held    */
#define SHM_WAIT	0x3802		/* blocked-waiter count (kernel-owned) */
#define SHM_OWNER	0x3804		/* pid of current holder (0 if none)   */
#define hr_lockw()	((short *)(HRTAIL + SHM_LOCK))	/* kept for the call sites */
extern int	hr_tas();		/* atomic test-and-set (hrtas.s)           */
extern int	hr_lock();		/* acquire the drawing lock                */
extern int	hr_unlock();		/* release it                              */

/* ---- topmost fast-path drain flags (SHM_INDRAW) -------------------------- *
 * One byte per window, written ONLY by that window's client and read by the
 * server.  The lock-free topmost fast path (clgfx cl_pbegin) sets its own byte
 * BEFORE it tests `stacking' and clears it in cl_pend; the server, after it
 * raises `stacking' inside srvlock, DRAINS these to 0 before it restacks.  That
 * is a two-flag (Dekker) handshake on `stacking' plus a drain: given ordered
 * stores (this in-order CPU is sequentially consistent), a fast-path blit and a
 * restacking op can never overlap, so a stale-clip client can never scribble a
 * window the server is covering -- WITHOUT the fast path ever taking the lock.
 * Single writer per byte, so no atomic is needed (this V7 CPU has only TSET).
 * Reached through hr_setdraw/hr_getdraw (hrlock.c) so the store doubles as the
 * ordering barrier before the client reads `stacking' and the server's repeated
 * load in the drain loop cannot be hoisted -- this K&R compiler has no volatile. */
#define SHM_INDRAW	0x3820		/* MAX_WINDOWS bytes: per-window "drawing now" */
extern int	hr_setdraw();		/* client: set/clear its fast-path flag  */
extern int	hr_getdraw();		/* server: read a window's flag (drain)  */

/* Slow-path ioctls into the hr driver (must match the driver's hr.h).  Taken
 * only on contention -- see hrlock.c. */
#define CIOMLOCK	('c'<<8 | 20)	/* block until the drawing lock is ours */
#define CIOMUNLOCK	('c'<<8 | 21)	/* release the lock and wake a waiter   */

#endif /* HRSHMEM_H */
