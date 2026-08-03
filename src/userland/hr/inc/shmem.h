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
 * neither travels through here.  Budget (of 28 KB): 3 font slots ~7.4 KB of the
 * 12 KB reserved + globals + 16 descriptors ~1.8 KB, leaving ~14 KB spare.
 */
#ifndef HRSHMEM_H
#define HRSHMEM_H

#define HRTAIL		((char *)0x3b009000L)	/* base of the 28 KB free tail */
#define HRTAILSZ	0x7000

/* ---- system fonts (server loads a .hf file into each slot) ---------------- *
 * A .hf file IS this struct: header words first/nch/cellw/cellh, then
 * nch*cellh glyph words (bit15 = leftmost pixel, set = ink; ink=1,
 * white-on-black -> blit L_NSRC for black-on-white).
 * The terminal font is 8 px wide ON PURPOSE: one glyph row is exactly one byte
 * and a window's content origin is 16-aligned, so every character cell lands on
 * a byte boundary (see zterm/clgfx).  Slot offsets are unchanged from the old
 * 12x22 gallant terminal font, so slot 0 now has 3.3 KB of slack. */
#define SHM_FONT0	0x0000		/* gacha.r.hf  8x15 terminal   (2858 B) */
#define SHM_FONT1	0x1800		/* gacha.b.hf  9x16 UI chrome   (3048 B) */
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
 * this header) -- keep in sync.  Past the SHM_SURF clip table (16 windows *
 * ~110 B = 0x6E0, ending 0x37E0), so 0x3800.. is clear -- but only just: a
 * MAX_WINDOWS past 16, or a bigger SHM_MAXVIS, must move these three words (and
 * the driver's mirrored addresses) up into the ~14 KB of spare tail. */
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

/* ---- PRIMARY selection (select-to-copy, middle-click to paste) ------------ *
 * TWO STORES, EITHER/OR, AND THE OWNER PICKS.  A selection lives WHOLLY in the
 * tail (sq_file == 0) or WHOLLY in a file (sq_file == the writer's pid, the body
 * in HRSEL_PFX<pid>) -- never split across both, and never migrated from one to
 * the other behind the caller's back by some size threshold inside the library.
 * The owner knows what it is copying and declares the store to hr_selopen():
 *
 *   HRSEL_MEM   the tail.  Costs no system call at all, so this is what a
 *               terminal uses -- a full 80x25 screen is 2025 bytes, under
 *               HRSEL_INL by construction, so a zterm copy never touches a disk.
 *   HRSEL_FILE  a file, for an owner (an editor) that cannot bound its content.
 *
 * The mode is a property of the WRITE side only: hr_sellen/hr_selread resolve
 * sq_file themselves, so a paste handles either store with one code path -- which
 * is what lets a zterm paste a selection far larger than the tail store even
 * though it only ever writes HRSEL_MEM.
 *
 * Readers take NO lock: they retry on sq_seq (odd = a writer is publishing), the
 * same seqlock idiom as HRSURF above. */
#define SHM_PRIM	0x3900		/* the PRIMARY selection: header + body */
#define HRSEL_INL	2048		/* tail-store capacity (80x25 = 2025 B) */
#define HRSEL_PFX	"/tmp/.hrsel"	/* file store is HRSEL_PFX<pid>         */
#define HRSEL_MEM	0		/* body lives wholly in the tail        */
#define HRSEL_FILE	1		/* body lives wholly in a file          */
#define HRSEL_MAGIC	0x5351		/* 'SQ' -- the store has been initialised */
typedef struct {
	short	sq_magic;		/* HRSEL_MAGIC; the tail is garbage RAM  */
					/* at power-on, so the first writer      */
					/* stamps this and zeroes the rest --    */
					/* which is what lets hrclip work on a   */
					/* boot where zview never ran            */
	short	sq_seq;			/* seqlock: odd while a writer publishes */
	short	sq_gen;			/* bumped on every new selection         */
	short	sq_owner;		/* wid that owns it, -1 = none           */
	short	sq_file;		/* 0 = body wholly in sq_data, else the  */
					/* writer's pid and the body is wholly   */
					/* in HRSEL_PFX<pid> -- never both       */
	long	sq_len;			/* TOTAL length, whichever store         */
	char	sq_data[HRSEL_INL];	/* the body, when sq_file == 0           */
} HRSEL;				/* 2062 B: 0x3900..0x410E                */
#define hr_prim()	((HRSEL *)(HRTAIL + SHM_PRIM))

/* The selection lock -- deliberately NOT the drawing lock above.  SHM_LOCK
 * serialises every client's blits and the server's restacking, so taking it here
 * would stall the whole UI for as long as a selection write ran, and a file-store
 * write is disk I/O.  This is its own word, and the critical section it guards
 * contains no system call and no file I/O: the FILE store writes and closes with
 * NO lock held and takes this one only to publish the header (a few word stores),
 * which is exactly why hr_selopen() makes the caller declare the store up front.
 *
 * Taken with the same TSET as the drawing lock but with NO kernel slow path --
 * the driver's CIOMLOCK is hardwired to SHM_LOCK's address, and a second futex is
 * not worth a driver change for a lock held microseconds.  A waiter spins,
 * bounded, and then STEALS the word if SHM_SELTIME says the holder has had it for
 * seconds (kill(pid,0) is not an option: this kernel rejects signal 0 with EINVAL,
 * coh/sys1.c ukill).  A legitimate holder never holds it for a second; a dropped
 * selection is harmless, a permanently wedged one is not. */
#define SHM_SELLOCK	0x4110		/* TSET word: 0 free / 0xFFFF held        */
#define SHM_SELPID	0x4112		/* holder's pid (diagnostic)              */
#define SHM_SELTIME	0x4114		/* long: time() when taken, for the steal */
#define SHM_SELPROBE	0x4118		/* scratch word: hr_selok's write/read-back */
					/* ... ends 0x411A; spare tail follows    */

/* ---- connect acknowledgements ------------------------------------------- *
 * The server answers a C_CONNECT with E_CONNECTED over the client's event
 * pipe.  For a client the server FORKED that pipe is an anonymous pipe(2) and
 * is completely reliable.  For a client started from a shell (the desktop rc)
 * it is a named pipe the client mknod'd, and that channel has proved NOT to be
 * reliable: with two such clients starting together the server opens the right
 * inode (verified by fstat on both ends) and write() returns 16, yet the second
 * client never sees a byte -- it re-read for 30 s and got nothing.  The result
 * was a window that exists but is never drawn.
 *
 * So the ACK does not travel that way any more.  The server also publishes it
 * here, in the tail -- the one shared medium in this system that is known
 * solid -- and the client takes whichever arrives first.  The pipe is still
 * used for everything afterwards (expose/resize/input); this only removes the
 * handshake's dependence on it. */
#define SHM_ACK		0x4120		/* HRACK_N slots */
#define HRACK_N		16		/* == MAX_WINDOWS (gfx/smgr_defs.h), which
					 * this header deliberately does not pull in */
typedef struct {
	short	ak_pid;			/* client that asked (0 = slot free) */
	short	ak_wid;			/* window it was given               */
	short	ak_w, ak_h;		/* granted content size              */
} HRACK;
#define hr_ack(i)	((HRACK *)(HRTAIL + SHM_ACK) + (i))
					/* 16 * 8 = 0x80, ends 0x41A0 */

extern int	hr_ackput();	/* server: publish (pid -> wid, w, h)            */
extern int	hr_ackget();	/* client: hr_ackget(pid, &wid, &w, &h) -> 1/0   */
extern int	hr_ackclr();	/* server: drop a pid's slot / clear all (pid 0) */

/* ---- per-window event rings (server -> client) --------------------------- *
 * This REPLACES the per-client event pipe.  A pipe was the wrong medium here:
 * on this kernel a pipe IS a file (coh/pipe.c pread/pwrite call fread/fwrite on
 * the inode), so every event cost block allocation and buffer-cache traffic,
 * launching a client cost a mknod + unlink in /tmp, and the channel needed a
 * writable filesystem.  It was also unreliable in exactly the case that matters
 * -- see [SHM_ACK above].  Events are small, fixed-size and control-rate: they
 * belong in the shared tail, like everything else here.
 *
 * One ring per window, SINGLE producer (the server) and SINGLE consumer (that
 * window's client), so no lock is needed -- just ordered stores, which this
 * in-order CPU gives (the same argument SHM_INDRAW rests on).  eq_head and
 * eq_tail free-run and are masked, so the ring holds EVQ_SLOTS events and the
 * wrap is a mask, not a divide (GUI.md sec 3.7).
 *
 * A FULL ring cannot block the server -- that was the old sendev() hazard which
 * forced the qexpose/flushexp deferral.  It sets eq_over instead and drops the
 * event; the client treats "I overflowed" as "repaint everything", which for
 * E_EXPOSE is exactly the right semantics anyway.
 *
 * Blocking is the one thing shared memory cannot do by itself: a client must
 * SLEEP when its ring is empty (there is no select(), and busy-polling would
 * burn the whole 6 MHz CPU).  So the driver provides a doorbell, CIOEVWAIT /
 * CIOEVWAKE, used exactly like the drawing-lock futex: the kernel is entered
 * only when a client actually has to block, and an event that is already queued
 * costs NO system call at all.  eq_wait says a consumer is asleep, so the server
 * only traps when there is really someone to wake.
 *
 * EVQ_CONNECT is a 17th ring used only for the handshake: a client has no window
 * id yet, so it cannot wait on its own ring.  The server bumps this one after
 * publishing an ack (SHM_ACK) and the client re-checks the table. */
#define SHM_EVQ		0x4200
#define EVQ_SLOTS	16		/* power of two: wrap is a mask */
#define EVQ_MASK	(EVQ_SLOTS - 1)
#define EVQ_CONNECT	HRACK_N		/* the handshake ring (index 16)  */
#define EVQ_N		(HRACK_N + 1)	/* per window, plus that one      */
typedef struct {
	short	eq_head;		/* producer index, free-running   */
	short	eq_tail;		/* consumer index, free-running   */
	short	eq_wait;		/* 1 = consumer asleep in CIOEVWAIT */
	short	eq_over;		/* 1 = dropped events; repaint all  */
	short	eq_ev[EVQ_SLOTS][8];	/* one WMSG (wire.h) per slot       */
} HREVQ;				/* 264 B; 17 rings = 0x4200..0x5388 */
#define hr_evq(i)	((HREVQ *)(HRTAIL + SHM_EVQ) + (i))

/* Driver doorbell (must match the driver's hr.h). */
#define CIOEVWAIT	('c'<<8 | 22)	/* sleep until ring <arg> is non-empty */
#define CIOEVWAKE	('c'<<8 | 23)	/* wake whoever sleeps on ring <arg>   */

extern int	hr_evinit();	/* server: reset one ring (-1 = all)             */
extern int	hr_evput();	/* server: hr_evput(i, wmsg) -> 0, or -1 if full */
extern int	hr_evget();	/* client: hr_evget(i, wmsg) -> 1 if one came    */
extern int	hr_evwait();	/* client: block until ring i has something      */
extern int	hr_evover();	/* client: read+clear the overflow flag          */

extern int	hr_selopen();	/* writer: hr_selopen(wid, HRSEL_MEM|HRSEL_FILE) */
extern int	hr_selwrite();	/* writer: hr_selwrite(buf, len), append         */
extern int	hr_selclose();	/* writer: publish (or discard, on error)        */
extern long	hr_sellen();	/* reader: total length, either store            */
extern int	hr_selread();	/* reader: hr_selread(off, buf, max) -> n        */
extern int	hr_selgen();	/* reader: generation, to notice a change        */
extern int	hr_selowner();	/* reader: wid that owns it, -1 = none           */
extern int	hr_selinit();	/* server: initialise the header at start-up     */
extern int	hr_selok();	/* 1 if the tail is real RAM (no card -> 0)      */

#endif /* HRSHMEM_H */
