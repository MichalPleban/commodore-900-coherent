/*
 * wserver.c - the hrgui window server (GUI.md Phase 1).
 *
 * A single userland process that owns every pixel.  It draws through the
 * salvaged rendering engine libhrgfx (which it links), keeps the per-window
 * WSTRUCT/LAYER table, and does clipping / z-order / damage entirely with the
 * engine's layer.c primitives -- so "redraw correctly when partially covered"
 * comes straight from make_vis_list()/perform_update(), not from new code here.
 *
 * It replaces the old jlib/coroutine/per-daemon stack (GUI.md sec 0) with:
 *   - ONE blocking read() on a shared command pipe (all clients + the input
 *     pump write to it); no select(), no coroutines (GUI.md sec 3.5, 4.4).
 *   - a per-client event pipe for expose/quit/input.
 *   - fork+exec client launch with inherited pipe fds (GUI.md sec 3.3).
 *
 * The engine's damage path (layer.c perform_update -> sendmsg) is captured by
 * gfx_reply_hook and turned into E_EXPOSE events to the affected client, which
 * repaints (redraw-on-expose, GUI.md sec 2.10).
 *
 * Phase 1a is self-driven: it launches a few overlapping clocks and, on its own
 * timer, raises them in turn -- exercising cover/uncover so the expose path can
 * be seen ticking correctly under partial cover without any mouse yet.  Phase 1b
 * adds the input pump + mouse-driven menu/move/resize/raise/minimise.
 */
#include <stdio.h>
#include <signal.h>
#include "smgr.h"
#include "wire.h"
#include "shmem.h"

extern BITMAP	display;
extern LAYER	*newlayer();
extern int	*screen_addr();
extern void	background(), outline(), gkLine(), SM_Move(), SM_Line(),
		SM_Point(), SM_ClrClip(), upfront(), pushback(), dellayer(),
		new_dimensions();
extern RECT	R_inset();
extern RECT	R_Intersection();
extern char	*malloc();
extern int	bitblt(), R_null();
extern POINT	SM_Mouse_Pos;		/* engine cursor pos (bitblt hide gating) */

extern int	(*gfx_reply_hook)();

/* Per-window client bookkeeping, indexed by window id (0..MAX_WINDOWS-1). */
struct win {
	int	used;
	int	pid;		/* client process               */
	int	evfd;		/* server -> client event pipe  */
	int	min;		/* 1 if minimised to a desktop icon */
	int	islot;		/* desktop-icon slot while minimised */
	int	sx, sy, sw, sh;	/* saved geometry while minimised */
	WSTRUCT	*wp;		/* stashed WSTRUCT while minimised (out of wtbl) */
	char	title[24];	/* displayed title (base, plus " #N" if duplicated) */
	char	base[16];	/* app name, before any #N suffix */
	int	inst;		/* stable instance number among same-base windows */
	char	icon[40];	/* .icn file for the desktop icon */
} wins[MAX_WINDOWS];

int	cmdfd;			/* server end (read) of the shared command pipe */
int	cmdwr;			/* write end kept so children can inherit it    */

/* Launchable applications, read from /usr/hr/etc/apps at startup (GUI.md: a
 * data-driven launcher menu so new software installs without recompiling the
 * server).  One line per app: name:iconpath:execpath:contentW:contentH .  The
 * table is fixed-size bss (no heap) and the strings are filled at runtime, so
 * nothing here bloats the near-full data segment. */
#define MAX_APPS	12
struct app {
	char	name[16];	/* menu label / window title base */
	char	icon[40];	/* .icn file (used by the desktop icons, Phase 4) */
	char	path[40];	/* executable                                     */
	int	w, h;		/* default content size in pixels                 */
	int	multi;		/* 1 = allow many instances; 0 = single (re-raise) */
} apps[MAX_APPS];
int	napps;

int	nwins;			/* count of live windows          */
int	focuswid = -1;		/* window that receives keystrokes (Phase 2) */

/* Fonts live in the shared VRAM tail (shmem.h): the server loads three .hf files
 * into it once at startup, then the server AND every direct-render client blit
 * glyphs straight from that single copy with the asm bitblt (a whole glyph ROW
 * per masked word op -- never the per-pixel path the old kernel CIOGLYPH used).
 * SHM_FTERM = gallant 12x25 (terminal), SHM_FUI = gacha 9x16 (title bars/menus),
 * SHM_FICON = sail 6x8 (minimized-icon labels).  Terminal cell metrics drive
 * C_TEXT/C_ERASE and the cols/rows handed to terminal clients. */
#define HRFW	12		/* gallant (terminal) glyph cell width  (px) */
#define HRFH	25		/* gallant (terminal) glyph cell height (px) */

int	termcw = HRFW;		/* terminal cell width  (px) */
int	termch = HRFH;		/* terminal cell height (px) */
RECT	srvhrclip;		/* extra clip for glyphs (title bar); off if empty */

/* mouse / interaction state (Phase 1b) */
int	mx, my;			/* current pointer position (global coords) */
int	dragwid = -1;		/* window being dragged/resized, or -1      */
int	dragmode;		/* 0 none, 1 move, 2 resize                 */
int	grabx, graby;		/* pointer offset into the window at grab   */
extern int who_top_at();
int	curfd = -1;		/* a driver fd for cursor on/off (CIOMSE*) */
int	pumppid = -1;		/* the input-pump child (killed on WM quit) */

/* Cursor arbitration hooks (installed into libhrgfx): the driver draws an XOR
 * cursor, so hide it around each server blit or it leaves trails (GUI.md 6.2). */
/* Reference-counted so nested hide/show compose (the driver's cursor flag is a
 * plain boolean): only the OUTERMOST hide actually removes the XOR cursor and
 * the outermost show restores it, so a top-level op can bracket a whole blit
 * sequence even though inner helpers (outline, drawicon, bitblt) bracket too. */
int	curdepth;
srv_curhide()
{
	if ( curfd >= 0 && curdepth++ == 0 )
		ioctl(curfd, CIOMSEOFF, (char *)0);
}
srv_curshow()
{
	if ( curfd >= 0 && curdepth > 0 && --curdepth == 0 )
		ioctl(curfd, CIOMSEON, (char *)0);
}

/* The global GUI drawing lock (hrlock.s / shmem.h), held while the server
 * changes the layer stack / clip descriptors / framebuffer so no direct-render
 * client (or the driver's XOR cursor) can interleave -- this is what stops a
 * busy client painting into a window the server is mid-way through stacking on
 * top (the persistent "root bleeds into the new terminal" race), and the stray
 * cursor.  RECURSIVE (a per-process depth count): the engine's reply hook
 * (onreply) draws from DEEP inside layer ops (upfront/dellayer/new_dimensions ->
 * perform_update -> sendmsg), so a top-level op and the hook both bracket; only
 * the outermost pair actually takes/drops the single TSET lock. */
int	locklevel;
srvlock()
{
	int w;
	long spin;

	if ( locklevel++ == 0 )
	{
		hr_lock(hr_lockw());
		/* Freeze the clients' lock-free fast path (clgfx cl_pbegin) for as long
		 * as we hold the lock: while the server is restacking / redrawing, a
		 * fully-visible window's clip may be about to change and our blits target
		 * its pixels, so it must draw under the lock (serialised with us), not
		 * lock-free.  Steady state (server idle in read()) leaves this 0, so the
		 * fast path is available whenever it matters. */
		hr_glob()->stacking = 1;
		/* Drain any client already mid lock-free blit.  It set SHM_INDRAW[w]
		 * before it tested `stacking' (clgfx cl_pbegin), so now that we have
		 * raised `stacking' we are guaranteed to see it (Dekker: ordered stores),
		 * and stacking=1 stops it starting another once this one ends -- so
		 * waiting the flag out makes a restack and a fast blit mutually exclusive.
		 * The client needs no lock to finish, so it drains under preemption; the
		 * spin cap is only a backstop for a client that died mid-primitive (then
		 * we proceed -- the flag is cleared when its window is torn down). */
		for ( w = 0; w < MAX_WINDOWS; w++ )
			for ( spin = 0; hr_getdraw(w) && spin < 1000000L; spin++ )
				;
	}
}

/* Exposes must NOT be sent while the lock is held.  The engine's reply hook
 * fires E_EXPOSE from inside layer ops, and sending it can BLOCK on a client
 * whose event pipe is full -- and that client may be spinning on this very lock
 * (its mux backed up behind a flood).  That deadlock is exactly what let one
 * stale-clip primitive slip through (the spin-breaker firing).  So queue the
 * exposes and flush them the instant the lock is dropped, never while held. */
int	pendexp;			/* bitmask of wids awaiting E_EXPOSE */
flushexp()
{
	int w;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( pendexp & (1 << w) )
		{
			pendexp &= ~(1 << w);
			if ( wins[w].used && wtbl[w] )
				sendev(w, E_EXPOSE, 0, 0,
					wtbl[w]->wn_Psize.x, wtbl[w]->wn_Psize.y);
		}
}
srvunlock()
{
	if ( locklevel > 0 && --locklevel == 0 )
	{
		hr_glob()->stacking = 0;	/* clients may fast-path again */
		hr_unlock(hr_lockw());
		flushexp();		/* safe to block on pipe writes now */
	}
}

/* Request a full-content E_EXPOSE for wid.  While the lock is held it is queued
 * (flushed by srvunlock); otherwise it is sent immediately. */
qexpose(wid)
{
	if ( wid < 0 || wid >= MAX_WINDOWS )
		return;
	if ( locklevel == 0 )
	{
		if ( wins[wid].used && wtbl[wid] )
			sendev(wid, E_EXPOSE, 0, 0,
				wtbl[wid]->wn_Psize.x, wtbl[wid]->wn_Psize.y);
	}
	else
		pendexp |= 1 << wid;
}

/* the driver's default arrow cursor sprite (from the old smgr) */
int DEF_MOUSE[] = { 0xfffc, 0xfff8, 0xfff0, 0xffe0,
		    0xffc0, 0xffc0, 0xffe0, 0xfff0,
		    0xfff8, 0xfffc, 0xf3fe, 0xe1ff,
		    0x80ff, 0x007f, 0x003e, 0x001c };

/* the move/resize "hand" cursor (original desktop dmouse.c MOV_MOUSE): shown
 * while a ghost-drag is active, then DEF_MOUSE is restored. */
int MOV_MOUSE[] = { 0x0000, 0x01b0, 0x19b0, 0x19b6,
		    0x0db6, 0x0db6, 0x0ffe, 0x0ffe,
		    0x07fe, 0x67fe, 0x7ffe, 0x3ffe,
		    0x1ffc, 0x07fc, 0x07f8, 0x03f8 };

/* the menu cursor (original desktop dmouse.c MNU_MOUSE): a right-pointing arrow
 * shown while a pop-up menu is open, then DEF_MOUSE is restored. */
int MNU_MOUSE[] = { 0x0000, 0x0180, 0x01c0, 0x01e0,
		    0x01f0, 0xfff8, 0xfffc, 0xfffe,
		    0xffff, 0xfffe, 0xfffc, 0xfff8,
		    0x01f0, 0x01e0, 0x01c0, 0x0180 };

/* checkpoint tracing to the console (fd 2), for bring-up; off by default */
int	trace = 0;
extern int	strlen();
extern char	*strcpy();
extern char	*strncpy();
extern char	*strcat();
extern int	atoi();
extern int	strcmp();
static
dbg(s)
char *s;
{
	if ( trace )
		write(2, s, strlen(s));
}

int	dbgn;			/* limit tracing volume */
static
dnum(label, n)
char *label;
{
	char b[24];
	if ( !trace || dbgn++ > 60 )
		return;
	sprintf(b, "%s%d\n", label, n);
	write(2, b, strlen(b));
}

/* Debug log to a file on the (rw-mounted) root, extractable with disk.py after a
 * headless run.  Off unless /wslog was created at startup. */
int	srvlog = -1;
srvlogs(s)
char *s;
{
	if ( srvlog >= 0 )
		write(srvlog, s, strlen(s));
}
srvlogn(label, n)
char *label;
{
	char b[40];
	if ( srvlog < 0 )
		return;
	sprintf(b, "%s%d\n", label, n);
	write(srvlog, b, strlen(b));
}

/* ------------------------------------------------------------------ */
/* engine glue                                                        */
/* ------------------------------------------------------------------ */

/* Load the working context gk from a window, run an op, save it back.  This is
 * exactly the smgr protocol (gk = *wtbl[wid]; ... ; *wtbl[wid] = gk). */
#define LOADW(w)	(gk = *wtbl[w])
#define SAVEW(w)	(*wtbl[w] = gk)

/* Send one event record to a client. */
sendev(wid, type, a0, a1, a2, a3)
{
	WMSG e;

	if ( !wins[wid].used )
		return;
	e.wm_type = type;
	e.wm_wid = wid;
	e.wm_arg[0] = a0; e.wm_arg[1] = a1;
	e.wm_arg[2] = a2; e.wm_arg[3] = a3;
	e.wm_arg[4] = e.wm_arg[5] = 0;
	write(wins[wid].evfd, &e, sizeof(e));
}

/*
 * gfx_reply_hook: the engine calls sendmsg(&msg) from layer.c's perform_update
 * to announce that a window's area must be repainted (WM_UPDATE) -- i.e. it was
 * uncovered.  Turn that into an E_EXPOSE to the owning client.  Queries
 * (WM_REPLY) are not used by the clock, so ignore them.
 */
onreply(m)
MESSAGE *m;
{
	int wid, cmd;

	cmd = m->msg_Cmd;
	wid = (m->msg_Data[0] >> 8) & 0xff;
	if ( (cmd == WM_UPDATE || cmd == WM_RGNUPDATE) &&
	     wid >= 0 && wid < MAX_WINDOWS && wins[wid].used && wtbl[wid] )
	{
		/* outline() just repainted the (white) title-bar background for this
		 * window; relay the title text over it (gk is not window wid here). */
		WSTRUCT save;
		save = gk;
		gk = *wtbl[wid];
		srvtitle(wid);
		gk = save;
		/* full-content expose; a dumb client repaints everything.  QUEUED, not
		 * sent: this hook runs deep inside a locked layer op, and sending here
		 * could block on a client that is spinning on the lock (deadlock). */
		qexpose(wid);
	}
	return 0;
}

/* Initialise gk's graphics state to the engine defaults (gctrl.c gkReset). */
static
rstgraph()
{
	gkPen.pn_Width = 1;
	gkPen.pn_Height = 1;
	gkPen.pn_Pat = FP_FORE;
	gkLogop = L_TRUE;
	gkFpat = FP_FORE;
	gkBpat = FP_BACK;
	gkFcolor = BLACK;
	gkBcolor = WHITE;
	gkFont.fi_Id = SYS_FID;
}

/* ------------------------------------------------------------------ */
/* publish per-window clip descriptors to the shared VRAM tail        */
/* ------------------------------------------------------------------ */
/* Direct-render clients (Model A, GUI.md 2.9) draw their own content straight to
 * VRAM, so they must know where their content is and which parts are visible.
 * After any geometry / z-order change the server rewrites each window's
 * descriptor in the tail (shmem.h) under a seqlock: seq odd while writing, even
 * when done; a client reads {seq, fields, seq} and retries while seq is odd or
 * changed.  Read straight from wtbl[] so the global gk is never disturbed. */
publish_surf(wid)
{
	HRSURF *sp;
	WSTRUCT *wp;
	LAYER *lp;
	SM_REGION *rp;
	RECT cr, dr;
	int n;

	sp = hr_surf(wid);
	sp->seq++;					/* odd: writing */
	wp = wtbl[wid];
	if ( !wins[wid].used || wins[wid].min || wp == (WSTRUCT *)NULL
	     || wp->wn_Layer == (LAYER *)NULL )
	{
		sp->mapped = 0;
		sp->nvis = 0;
		sp->seq++;				/* even */
		hr_setdraw(wid, 0);		/* unmapped: not drawing -> clear drain flag */
		return;
	}
	cr = wp->wn_Crect;
	lp = (LAYER *)wp->wn_Layer;
	sp->ox = cr.origin.x;  sp->oy = cr.origin.y;
	sp->cw = cr.corner.x - cr.origin.x;
	sp->ch = cr.corner.y - cr.origin.y;
	n = 0;
	for ( rp = lp->reg; rp < lp->reg + MAX_LRBUF && n < SHM_MAXVIS; rp++ )
	{
		if ( rp->flag == L_EMPTY )
			break;
		if ( rp->flag != L_VISIBLE )
			continue;
		dr = R_Intersection(cr, rp->bm.rect);
		if ( R_null(dr) )
			continue;
		sp->vis[n].x0 = dr.origin.x;  sp->vis[n].y0 = dr.origin.y;
		sp->vis[n].x1 = dr.corner.x;  sp->vis[n].y1 = dr.corner.y;
		n++;
	}
	sp->nvis = n;
	sp->mapped = 1;
	sp->seq++;					/* even: done */
}

/* Republish every window: any single op can cover/uncover others. */
publish_all()
{
	int w;
	for ( w = 0; w < MAX_WINDOWS; w++ )
		publish_surf(w);
}

/* Create a window at rectangle r, register it as window `wid'.  Mirrors the
 * drawing-relevant half of wmgr.c SM_Create, minus the message plumbing. */
mkwin(wid, r)
RECT r;
{
	WSTRUCT *wp;

	srvlock();			/* layer change + publish + decorate = atomic */
	gkWid = wid;
	/* Logical (0,0) must map to the CONTENT origin (below the title bar), not
	 * the layer corner: gkToGlobal() adds the layer rect origin, so bias the
	 * logical origin by the content inset.  Otherwise a client (e.g. the clock)
	 * draws from the window corner and the title bar overlaps its top. */
	gkLorigin.x = -WD_BORDER;
	gkLorigin.y = -WD_TITLEH;
	gkCrect = r;
	gkPsize.x = r.corner.x - r.origin.x;
	gkPsize.y = r.corner.y - r.origin.y;
	gkWmgr = SMGR;
	gkEvmask = DEF_EVMASK;
	gkFlags = WT_FULLY_VIS;
	gkType = WT_OUTPUT;
	gk.wn_ascii = (int *)NULL;
	rstgraph();
	gkDp = r.origin;

	gkLayer = newlayer(r);
	dbg("  mk: newlayer\n");
	gkLayer->base = screen_addr(r.origin.x, r.origin.y);
	gkLayer->width = 1024;

	wp = (WSTRUCT *)malloc(sizeof(WSTRUCT));
	if ( wp == (WSTRUCT *)NULL ) { dbg("  mk: malloc NULL\n"); srvunlock(); return; }
	wtbl[wid] = wp;
	*wtbl[wid] = gk;
	outline(wid);				/* frame + shadow + title bar */
	srvtitle(wid);				/* title text */
	/* content: below the title bar, inside the frame, clear of the shadow */
	gkCrect.origin.x = r.origin.x + WD_BORDER;
	gkCrect.origin.y = r.origin.y + WD_TITLEH;
	gkCrect.corner.x = r.corner.x - WD_SHADOW - WD_BORDER;
	gkCrect.corner.y = r.corner.y - WD_SHADOW - WD_BORDER;
	*wtbl[wid] = gk;
	publish_all();				/* clip descriptors for direct-render */
	srvunlock();
	dbg("  mk: done\n");
}

/* ------------------------------------------------------------------ */
/* client lifecycle                                                   */
/* ------------------------------------------------------------------ */

/* (The former hardcoded launch()/launchterm() were replaced by the config-driven
 * launchapp() below.) */

/* Tear a window down (server-owned lifecycle, GUI.md sec 2.10). */
killwin(wid)
{
	char base[16];

	if ( !wins[wid].used )
		return;
	sendev(wid, E_QUIT, 0, 0, 0, 0);	/* outside the lock: may block */
	hr_setdraw(wid, 0);		/* client is gone: drop its drain flag so the */
					/* srvlock() below (and later ops) don't spin */
	srvlock();
	srvlogn("killwin ", wid);
	strcpy(base, wins[wid].base);
	gfx_cursor_hide();
	if ( wins[wid].min )
		drawicon(wid, 0);		/* erase its desktop icon */
	if ( wtbl[wid] )
	{
		LOADW(wid);
		dellayer(gkLayer);		/* uncovers + exposes those beneath */
		perform_update();
		free((char *)wtbl[wid]);
		wtbl[wid] = (WSTRUCT *)NULL;
	}
	close(wins[wid].evfd);
	wins[wid].used = 0;
	nwins--;
	relabel(base);				/* drop #N from a surviving sibling */
	gfx_cursor_show();
	publish_all();
	srvunlock();
	redraw_icons();				/* closing a window may uncover icons */
}

/* Raise a window to the front (click-to-raise / demo cycling). */
raisewin(wid)
{
	if ( !wins[wid].used || !wtbl[wid] )
		return;
	srvlock();
	focuswid = wid;				/* raised window takes the keyboard */
	LOADW(wid);
	gfx_cursor_hide();
	upfront(gkLayer);			/* exposes whatever it now covers... */
	SAVEW(wid);
	gfx_cursor_show();
	/* upfront already exposes the previously-obscured parts of OTHER
	 * windows via the reply hook; the raised window itself is fully drawn
	 * over, so ask its client to repaint too. */
	publish_all();				/* z-order changed: refresh all clips */
	qexpose(wid);				/* deferred: flushed by srvunlock */
	srvunlock();
}

/* Expose every mapped window (except exclwid) whose layer rect intersects the
 * rect (rx0,ry0)-(rx1,ry1): used when a window is hidden or pushed back to
 * repaint what it was covering (the engine reply hook does not reliably do it). */
expose_covered(exclwid, rx0, ry0, rx1, ry1)
{
	int w;
	LAYER *lp;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( w != exclwid && wins[w].used && !wins[w].min && wtbl[w] &&
		     (lp = wtbl[w]->wn_Layer) &&
		     lp->rect.origin.x < rx1 && lp->rect.corner.x > rx0 &&
		     lp->rect.origin.y < ry1 && lp->rect.corner.y > ry0 )
			sendev(w, E_EXPOSE, 0, 0,
				wtbl[w]->wn_Psize.x, wtbl[w]->wn_Psize.y);
}

/* Send a window to the back (window-menu "Back", the opposite of "Front"). */
backwin(wid)
{
	RECT r;

	if ( !wins[wid].used || !wtbl[wid] || !wtbl[wid]->wn_Layer )
		return;
	srvlock();
	r = wtbl[wid]->wn_Layer->rect;
	LOADW(wid);
	gfx_cursor_hide();
	pushback(gkLayer);			/* z-order change: now rearmost */
	SAVEW(wid);
	gfx_cursor_show();
	publish_all();
	srvunlock();
	/* windows that were under this one are now on top of it -> repaint the
	 * area it used to cover. */
	expose_covered(wid, r.origin.x, r.origin.y, r.corner.x, r.corner.y);
	redraw_icons();			/* this window may now cover/uncover icons */
}

/* ------------------------------------------------------------------ */
/* input: load /drv/hr and pump its events into the command pipe      */
/* ------------------------------------------------------------------ */

/* Load the hi-res driver (keyboard ISR + polled mouse + cursor).  A user
 * process cannot read the mouse ports itself, so we reuse the existing driver
 * purely as an input+cursor source (not its message-switch window system). */
loaddriver()
{
	int pid, status;

	pid = fork();
	if ( pid == 0 )
	{
		execl("/etc/load", "load", "/drv/hr", (char *)0);
		_exit(1);
	}
	while ( wait(&status) != pid )
		;
	return status;
}

/* Load the pseudo-terminal driver (/drv/pty, major 9).  It is a loadable to
 * keep it out of the 64K resident kernel; wterm needs it to allocate its
 * master/slave pair, so pull it in before any terminal app is launched.
 * Harmless if it is already loaded (load reports EDBUSY and we ignore it). */
loadpty()
{
	int pid, status;

	pid = fork();
	if ( pid == 0 )
	{
		execl("/etc/load", "load", "/drv/pty", (char *)0);
		_exit(1);
	}
	while ( wait(&status) != pid )
		;
	return status;
}

/* ------------------------------------------------------------------ */
/* keyboard: raw PC scancode -> ASCII                                 */
/* ------------------------------------------------------------------ */
/* The /drv/hr driver only delivers raw make/break scancodes via SM_KKEY; the
 * ASCII translation (shift/ctrl/caps state machine) lived in the discarded
 * message layer (kev.c SM_Keyboard, GUI.md 5.3).  Ported verbatim here so the
 * input pump can hand the server real ASCII. */
#define KB_KEYUP	0x80
#define KB_KEYSC	0x7f
#define KB_LSHIFT	(0x2a-1)
#define KB_RSHIFT	(0x36-1)
#define KB_CTRL		(0x1d-1)
#define KB_ALT		(0x38-1)
#define KB_CAPLOCK	(0x3a-1)
#define KB_SRS	0x01
#define KB_SLS	0x02
#define KB_CTS	0x04
#define KB_ALS	0x08
#define KB_CPLS	0x10
#define KB_NMLS	0x20
#define KB_SHFT	0x80
#define KB_SES	(KB_SLS|KB_SRS)
#define KB_SS1	(KB_SLS|KB_SRS|KB_CTS)
#define KB_LET	(KB_SLS|KB_SRS|KB_CPLS|KB_CTS)
#define XXX	0377
#define SPC	0376
#define DEL	0x7f

static unsigned char lmaptab[] ={
	     '\33',  '1',  '2',  '3',  '4',  '5',  '6',
	 '7',  '8',  '9',  '0',  '-',  '=', '\b', '\t',
	 'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
	 'o',  'p',  '[',  ']', '\r',  XXX,  'a',  's',
	 'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
	 '\'', '`',  XXX,  '\\',  'z',  'x',  'c',  'v',
	 'b',  'n',  'm',  ',',  '.',  '/',  XXX,  SPC,
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,
	 SPC,  SPC,  SPC,  SPC,  SPC,  DEL,  SPC,  SPC,
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  '\r', SPC,
	 SPC,  SPC,  SPC,  XXX,  XXX
};
static unsigned char umaptab[] ={
	     '\33',  '!',  '@',  '#',  '$',  '%',  '^',
	 '&',  '*',  '(',  ')',  '_',  '+', '\b', '\t',
	 'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
	 'O',  'P',  '{',  '}', '\r',  XXX,  'A',  'S',
	 'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
	 '"',  '~',  XXX,  '|',  'Z',  'X',  'C',  'V',
	 'B',  'N',  'M',  '<',  '>',  '?',  XXX,  SPC,
	 XXX,  ' ',  XXX,  SPC,  SPC,  SPC,  SPC,  SPC,
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  SPC,
	 SPC,  SPC,  '-',  SPC,  SPC,  SPC,  '+',  SPC,
	 SPC,  SPC,  SPC,  SPC,  SPC,  DEL,  SPC,  SPC,
	 SPC,  SPC,  SPC,  SPC,  SPC,  SPC,  '\r', SPC,
	 SPC,  SPC,  SPC,  XXX,  XXX
};
#define SS0	0
#define SS1	(KB_SLS|KB_SRS|KB_CTS)
#define SES	(KB_SLS|KB_SRS)
#define LET	(KB_SLS|KB_SRS|KB_CPLS|KB_CTS)
#define KEY	(KB_SLS|KB_SRS|KB_NMLS|0x40)
#define SHFT	KB_SHFT
static unsigned char smaptab[] ={
	       SS0,  SES,  SS1,  SES,  SES,  SES,  SS1,
	 SES,  SES,  SES,  SES,  SS1,  SES,  SS0,  SS0,
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  LET,
	 LET,  LET,  SS1,  SS1,  SS0, SHFT,  LET,  LET,
	 LET,  LET,  LET,  LET,  LET,  LET,  LET,  SES,
	 SES,  SS1, SHFT,  SS1,  LET,  LET,  LET,  LET,
	 LET,  LET,  LET,  SES,  SES,  SES, SHFT,  SS0,
	SHFT,  SS1, SHFT,  SS0,  SS0,  SS0,  SS0,  SS0,
	 SS0,  SS0,  SS0,  SS0,  SS0,  SS0,  KEY,  KEY,
	 KEY,  KEY,  SS0,  KEY,  KEY,  KEY,  SS0,  KEY,
	 KEY,  KEY,  KEY,  KEY,  SS0,  SS0,  SS0,  SS0,
	 SS0,  SS0,  SS0,  SS0,  SS0,  SS0,  SS0,  SS0,
	 SS0,  SS0,  SS0,  LET,  LET
};
#undef SHFT

static int kbshift = 0;

/* Translate a raw scancode to ASCII; return -1 for releases, modifiers and
 * dead/special keys (which the terminal ignores). */
keymap(r)
int r;
{
	register int c, s;

	r &= 0xff;
	if ( r == 0xff )
		return -1;
	c = (r & KB_KEYSC) - 1;
	if ( c < 0 || c >= sizeof(smaptab) )
		return -1;
	s = smaptab[c];
	if ( s & KB_SHFT )
	{
		if ( r & KB_KEYUP )
		{
			if ( c == KB_RSHIFT ) kbshift &= ~KB_SRS;
			else if ( c == KB_LSHIFT ) kbshift &= ~KB_SLS;
			else if ( c == KB_CTRL ) kbshift &= ~KB_CTS;
			else if ( c == KB_ALT ) kbshift &= ~KB_ALS;
		}
		else
		{
			if ( c == KB_LSHIFT ) kbshift |= KB_SLS;
			else if ( c == KB_RSHIFT ) kbshift |= KB_SRS;
			else if ( c == KB_CTRL ) kbshift |= KB_CTS;
			else if ( c == KB_ALT ) kbshift |= KB_ALS;
			else if ( c == KB_CAPLOCK ) kbshift ^= KB_CPLS;
		}
		return -1;
	}
	if ( r & KB_KEYUP )
		return -1;
	if ( kbshift & KB_CTS )
	{
		if ( s == KB_SS1 || s == KB_LET )
			c = umaptab[c] & 0x1f;
		else
			return -1;
	}
	else if ( s &= kbshift )
	{
		if ( kbshift & KB_SES )
			c = (s & (KB_CPLS|KB_NMLS)) ? lmaptab[c] : umaptab[c];
		else
			c = (s & (KB_CPLS|KB_NMLS)) ? umaptab[c] : lmaptab[c];
	}
	else
		c = lmaptab[c];
	if ( c == XXX || c == SPC )
		return -1;
	return c & 0xff;
}

/* Child process: register as the driver's event manager and forward every
 * keyboard/mouse event to the server as a C_INPUT record on the command pipe
 * (HR_CMDFD).  This is the V7 two-process split (GUI.md sec 4.5/7): the pump
 * blocks in CIOGETM while the server blocks in read(), and neither needs
 * select().  The driver draws the arrow cursor itself. */
runpump()
{
	int fd;
	MESSAGE m;
	WMSG c;

	fd = open("/dev/smgr", 2);
	if ( fd < 0 )
		_exit(1);
	ioctl(fd, CIOEVMGR);
	ioctl(fd, CIOMOUSE, DEF_MOUSE);
	ioctl(fd, CIOMSEON, (char *)0);

	c.wm_wid = 0;
	c.wm_type = C_INPUT;
	for (;;)
	{
		if ( ioctl(fd, CIOGETM, &m) < 0 )
			continue;
		if ( m.msg_Cmd == SM_MOUSE )
		{
			c.wm_arg[0] = IN_MOVE;
			c.wm_arg[1] = m.msg_Data[1] & 0x1fff;
			c.wm_arg[2] = m.msg_Data[2] & 0x1fff;
		}
		else if ( m.msg_Cmd == SM_MKEY )
		{
			c.wm_arg[0] = IN_BUTTON;
			c.wm_arg[1] = m.msg_Data[1] & 0x1fff;	/* x            */
			c.wm_arg[2] = m.msg_Data[2] & 0x1fff;	/* y            */
			c.wm_arg[3] = m.msg_Data[2] & 0xe000;	/* buttons down */
			c.wm_arg[4] = m.msg_Data[1] & 0xe000;	/* changed bits */
		}
		else if ( m.msg_Cmd == SM_KKEY )
		{
			int a = keymap(m.msg_Data[1]);
			if ( a < 0 )
				continue;	/* release / modifier / dead key */
			c.wm_arg[0] = IN_KEY;
			c.wm_arg[1] = a;
		}
		else
			continue;
		write(HR_CMDFD, &c, sizeof(c));
	}
}

startpump()
{
	int pid;

	pid = fork();
	if ( pid == 0 )
	{
		close(cmdfd);
		runpump();
		_exit(1);
	}
	return pid;
}

/* ------------------------------------------------------------------ */
/* window interaction (move / resize / minimise / raise)              */
/* ------------------------------------------------------------------ */

/* Hidden windows iconify to icons on the desktop (period-authentic: no dock).
 * Icons sit in a row across the lower desktop; each is a 48x48 .icn glyph with
 * the window title beneath.  Slots pack left-to-right (lowest free slot). */
#define ICONW	48			/* icon glyph size */
#define ICONLH	10			/* label height (one sail 6x8 row + pad) */
#define ICONCW	104			/* per-icon cell width */
#define ICONROWY (YMAX - (ICONW + ICONLH + 8))
#define iconx(slot) (12 + (slot) * ICONCW)

extern int	*texture[];
extern int	words_between();

/* Fill a rectangle on screen (display base, the working blit path). */
srvfill(r, patidx, op)
RECT r;
{
	BLTSTRUCT blt;
	BITMAP s;

	if ( r.corner.x <= r.origin.x || r.corner.y <= r.origin.y )
		return;
	s.rect = r;
	s.width = 16 * words_between(r.origin.x, r.corner.x);
	s.base = screen_addr(r.origin.x, r.origin.y);
	blt.src = &s;
	blt.sp = r.origin;
	blt.dst = &display;
	blt.dr = r;
	blt.op = op;
	blt.pat = texture[patidx];
	bitblt(&blt, 1, 0);
}

/* ------------------------------------------------------------------ */
/* terminal text (Phase 2)                                            */
/* ------------------------------------------------------------------ */

/* Region-clipped fill of an absolute-coordinate rect (current window, gk
 * loaded) to the display base -- the proven blit path. */
srvrect(x0, y0, x1, y1, op)
{
	BLTSTRUCT blt;
	BITMAP s;
	SM_REGION *rp;
	RECT g, dr;

	g.origin.x = x0;  g.origin.y = y0;
	g.corner.x = x1;  g.corner.y = y1;
	if ( g.corner.x <= g.origin.x || g.corner.y <= g.origin.y )
		return;
	blt.op = op;
	blt.pat = texture[gkBpat];
	blt.dst = &display;
	for ( rp = gkLayer->reg; rp < gkLayer->reg + MAX_LRBUF; rp++ )
	{
		if ( rp->flag == L_EMPTY )
			break;
		if ( rp->flag != L_VISIBLE )
			continue;
		dr = R_Intersection(g, rp->bm.rect);
		if ( R_null(dr) )
			continue;
		s.rect = dr;
		s.width = 16 * words_between(dr.origin.x, dr.corner.x);
		s.base = screen_addr(dr.origin.x, dr.origin.y);
		blt.src = &s;
		blt.sp = dr.origin;
		blt.dr = dr;
		bitblt(&blt, 1, 0);
	}
}

/* Draw one glyph `c' of the tail font `fslot' with its cell top-left at (gx,gy),
 * painting only the part inside `dr' (screen coords, already clipped).  ONE asm
 * bitblt shifts each glyph row into place -- never per-pixel (the old kernel
 * CIOGLYPH path was per-pixel and unusably slow).  The .hf fonts store ink=1
 * (white-on-black), so L_NSRC paints black ink on a white cell. */
static
glyph1(fslot, gx, gy, c, dr)
RECT dr;
{
	HRFONT *f;
	BLTSTRUCT blt;
	BITMAP src;
	int gi;

	if ( dr.corner.x <= dr.origin.x || dr.corner.y <= dr.origin.y )
		return;
	f = hr_font(fslot);
	if ( c < f->first || c >= f->first + f->nch )
		return;
	gi = c - f->first;
	src.base = (int *)&f->bits[gi * f->cellh];
	src.width = 16;				/* one word per glyph row */
	src.rect.origin.x = 0;   src.rect.origin.y = 0;
	src.rect.corner.x = f->cellw;  src.rect.corner.y = f->cellh;
	blt.src = &src;
	blt.dst = &display;
	blt.op = L_NSRC;
	blt.pat = texture[0];
	blt.dr = dr;
	blt.sp.x = dr.origin.x - gx;
	blt.sp.y = dr.origin.y - gy;
	bitblt(&blt, 1, 0);
}

/* Draw a NUL-terminated string in tail font `fslot' at absolute pixel top-left
 * (px,ptop), region-clipped to the current window's visible layer regions (gk
 * loaded) and, if srvhrclip is set, to it as well (keeps title text inside the
 * bar).  Advance = the font's cell width. */
srvglyphs(fslot, px, ptop, s)
char *s;
{
	HRFONT *f;
	SM_REGION *rp;
	RECT g, dr;
	int c, cw, ch, clip;

	f = hr_font(fslot);
	cw = f->cellw;  ch = f->cellh;
	clip = ( srvhrclip.corner.x > srvhrclip.origin.x &&
		 srvhrclip.corner.y > srvhrclip.origin.y );
	for ( ; (c = *s & 0xff) != 0; s++, px += cw )
	{
		if ( c < 0x20 || c > 0x7e )
			continue;
		g.origin.x = px;       g.origin.y = ptop;
		g.corner.x = px + cw;  g.corner.y = ptop + ch;
		for ( rp = gkLayer->reg; rp < gkLayer->reg + MAX_LRBUF; rp++ )
		{
			if ( rp->flag == L_EMPTY )
				break;
			if ( rp->flag != L_VISIBLE )
				continue;
			dr = R_Intersection(g, rp->bm.rect);
			if ( clip )
				dr = R_Intersection(dr, srvhrclip);
			if ( R_null(dr) )
				continue;
			glyph1(fslot, px, ptop, c, dr);
		}
	}
}

/* Draw a run of characters at content cell (col,row) of the current window. */
srvtext(col, row, s)
char *s;
{
	srvglyphs(SHM_FTERM,
		  gkCrect.origin.x + col * termcw,
		  gkCrect.origin.y + row * termch, s);
}

/* Draw window wid's title, then invert the whole title-bar interior so the text
 * reads light-on-dark (gk already loaded).  outline() painted the bar white and
 * the black title text is laid over it; the invert turns that into white text on
 * a dark bar, which is far more legible than black-on-white.  The title uses the
 * same hrfont glyphs as the terminal, clipped to the bar so the 25px cell never
 * spills below the 20px strip. */
srvtitle(wid)
{
	RECT r, bar;
	int bx1, ty;

	r = gkLayer->rect;					/* outer (with shadow) */
	bx1 = r.corner.x - WD_SHADOW - 1;			/* inside the frame */
	if ( wins[wid].title[0] )
	{
		bar.origin.x = r.origin.x + 1;
		bar.origin.y = r.origin.y + 1;
		bar.corner.x = bx1;
		bar.corner.y = r.origin.y + WD_TITLEH;
		srvhrclip = bar;				/* clip glyphs to bar */
		ty = r.origin.y + 1 + (WD_TITLEH - 1 - hr_font(SHM_FUI)->cellh) / 2;
		srvglyphs(SHM_FUI, r.origin.x + 5, ty, wins[wid].title);
		srvhrclip.corner.x = srvhrclip.origin.x = 0;	/* clip off */
	}
	srvrect(r.origin.x + 1, r.origin.y + 1, bx1, r.origin.y + WD_TITLEH,
		L_NDST);
}

/* Clear a rectangular block of cells (col,row)..(col+ncol,row+nrow) of the
 * current window to the background, region-clipped, via the display base. */
srverase(col, row, ncol, nrow)
{
	int x0, y0, x1, y1;

	x0 = gkCrect.origin.x + col * termcw;
	y0 = gkCrect.origin.y + row * termch;
	x1 = x0 + ncol * termcw;
	y1 = y0 + nrow * termch;
	if ( x1 > gkCrect.corner.x ) x1 = gkCrect.corner.x;
	if ( y1 > gkCrect.corner.y ) y1 = gkCrect.corner.y;
	srvrect(x0, y0, x1, y1, L_TRUE);
}

/* Blit an external icon file /usr/hr/icons/<name> (the installable .icn format:
 * word0=width, word1=height, then 1bpp rows) at absolute (px,ptop).  Loaded into
 * a stack buffer -- no persistent memory, and the artwork lives on disk so new
 * apps ship their own icon (GUI.md).  Blitted L_NSRC so the file's white-on-black
 * strokes render as black-on-white (a clean button), matching the font path. */
srvicon(px, ptop, name, clip)
char *name;
RECT clip;
{
	int buf[48 * 3 + 4];		/* header + up to 48x48 (3 words/row) */
	char path[64];
	BLTSTRUCT blt;
	BITMAP src;
	int fd, nb, w, h;

	strcpy(path, "/usr/hr/icons/");
	strcat(path, name);
	fd = open(path, 0);
	if ( fd < 0 )
		return;
	nb = read(fd, buf, sizeof(buf));
	close(fd);
	if ( nb < 4 )
		return;
	w = buf[0];  h = buf[1];		/* big-endian words == Z8001 ints */
	if ( w <= 0 || w > 48 || h <= 0 || h > 48 )
		return;
	src.base = &buf[2];
	src.width = 16 * words_between(0, w);
	src.rect.origin.x = 0;   src.rect.origin.y = 0;
	src.rect.corner.x = w;   src.rect.corner.y = h;
	blt.src = &src;
	blt.dst = &display;
	blt.dr.origin.x = px;      blt.dr.origin.y = ptop;
	blt.dr.corner.x = px + w;  blt.dr.corner.y = ptop + h;
	/* clip the blit to the caller's rect (the icon's visible desktop piece) */
	if ( blt.dr.origin.x < clip.origin.x ) blt.dr.origin.x = clip.origin.x;
	if ( blt.dr.origin.y < clip.origin.y ) blt.dr.origin.y = clip.origin.y;
	if ( blt.dr.corner.x > clip.corner.x ) blt.dr.corner.x = clip.corner.x;
	if ( blt.dr.corner.y > clip.corner.y ) blt.dr.corner.y = clip.corner.y;
	if ( blt.dr.corner.x <= blt.dr.origin.x || blt.dr.corner.y <= blt.dr.origin.y )
		return;
	blt.sp.x = blt.dr.origin.x - px;	/* matching source offset */
	blt.sp.y = blt.dr.origin.y - ptop;
	blt.op = L_NSRC;
	blt.pat = texture[0];
	bitblt(&blt, 1, 0);
}

/* Lowest desktop-icon slot not currently occupied by a minimised window. */
iconslot()
{
	int s, w, used;

	for ( s = 0; s < MAX_WINDOWS; s++ )
	{
		used = 0;
		for ( w = 0; w < MAX_WINDOWS; w++ )
			if ( wins[w].used && wins[w].min && wins[w].islot == s )
			{
				used = 1;
				break;
			}
		if ( !used )
			return s;
	}
	return 0;
}

/* The parts of A not covered by B (A minus B): 0..4 sub-rects into out[].
 * If they do not overlap, out[0]=A (one rect). */
static int
rect_minus(A, B, out)
RECT A, B, out[];
{
	int n;
	RECT ix;

	ix.origin.x = A.origin.x > B.origin.x ? A.origin.x : B.origin.x;
	ix.origin.y = A.origin.y > B.origin.y ? A.origin.y : B.origin.y;
	ix.corner.x = A.corner.x < B.corner.x ? A.corner.x : B.corner.x;
	ix.corner.y = A.corner.y < B.corner.y ? A.corner.y : B.corner.y;
	if ( ix.corner.x <= ix.origin.x || ix.corner.y <= ix.origin.y ) {
		out[0] = A;			/* no overlap: A is whole */
		return 1;
	}
	n = 0;
	if ( A.origin.y < ix.origin.y ) {	/* strip above B */
		out[n].origin.x = A.origin.x;   out[n].origin.y = A.origin.y;
		out[n].corner.x = A.corner.x;   out[n].corner.y = ix.origin.y;  n++;
	}
	if ( A.corner.y > ix.corner.y ) {	/* strip below B */
		out[n].origin.x = A.origin.x;   out[n].origin.y = ix.corner.y;
		out[n].corner.x = A.corner.x;   out[n].corner.y = A.corner.y;   n++;
	}
	if ( A.origin.x < ix.origin.x ) {	/* strip left of B (middle band) */
		out[n].origin.x = A.origin.x;   out[n].origin.y = ix.origin.y;
		out[n].corner.x = ix.origin.x;  out[n].corner.y = ix.corner.y;  n++;
	}
	if ( A.corner.x > ix.corner.x ) {	/* strip right of B */
		out[n].origin.x = ix.corner.x;  out[n].origin.y = ix.origin.y;
		out[n].corner.x = A.corner.x;   out[n].corner.y = ix.corner.y;  n++;
	}
	return n;
}

/* Visible sub-rects of `cell' -- the parts not covered by any live window --
 * into out[] (up to max).  This is how an icon (which is NOT a layer) gets
 * clipped to the bare desktop, so a partially-covered icon shows its visible
 * part and a fully-covered one shows nothing. */
#define ICONRB	24
static int
desktop_rects(cell, out, max)
RECT cell, out[];
{
	RECT work[ICONRB], next[ICONRB], pieces[4];
	LAYER *lp;
	int nw, nn, w, i, k, np;

	nw = 0;
	work[nw++] = cell;
	for ( w = 0; w < MAX_WINDOWS && nw; w++ ) {
		if ( !(wins[w].used && !wins[w].min && wtbl[w] &&
		       (lp = wtbl[w]->wn_Layer)) )
			continue;
		nn = 0;
		for ( i = 0; i < nw; i++ ) {
			np = rect_minus(work[i], lp->rect, pieces);
			for ( k = 0; k < np && nn < ICONRB; k++ )
				next[nn++] = pieces[k];
		}
		for ( i = 0; i < nn; i++ )
			work[i] = next[i];
		nw = nn;
	}
	for ( i = 0; i < nw && i < max; i++ )
		out[i] = work[i];
	return (nw < max) ? nw : max;
}

/* Draw (on) or erase (off) a minimised window's desktop icon + label, CLIPPED to
 * the parts of its cell not covered by any window -- icons are not layers, so we
 * clip them to the bare desktop ourselves (a partially covered icon shows its
 * visible piece; a fully covered one shows nothing, and moving a window off it
 * makes it reappear via redraw_icons). */
drawicon(wid, on)
{
	RECT cell, label, vr[ICONRB], lc;
	int x, y, nvr, i;

	x = iconx(wins[wid].islot);
	y = ICONROWY;
	cell.origin.x = x - 4;             cell.origin.y = y - 2;
	cell.corner.x = x + ICONCW - 12;   cell.corner.y = y + ICONW + ICONLH + 2;
	label.origin.x = cell.origin.x;    label.origin.y = y + ICONW;
	label.corner.x = cell.corner.x;    label.corner.y = y + ICONW + ICONLH;
	nvr = desktop_rects(cell, vr, ICONRB);
	gfx_cursor_hide();
	for ( i = 0; i < nvr; i++ ) {
		srvfill(vr[i], 10, L_TRUE);		/* clear this visible piece */
		if ( !on )
			continue;
		srvicon(x, y, wins[wid].icon, vr[i]);	/* app icon, clipped */
		lc = R_Intersection(label, vr[i]);
		if ( !R_null(lc) )
			srvmenuglyphs(SHM_FICON, x, y + ICONW, wins[wid].title, lc);
	}
	gfx_cursor_show();
}

/* Repaint every minimised window's icon.  The engine never repaints icons (they
 * are not layers), so a window moving off one would leave the desktop bare
 * there; call this after any op that can change bottom-of-screen coverage.  Each
 * icon is clipped to the bare desktop by drawicon, so covered parts are left to
 * the window on top. */
redraw_icons()
{
	int w;

	srvlock();
	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used && wins[w].min )
			drawicon(w, 1);
	srvunlock();
}

/* Rebuild window `wid's layer at rectangle r (used to restore a minimised
 * window; the WSTRUCT and its graphics state are preserved).  wtbl[wid] is
 * temporarily removed so make_vis_list() (run inside newlayer) never touches
 * this window's not-yet-existent layer. */
relayout(wid, r)
RECT r;
{
	WSTRUCT *wp;

	wp = wtbl[wid];
	gk = *wp;		/* struct copy through a pointer variable -- safe
				 * now that z8001-coherent-cc restores the source
				 * pointer after ldirb (the compiler used to leave wp
				 * advanced by sizeof(WSTRUCT)). */
	wtbl[wid] = (WSTRUCT *)NULL;
	gkWid = wid;
	gkCrect = r;
	gkPsize.x = r.corner.x - r.origin.x;
	gkPsize.y = r.corner.y - r.origin.y;
	gkFlags = WT_FULLY_VIS;
	gkDp = r.origin;
	gkLayer = newlayer(r);
	gkLayer->base = screen_addr(r.origin.x, r.origin.y);
	gkLayer->width = 1024;
	wtbl[wid] = wp;
	*wtbl[wid] = gk;
	outline(wid);
	srvtitle(wid);
	gkCrect.origin.x = r.origin.x + WD_BORDER;
	gkCrect.origin.y = r.origin.y + WD_TITLEH;
	gkCrect.corner.x = r.corner.x - WD_SHADOW - WD_BORDER;
	gkCrect.corner.y = r.corner.y - WD_SHADOW - WD_BORDER;
	*wtbl[wid] = gk;
}

minwin(wid)
{
	if ( !wins[wid].used || wins[wid].min || !wtbl[wid] )
		return;
	srvlock();
	srvlogn("minwin ", wid);
	LOADW(wid);
	wins[wid].sx = gkLayer->rect.origin.x;
	wins[wid].sy = gkLayer->rect.origin.y;
	wins[wid].sw = gkLayer->rect.corner.x - gkLayer->rect.origin.x;
	wins[wid].sh = gkLayer->rect.corner.y - gkLayer->rect.origin.y;
	gfx_cursor_hide();
	dellayer(gkLayer);		/* unhook + list windows it uncovers */
	perform_update();		/* repaint them */
	free((char *)gk.wn_Layer);
	/* Remove the window from wtbl entirely while minimised, so nothing
	 * (make_vis_list, lp2id, ...) dereferences its freed layer.  Keep the
	 * WSTRUCT so its graphics state survives. */
	gk.wn_Layer = (LAYER *)NULL;
	*wtbl[wid] = gk;
	wins[wid].wp = wtbl[wid];
	wtbl[wid] = (WSTRUCT *)NULL;
	wins[wid].islot = iconslot();
	wins[wid].min = 1;
	drawicon(wid, 1);			/* clips itself to the bare desktop */
	gfx_cursor_show();
	publish_all();				/* this window unmapped; others uncovered */
	srvunlock();

	/* Explicitly repaint every window the hidden one was covering.  minwin is
	 * the one op that otherwise relies purely on perform_update's reply hook to
	 * expose uncovered windows, and that engine path does not reliably deliver
	 * the E_EXPOSE (a covered clock came back with only its hands, no face). */
	expose_covered(wid, wins[wid].sx, wins[wid].sy,
		       wins[wid].sx + wins[wid].sw, wins[wid].sy + wins[wid].sh);
	/* Minimising this window may have uncovered OTHER windows' icons. */
	redraw_icons();
}

restorewin(wid)
{
	RECT r;

	if ( !wins[wid].used || !wins[wid].min )
		return;
	srvlock();
	gfx_cursor_hide();
	drawicon(wid, 0);
	wtbl[wid] = wins[wid].wp;	/* put the WSTRUCT back into the table */
	r.origin.x = wins[wid].sx;  r.origin.y = wins[wid].sy;
	r.corner.x = wins[wid].sx + wins[wid].sw;
	r.corner.y = wins[wid].sy + wins[wid].sh;
	relayout(wid, r);
	wins[wid].min = 0;
	gfx_cursor_show();
	publish_all();
	srvunlock();
	sendev(wid, E_EXPOSE, 0, 0, wtbl[wid]->wn_Psize.x, wtbl[wid]->wn_Psize.y);
	redraw_icons();			/* the freed icon slot / new window coverage */
}

/* Redraw a window's decoration (frame + stepped shadow + title bar).  Needed
 * after a resize/move: new_dimensions only re-outlines a window that gained
 * newly-VISIBLE regions, so a SHRINK (which gains none) would otherwise leave the
 * old shadow/frame stale on whatever was uncovered beneath it (bug: shadow not
 * redrawn over the revealed window). */
redecorate(wid)
{
	if ( !wtbl[wid] || !wtbl[wid]->wn_Layer )
		return;
	srvlock();
	LOADW(wid);
	gfx_cursor_hide();
	outline(wid);
	srvtitle(wid);
	gfx_cursor_show();
	srvunlock();
}

/* Move window wid so its outer rect origin is (nx,ny), same size. */
movewin(wid, nx, ny)
{
	RECT r;
	int w, h;

	if ( !wtbl[wid] )
		return;
	srvlock();
	w = wtbl[wid]->wn_Layer->rect.corner.x - wtbl[wid]->wn_Layer->rect.origin.x;
	h = wtbl[wid]->wn_Layer->rect.corner.y - wtbl[wid]->wn_Layer->rect.origin.y;
	if ( nx < 0 ) nx = 0;
	if ( ny < 0 ) ny = 0;
	if ( nx + w > XMAX ) nx = XMAX - w;
	if ( ny + h > YMAX ) ny = YMAX - h;
	r.origin.x = nx;  r.origin.y = ny;
	r.corner.x = nx + w;  r.corner.y = ny + h;
	LOADW(wid);
	gfx_cursor_hide();
	new_dimensions(r);		/* moves, exposes uncovered, saves gk */
	redecorate(wid);		/* ensure frame+shadow+title at new spot */
	gfx_cursor_show();
	publish_all();
	srvunlock();
	sendev(wid, E_EXPOSE, 0, 0, wtbl[wid]->wn_Psize.x, wtbl[wid]->wn_Psize.y);
	redraw_icons();			/* moving off an icon must repaint it */
}

/* Resize window wid so its outer corner is (cx,cy). */
resizewin(wid, cx, cy)
{
	RECT r;
	int nw, nh;

	if ( !wtbl[wid] )
		return;
	srvlock();
	r.origin = wtbl[wid]->wn_Layer->rect.origin;
	if ( cx < r.origin.x + 48 ) cx = r.origin.x + 48;
	if ( cy < r.origin.y + 48 ) cy = r.origin.y + 48;
	if ( cx > XMAX ) cx = XMAX;
	if ( cy > YMAX ) cy = YMAX;
	r.corner.x = cx;  r.corner.y = cy;
	LOADW(wid);
	gfx_cursor_hide();
	new_dimensions(r);
	redecorate(wid);		/* shrink gains no new-visible region -> */
					/* redraw frame+shadow+title explicitly */
	gfx_cursor_show();
	nw = wtbl[wid]->wn_Psize.x - WD_SHADOW - 2 * WD_BORDER;	/* content size */
	nh = wtbl[wid]->wn_Psize.y - WD_TITLEH - WD_SHADOW - WD_BORDER;
	publish_all();
	srvunlock();
	sendev(wid, E_RESIZE, nw, nh, 0, 0);
	redraw_icons();			/* resizing off an icon must repaint it */
}

/* ------------------------------------------------------------------ */
/* application launcher (config-driven)                               */
/* ------------------------------------------------------------------ */

/* Read /usr/hr/etc/apps into apps[].  One line per app:
 * name:iconpath:execpath:contentW:contentH  ('#'/blank lines skipped). */
loadapps()
{
	int fd, nb;
	char buf[1024];
	char *p, *e;

	napps = 0;
	fd = open("/usr/hr/etc/apps", 0);
	if ( fd < 0 )
		return;
	nb = read(fd, buf, sizeof(buf) - 1);
	close(fd);
	if ( nb <= 0 )
		return;
	buf[nb] = 0;
	p = buf;
	while ( *p && napps < MAX_APPS )
	{
		char *fld[6];
		int nf;

		e = p;
		while ( *e && *e != '\n' )
			e++;
		if ( *e )
			*e++ = 0;
		if ( *p && *p != '#' )
		{
			char *f;

			nf = 0;
			fld[nf++] = p;
			f = p;
			while ( nf < 6 )
			{
				while ( *f && *f != ':' )
					f++;
				if ( !*f )
					break;
				*f++ = 0;
				fld[nf++] = f;
			}
			if ( nf >= 3 )
			{
				struct app *a;

				a = &apps[napps++];
				strncpy(a->name, fld[0], sizeof(a->name) - 1);
				strncpy(a->icon, fld[1], sizeof(a->icon) - 1);
				strncpy(a->path, fld[2], sizeof(a->path) - 1);
				a->w = (nf > 3) ? atoi(fld[3]) : 300;
				a->h = (nf > 4) ? atoi(fld[4]) : 200;
				a->multi = (nf > 5) ? atoi(fld[5]) : 0;
			}
		}
		p = e;
	}
}

/* Set window wid's displayed title from its base: plain when it is the only
 * window of that app, "base #inst" when two or more are open (the one kept
 * modern touch). */
setwintitle(wid)
{
	int w, cnt;

	cnt = 0;
	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used && !strcmp(wins[w].base, wins[wid].base) )
			cnt++;
	if ( cnt >= 2 )
		sprintf(wins[wid].title, "%s #%d", wins[wid].base, wins[wid].inst);
	else
		strcpy(wins[wid].title, wins[wid].base);
}

/* Recompute + repaint the titles of every live window sharing `base' (call after
 * a launch or a close, so #N appears/vanishes as the count crosses two). */
relabel(base)
char *base;
{
	int w;

	srvlock();
	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used && !strcmp(wins[w].base, base) )
		{
			setwintitle(w);
			if ( wins[w].min )
				drawicon(w, 1);			/* redraw icon label */
			else if ( wtbl[w] )
			{
				gk = *wtbl[w];			/* repaint decoration */
				gfx_cursor_hide();
				outline(w);
				srvtitle(w);
				gfx_cursor_show();
			}
		}
	srvunlock();
}

/* Launch apps[ai] in a new window at (x,y).  Standardized argv (GUI.md):
 * <path> wid contentW contentH cellW cellH.  Returns wid or -1. */
launchapp(ai, x, y)
{
	int wid, ev[2], pid;
	RECT r;
	struct app *a;
	char idbuf[8], wbuf[8], hbuf[8], cwbuf[8], chbuf[8];

	if ( ai < 0 || ai >= napps )
		return -1;
	a = &apps[ai];
	for ( wid = 0; wid < MAX_WINDOWS; wid++ )
		if ( !wins[wid].used && wtbl[wid] == (WSTRUCT *)NULL )
			break;
	if ( wid == MAX_WINDOWS )
		return -1;

	{				/* clamp the whole window on-screen */
		int ww, hh;
		ww = a->w + 2 * WD_BORDER + WD_SHADOW;
		hh = a->h + WD_TITLEH + WD_BORDER + WD_SHADOW;
		if ( x + ww > XMAX ) x = XMAX - ww;
		if ( y + hh > YMAX ) y = YMAX - hh;
		if ( x < 0 ) x = 0;
		if ( y < 0 ) y = 0;
		r.origin.x = x;       r.origin.y = y;
		r.corner.x = x + ww;  r.corner.y = y + hh;
	}
	strcpy(wins[wid].title, a->name);
	strncpy(wins[wid].base, a->name, sizeof(wins[wid].base) - 1);
	strncpy(wins[wid].icon, a->icon, sizeof(wins[wid].icon) - 1);
	wins[wid].used = 1;		/* mark used BEFORE mkwin so its publish_all
					 * marks the descriptor mapped=1 before we fork
					 * the client (else the client races a mapped=0
					 * descriptor and its first paint is dropped). */
	mkwin(wid, r);

	if ( pipe(ev) < 0 )
		return -1;
	sprintf(idbuf, "%d", wid);
	sprintf(wbuf, "%d", gkCrect.corner.x - gkCrect.origin.x);
	sprintf(hbuf, "%d", gkCrect.corner.y - gkCrect.origin.y);
	sprintf(cwbuf, "%d", termcw);
	sprintf(chbuf, "%d", termch);

	pid = fork();
	if ( pid == 0 )
	{
		dup2(ev[0], HR_EVFD);
		dup2(cmdwr, HR_CMDFD);
		{ int f; for ( f = 5; f < 20; f++ ) close(f); }
		execl(a->path, a->name, idbuf, wbuf, hbuf, cwbuf, chbuf, (char *)0);
		_exit(1);
	}
	close(ev[0]);
	wins[wid].used = 1;
	wins[wid].pid = pid;
	wins[wid].evfd = ev[1];
	nwins++;
	focuswid = wid;
	/* instance number = 1 + highest among the live windows of this app, then
	 * relabel so #N appears on all of them once there are two or more. */
	{
		int w, hi;
		hi = 0;
		for ( w = 0; w < MAX_WINDOWS; w++ )
			if ( w != wid && wins[w].used &&
			     !strcmp(wins[w].base, a->name) && wins[w].inst > hi )
				hi = wins[w].inst;
		wins[wid].inst = hi + 1;
	}
	relabel(a->name);
	return wid;
}

/* ------------------------------------------------------------------ */
/* pop-up menus (desktop launcher + per-window ops)                   */
/* ------------------------------------------------------------------ */

/* Draw a NUL-terminated string in tail font `fslot' at absolute (px,ptop),
 * clipped to the single rect `clip' (no layer walk): desktop chrome sits above
 * every window and the background under it is saved/restored. */
srvmenuglyphs(fslot, px, ptop, s, clip)
char *s;
RECT clip;
{
	HRFONT *f;
	RECT g, dr;
	int c, cw, ch;

	f = hr_font(fslot);
	cw = f->cellw;  ch = f->cellh;
	for ( ; (c = *s & 0xff) != 0; s++, px += cw )
	{
		if ( c < 0x20 || c > 0x7e )
			continue;
		g.origin.x = px;       g.origin.y = ptop;
		g.corner.x = px + cw;  g.corner.y = ptop + ch;
		dr = R_Intersection(g, clip);
		if ( R_null(dr) )
			continue;
		glyph1(fslot, px, ptop, c, dr);
	}
}

/* Top margin (px) above the first item: the menu pops with the pointer sitting
 * in this margin, so a press-release with no drag lands on NO item (dismiss) --
 * faithful to the original's UMARGIN + mu_FindItem "above content top" guard. */
#define MNU_TOP 7

/* An items[] entry equal to MNU_DIV renders as a non-selectable divider line
 * instead of text (e.g. between the app list and Quit in the desktop menu). */
#define MNU_DIV ((char *)0)

/* Which item (0..n-1) the pointer (mx,my) is over, or -1 if outside the box or
 * still in the top margin (nothing selected yet). */
static
mnu_item(box, itemh, n)
RECT box;
{
	int it;

	if ( mx < box.origin.x || mx >= box.corner.x ||
	     my < box.origin.y || my >= box.corner.y )
		return -1;
	if ( my < box.origin.y + MNU_TOP )
		return -1;			/* in the top margin: no item */
	it = (my - box.origin.y - MNU_TOP) / itemh;
	if ( it >= n )
		return -1;			/* in the bottom margin: no item */
	return it;
}

/* XOR-invert item i's row (self-erasing highlight, exactly like the original
 * mu_InvertItem). */
static
mnu_invert(box, itemh, i)
RECT box;
{
	RECT e;

	e.origin.x = box.origin.x + 1;
	e.corner.x = box.corner.x - 1;
	e.origin.y = box.origin.y + MNU_TOP + i * itemh;
	e.corner.y = e.origin.y + itemh;
	if ( e.corner.y > box.corner.y - 1 )
		e.corner.y = box.corner.y - 1;
	srvfill(e, 0, L_NDST);
}

/* Word copy (VRAM<->heap) for menu background save/restore. */
static
mnu_wcpy(s, d, n)
int *s, *d;
{
	while ( n-- > 0 )
		*d++ = *s++;
}

/* Pop up a menu of n text items at (mx0,my0); track the pointer in a nested
 * command-pipe loop (client draws are frozen meanwhile); return the selected
 * index or -1.  The pixels under the box are saved and restored, so whatever was
 * there -- desktop or a window -- reappears intact. */
srvmenu(items, n, mx0, my0)
char *items[];
{
	RECT box;
	int i, w, itemh, boxw, boxh, sel, last, it, nb, yy, wpl, rows;
	int *buf;
	WMSG c;

	itemh = hr_font(SHM_FUI)->cellh;
	w = 0;
	for ( i = 0; i < n; i++ )
	{
		int lw;
		if ( items[i] == MNU_DIV )
			continue;
		lw = strlen(items[i]) * hr_font(SHM_FUI)->cellw;
		if ( lw > w ) w = lw;
	}
	boxw = w + 16;
	boxh = MNU_TOP + n * itemh + MNU_TOP;	/* equal top + bottom margins */
	box.origin.x = mx0 & ~0x0f;			/* word-align for row copy */
	box.origin.y = my0;
	if ( box.origin.x + boxw > XMAX ) box.origin.x = (XMAX - boxw) & ~0x0f;
	if ( box.origin.y + boxh > YMAX ) box.origin.y = YMAX - boxh;
	if ( box.origin.x < 0 ) box.origin.x = 0;
	if ( box.origin.y < 0 ) box.origin.y = 0;
	box.corner.x = box.origin.x + boxw;
	box.corner.y = box.origin.y + boxh;

	rows = box.corner.y - box.origin.y;
	wpl = words_between(box.origin.x, box.corner.x);
	buf = (int *)malloc(wpl * rows * 2);
	/* Freeze direct-render clients for the menu's whole lifetime: the menu is a
	 * transient overlay, NOT a layer, so a client's clip can't exclude it and it
	 * would otherwise paint over it (the flood-covers-the-menu bug).  Set the flag
	 * BEFORE taking the lock so a client that checks it while holding the lock sees
	 * it; take the lock around the save+draw so any in-flight client primitive
	 * finishes first and the menu lands on top of it. */
	hr_glob()->overlay = 1;
	srvlock();
	/* Hide the cursor BEFORE saving the pixels under the menu: otherwise the XOR
	 * cursor (if it overlaps the box) is captured into the save buffer and
	 * painted back on restore, leaving a stray arrow where the menu was. */
	gfx_cursor_hide();
	if ( buf )
		for ( yy = 0; yy < rows; yy++ )
			mnu_wcpy(screen_addr(box.origin.x, box.origin.y + yy),
				 buf + yy * wpl, wpl);

	srvfill(box, 0, L_TRUE);			/* white body */
	{
		RECT e;
		e = box; e.corner.y = box.origin.y + 1;   srvfill(e, 0, L_FALSE);
		e = box; e.origin.y = box.corner.y - 1;   srvfill(e, 0, L_FALSE);
		e = box; e.corner.x = box.origin.x + 1;   srvfill(e, 0, L_FALSE);
		e = box; e.origin.x = box.corner.x - 1;   srvfill(e, 0, L_FALSE);
	}
	for ( i = 0; i < n; i++ )
		if ( items[i] == MNU_DIV )
		{
			RECT e;				/* black hairline across the row */
			e.origin.x = box.origin.x + 2;
			e.corner.x = box.corner.x - 2;
			e.origin.y = box.origin.y + MNU_TOP + i * itemh + itemh / 2;
			e.corner.y = e.origin.y + 1;
			srvfill(e, 0, L_FALSE);
		}
		else
			srvmenuglyphs(SHM_FUI, box.origin.x + 8,
				      box.origin.y + MNU_TOP + i * itemh, items[i], box);
	gfx_cursor_show();
	srvunlock();			/* menu is painted; clients stay frozen via overlay */

	/* menu cursor (right-pointing arrow) while the menu is up, like the
	 * original desktop (which sets MNU_MOUSE around MU_Start/MU_End). */
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, MNU_MOUSE);

	/* Faithful to the original desktop (dmenu upRwait/MU_End): press-hold to
	 * reveal, drag to highlight, and RELEASE (mouse up) selects the highlighted
	 * item and dismisses.  The button that opened the menu is still held (this
	 * runs from handlebtn on that press).  The menu opens with the pointer in the
	 * top margin (above item 0, see mnu_item), so a plain click-and-release with
	 * no drag selects nothing -- exactly like the original -- instead of firing
	 * item 0. */
	sel = -1;  last = -1;
	for (;;)
	{
		nb = read(cmdfd, &c, sizeof(c));
		if ( nb != sizeof(c) )
			continue;
		if ( c.wm_type != C_INPUT || c.wm_arg[0] == IN_KEY )
			continue;			/* freeze clients + ignore keys */
		mx = c.wm_arg[1];
		my = c.wm_arg[2];
		SM_Mouse_Pos.x = mx;  SM_Mouse_Pos.y = my;
		if ( c.wm_arg[0] == IN_BUTTON &&
		     c.wm_arg[4] && !(c.wm_arg[4] & c.wm_arg[3]) )
		{
			sel = mnu_item(box, itemh, n);	/* button release commits */
			if ( sel >= 0 && items[sel] == MNU_DIV )
				sel = -1;		/* a divider selects nothing */
			break;
		}
		it = mnu_item(box, itemh, n);
		if ( it >= 0 && items[it] == MNU_DIV )
			it = -1;			/* dividers don't highlight */
		if ( it != last )
		{
			gfx_cursor_hide();
			if ( last >= 0 ) mnu_invert(box, itemh, last);
			if ( it >= 0 ) mnu_invert(box, itemh, it);
			gfx_cursor_show();
			last = it;
		}
	}

	srvlock();
	gfx_cursor_hide();
	if ( buf )
	{
		for ( yy = 0; yy < rows; yy++ )
			mnu_wcpy(buf + yy * wpl,
				 screen_addr(box.origin.x, box.origin.y + yy), wpl);
		free((char *)buf);
	}
	else
		background(box);	/* malloc failed: no saved pixels, so repaint the */
					/* desktop where the menu was, then re-expose windows */
	gfx_cursor_show();
	srvunlock();
	hr_glob()->overlay = 0;		/* clients may draw again (they full-repaint) */
	/* If we couldn't save/restore (low memory), the menu box was left on screen
	 * -- exactly the "menu never goes away" bug.  Having painted the desktop back
	 * over it above, now ask any windows it overlapped to repaint (outside the
	 * lock: sendev may block).  This guarantees the menu is dismissed. */
	if ( !buf )
		expose_covered(-1, box.origin.x, box.origin.y,
			       box.corner.x, box.corner.y);
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, DEF_MOUSE);	/* restore arrow */
	return sel;
}

/* ------------------------------------------------------------------ */
/* ghost-frame move / resize (window-menu Move / Stretch)             */
/* ------------------------------------------------------------------ */

/* Draw a 1px XOR outline of rect g (self-erasing: a second call restores the
 * pixels).  The side bars skip the corner rows so the shared corners aren't
 * inverted twice (which would leave holes). */
ghostframe(g)
RECT g;
{
	RECT e;

	e = g; e.corner.y = g.origin.y + 1;                       srvfill(e, 0, L_NDST);
	e = g; e.origin.y = g.corner.y - 1;                       srvfill(e, 0, L_NDST);
	e = g; e.origin.y = g.origin.y + 1; e.corner.y = g.corner.y - 1;
	       e.corner.x = g.origin.x + 1;                       srvfill(e, 0, L_NDST);
	e = g; e.origin.y = g.origin.y + 1; e.corner.y = g.corner.y - 1;
	       e.origin.x = g.corner.x - 1;                       srvfill(e, 0, L_NDST);
}

/* Interactively move (mode 1) or resize (mode 2) window wid with an XOR ghost
 * frame -- faithful to the original desktop (main.c fmove/fstretch: dnLwait then
 * upLwait(ghost)).  After the menu picks Move/Stretch the pointer button is up;
 * press the LEFT button to grab, drag the ghost while HELD, and release to
 * commit.  A RIGHT/MIDDLE press instead cancels.  Resize drags the bottom-right
 * corner. */
ghostdrag(wid, mode)
{
	RECT r, g;
	int w, h, gx, gy, nb;
	WMSG c;

	if ( !wtbl[wid] || !wtbl[wid]->wn_Layer )
		return;
	r = wtbl[wid]->wn_Layer->rect;
	g = r;
	w = r.corner.x - r.origin.x;
	h = r.corner.y - r.origin.y;

	/* Switch to the move/resize "hand" cursor the moment the item is chosen
	 * (original fmove sets MOV_MOUSE before waiting for the grab), so the shape
	 * itself signals the mode and the pending left-button grab. */
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, MOV_MOUSE);

	/* Wait for the grab: LEFT down grabs, RIGHT/MIDDLE down cancels (dnLwait). */
	for (;;)
	{
		nb = read(cmdfd, &c, sizeof(c));
		if ( nb != sizeof(c) || c.wm_type != C_INPUT || c.wm_arg[0] == IN_KEY )
			continue;
		mx = c.wm_arg[1];  my = c.wm_arg[2];
		SM_Mouse_Pos.x = mx;  SM_Mouse_Pos.y = my;
		if ( c.wm_arg[0] == IN_BUTTON && (c.wm_arg[4] & c.wm_arg[3]) )
		{
			if ( c.wm_arg[3] & 0x8000 )		/* LEFT down: grab */
				break;
			if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, DEF_MOUSE);
			return;					/* RIGHT/MIDDLE: cancel */
		}
	}
	gx = mx - r.origin.x;			/* grab offset within the window */
	gy = my - r.origin.y;

	/* The cursor MUST stay enabled during tracking: the driver's mouse poll
	 * (hrmouse) returns early while the cursor is hidden, so hiding it for the
	 * whole drag would starve us of the very motion events we track.  Instead
	 * bracket ONLY the XOR ghost blits with hide/show (ref-counted in the driver
	 * now, so this composes cleanly and no longer burns a stray arrow), leaving
	 * the cursor shown between iterations so motion keeps flowing. */
	gfx_cursor_hide();  ghostframe(g);  gfx_cursor_show();

	/* Drag while the button is held; release (mouse up) commits (upLwait). */
	for (;;)
	{
		nb = read(cmdfd, &c, sizeof(c));
		if ( nb != sizeof(c) || c.wm_type != C_INPUT || c.wm_arg[0] == IN_KEY )
			continue;
		mx = c.wm_arg[1];  my = c.wm_arg[2];
		SM_Mouse_Pos.x = mx;  SM_Mouse_Pos.y = my;
		if ( c.wm_arg[0] == IN_BUTTON &&
		     c.wm_arg[4] && !(c.wm_arg[4] & c.wm_arg[3]) )
		{
			gfx_cursor_hide();  ghostframe(g);  gfx_cursor_show();
			break;
		}
		gfx_cursor_hide();
		ghostframe(g);					/* erase old */
		if ( mode == 1 )				/* move: translate */
		{
			int nx, ny;
			nx = mx - gx;  ny = my - gy;
			if ( nx < 0 ) nx = 0;
			if ( ny < 0 ) ny = 0;
			if ( nx + w > XMAX ) nx = XMAX - w;
			if ( ny + h > YMAX ) ny = YMAX - h;
			g.origin.x = nx;      g.origin.y = ny;
			g.corner.x = nx + w;  g.corner.y = ny + h;
		}
		else						/* resize: bottom-right corner */
		{
			int cx, cy;
			cx = mx;  cy = my;
			if ( cx < r.origin.x + 48 ) cx = r.origin.x + 48;
			if ( cy < r.origin.y + 48 ) cy = r.origin.y + 48;
			if ( cx > XMAX ) cx = XMAX;
			if ( cy > YMAX ) cy = YMAX;
			g.corner.x = cx;  g.corner.y = cy;
		}
		ghostframe(g);					/* draw new */
		gfx_cursor_show();
	}

	if ( mode == 1 ) movewin(wid, g.origin.x, g.origin.y);
	else             resizewin(wid, g.corner.x, g.corner.y);
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, DEF_MOUSE);	/* restore arrow */
}

/* Quit the whole window manager (desktop-menu "Quit"): tell every client to
 * exit and kill it (so it releases /dev/dmgr), stop the input pump, reap all
 * children, then unload the hi-res driver -- which restores the kernel text
 * console -- and return to the shell that launched us. */
quitwm()
{
	int w, pid, st;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used )
		{
			sendev(w, E_QUIT, 0, 0, 0, 0);
			if ( wins[w].pid > 0 )
				kill(wins[w].pid, SIGKILL);
			close(wins[w].evfd);
			wins[w].used = 0;
		}
	if ( curfd >= 0 )
		ioctl(curfd, CIOMSEOFF, (char *)0);	/* erase the XOR cursor */
	if ( pumppid > 0 )
		kill(pumppid, SIGKILL);
	/* Clear the screen to black (the text console's background) while the
	 * graphics driver is still loaded, so the restored console isn't left with
	 * the desktop image behind its prompt. */
	{
		RECT full;
		full.origin.x = XMIN;  full.origin.y = YMIN;
		full.corner.x = XMAX;  full.corner.y = YMAX;
		srvfill(full, 0, L_FALSE);
	}
	if ( curfd >= 0 )
	{
		close(curfd);			/* release the server's /dev/dmgr */
		curfd = -1;
	}
	while ( wait(&st) >= 0 )		/* reap all children -> driver fds freed */
		;
	pid = fork();			/* uload restores the text console */
	if ( pid == 0 )
	{
		execl("/etc/uload", "uload", "/drv/hr", (char *)0);
		_exit(1);
	}
	while ( wait(&st) != pid )
		;
	exit(0);
}

/* Find a live window belonging to app `ai' (matched by base name), or -1. */
appwindow(ai)
{
	int w;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used && !strcmp(wins[w].base, apps[ai].name) )
			return w;
	return -1;
}

/* Right-click on the empty desktop: menu of launchable apps, a divider, then
 * "Quit" (quit the whole window manager).  Multi-instance apps read "New <name>
 * ...", single-instance apps read "<name>...". */
deskmenu(x, y)
{
	char labels[MAX_APPS][24];
	char *items[MAX_APPS + 2];
	int i, n, qidx, sel;

	n = 0;
	for ( i = 0; i < napps; i++ )
	{
		if ( apps[i].multi )
			sprintf(labels[i], "New %s...", apps[i].name);
		else
			sprintf(labels[i], "%s...", apps[i].name);
		items[n++] = labels[i];
	}
	items[n++] = MNU_DIV;			/* divider before Quit */
	qidx = n;
	items[n++] = "Quit";
	sel = srvmenu(items, n, x, y);
	if ( sel < 0 )
		return;
	if ( sel == qidx )
		quitwm();
	else if ( sel < napps )
	{
		/* single-instance app already open? bring it forward instead of
		 * launching a duplicate. */
		int w;
		if ( !apps[sel].multi && (w = appwindow(sel)) >= 0 )
		{
			if ( wins[w].min )
				restorewin(w);
			else
				raisewin(w);
		}
		else
			launchapp(sel, x, y);
	}
}

/* Right-click on a window: per-window operations.  "Front"/"Back" raise/lower;
 * "Quit" closes that window (vs. the desktop menu's Quit = quit the WM). */
char	*g_winitems[] = { "Move", "Stretch", "Front", "Back", "Hide", "Quit" };
winmenu(w, x, y)
{
	int sel;

	sel = srvmenu(g_winitems, 6, x, y);
	if ( sel == 0 )      ghostdrag(w, 1);
	else if ( sel == 1 ) ghostdrag(w, 2);
	else if ( sel == 2 ) raisewin(w);
	else if ( sel == 3 ) backwin(w);
	else if ( sel == 4 ) minwin(w);
	else if ( sel == 5 ) killwin(w);
}

/* A pointer button was pressed.  Right = pop up the desktop (launcher) or window
 * menu; left = click-to-raise.  A click on a dock icon restores that window
 * (temporary until the desktop-icon phase). */
handlebtn(c)
WMSG *c;
{
	int down, changed, w;
	POINT p;

	changed = c->wm_arg[4];
	down = c->wm_arg[3];		/* buttons currently held */
	mx = c->wm_arg[1];
	my = c->wm_arg[2];
	SM_Mouse_Pos.x = mx;		/* keep bitblt cursor-hide gating current */
	SM_Mouse_Pos.y = my;
	hr_glob()->curx = mx;		/* clients read this for cursor-overlap */
	hr_glob()->cury = my;
	p.x = mx;  p.y = my;

	if ( !(changed & down) )		/* act only on a fresh press */
		return;

	/* a minimised-window desktop icon? restore it. */
	if ( my >= ICONROWY - 2 && my < ICONROWY + ICONW + ICONLH )
	{
		for ( w = 0; w < MAX_WINDOWS; w++ )
			if ( wins[w].used && wins[w].min &&
			     mx >= iconx(wins[w].islot) - 4 &&
			     mx < iconx(wins[w].islot) + ICONW + 8 )
			{
				restorewin(w);
				return;
			}
	}

	if ( changed & down & 0x2000 )		/* RIGHT: menus */
	{
		w = who_top_at(p);
		if ( w >= 0 && w < MAX_WINDOWS && wins[w].used )
			winmenu(w, mx, my);
		else
			deskmenu(mx, my);
		return;
	}
	if ( changed & down & 0x8000 )		/* LEFT: click-to-raise */
	{
		w = who_top_at(p);
		if ( w >= 0 && w < MAX_WINDOWS && wins[w].used )
			raisewin(w);
	}
}

/* ------------------------------------------------------------------ */
/* command dispatch                                                   */
/* ------------------------------------------------------------------ */

docmd(c)
WMSG *c;
{
	int wid;

	wid = c->wm_wid;
	if ( c->wm_type == C_INPUT )
	{
		if ( c->wm_arg[0] == IN_MOVE )
		{
			mx = c->wm_arg[1];
			my = c->wm_arg[2];
			hr_glob()->curx = mx;	/* clients read this for overlap */
			hr_glob()->cury = my;
			SM_Mouse_Pos.x = mx;	/* bitblt cursor-hide gating (bug #1) */
			SM_Mouse_Pos.y = my;
		}
		else if ( c->wm_arg[0] == IN_BUTTON )
			handlebtn(c);
		else if ( c->wm_arg[0] == IN_KEY )
		{
			/* route the keystroke to the focused window's client */
			if ( focuswid >= 0 && focuswid < MAX_WINDOWS &&
			     wins[focuswid].used )
				sendev(focuswid, E_KEY, c->wm_arg[1], 0, 0, 0);
		}
		return;
	}
	/* C_BYE reaps the window (works even while minimised).  All per-primitive
	 * draw ops (C_MOVE/LINE/POINT/TEXT/ERASE/CLRCLIP/SETLOGOP) are RETIRED:
	 * clients direct-render their content straight to VRAM via clgfx (GUI.md
	 * Model A), so no pixels cross IPC.  C_FLUSH and anything else are no-ops. */
	if ( c->wm_type == C_BYE )
	{
		srvlogn("C_BYE wid ", wid);
		killwin(wid);
	}
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

reset_screen()
{
	int i;

	display.base = SEG0;
	display.rect.origin.x = XMIN;
	display.rect.origin.y = YMIN;
	display.rect.corner.x = XMAX;
	display.rect.corner.y = YMAX;
	display.width = DIS_WIDTH;
	for ( i = 0; i < MAX_WINDOWS; i++ )
		wtbl[i] = (WSTRUCT *)NULL;
	DM_frontmost = DM_rearmost = (LAYER *)NULL;
	background(display.rect);
}

/* Copy an .hf font file into its slot in the shared VRAM tail (shmem.h).  Read
 * in chunks into a stack buffer (the heap is tiny) and store the bytes verbatim
 * to the tail via its far pointer -- the tail is real, user-accessible RAM, so a
 * plain byte copy works (same segment the server already draws into).  After
 * this the server AND every client blit glyphs from the one copy. */
loadfont(off, path)
char *path;
{
	char buf[512];
	char *d;
	int fd, n, i;

	fd = open(path, 0);
	if ( fd < 0 )
	{
		srvlogs("loadfont: open failed\n");
		return -1;
	}
	d = HRTAIL + off;
	while ( (n = read(fd, buf, sizeof(buf))) > 0 )
		for ( i = 0; i < n; i++ )
			*d++ = buf[i];
	close(fd);
	return 0;
}

/* A client that died leaves a broken event pipe; without this, the server's next
 * sendev() write() would raise SIGPIPE and kill the whole GUI.  Survive + log. */
onpipe()
{
	srvlogs("SIGPIPE (dead client evpipe)\n");
	signal(SIGPIPE, onpipe);
}

main(argc, argv)
char **argv;
{
	int cp[2], n;
	WMSG c;

	gfx_reply_hook = onreply;
	if ( trace )
		srvlog = creat("/wslog", 0644);	/* debug log, extract with disk.py */
	signal(SIGPIPE, onpipe);		/* survive a client's broken pipe */

	if ( pipe(cp) < 0 )
	{
		printf("wserver: pipe failed\n");
		exit(1);
	}
	cmdfd = cp[0];
	cmdwr = cp[1];

	/* Take over the screen + input: load /drv/hr (keyboard + polled mouse +
	 * hardware cursor), paint the desktop, then fork the input pump. */
	loaddriver();
	loadpty();				/* pty pairs for the terminal windows */
	/* A second driver fd (any minor) for cursor on/off; wire it into the
	 * engine so blits hide the driver's XOR cursor and leave no trails. */
	curfd = open("/dev/dmgr", 2);
	gfx_curhide_hook = srv_curhide;
	gfx_curshow_hook = srv_curshow;
	reset_screen();

	/* Load the system fonts into the shared VRAM tail before any glyph is
	 * drawn (chrome in launchapp->mkwin->srvtitle needs them).  One copy each;
	 * the server and every direct-render client blit from here. */
	loadfont(SHM_FTERM, "/usr/hr/fonts/gallant.hf");
	loadfont(SHM_FUI,   "/usr/hr/fonts/gacha.hf");
	loadfont(SHM_FICON, "/usr/hr/fonts/sail.hf");
	hr_glob()->curon = 1;			/* driver draws its cursor by default */
	hr_glob()->overlay = 0;			/* no menu/overlay up yet */
	hr_glob()->stacking = 0;		/* no layer op in flight yet */
	{ int w; for ( w = 0; w < MAX_WINDOWS; w++ ) hr_setdraw(w, 0); }
	hr_glob()->magic = HR_MAGIC;

	pumppid = startpump();

	/* Load the launchable-app catalog (/usr/hr/etc/apps), then auto-open a
	 * couple for a usable first screen; further apps start from the right-click
	 * desktop menu.  App 0 = terminal, app 1 = clock, per the config order. */
	loadapps();
	if ( napps > 0 ) launchapp(0, 48, 40);
	if ( napps > 1 ) launchapp(1, 470, 150);

	for (;;)
	{
		n = read(cmdfd, &c, sizeof(c));
		if ( n == sizeof(c) )
			docmd(&c);
	}
}
