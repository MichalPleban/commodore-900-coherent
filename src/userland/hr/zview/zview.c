/*
 * zview.c - the ZView window server (GUI.md Phase 1).
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
 * Nothing about the desktop's contents is compiled in: the catalog
 * /usr/hr/etc/apps (loadapps) names what the right-click desktop menu offers
 * AND which entries get a start-up placeholder icon -- an icon with no process
 * behind it that launches the app when clicked (placesync/drawplace) -- and
 * the shell script /usr/hr/etc/rc (runrc) starts anything the user wants
 * resident from power-on.  An app declares its own window when it connects
 * (wire.h HRCONN), and may be started either by us or straight from a shell.
 */
#include <stdio.h>
#include <signal.h>
#include "smgr.h"
#include "wire.h"
#include "shmem.h"
#include "hrdlg.h"		/* DLG_* chrome metrics (srvdialog) */

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
extern RECT	gfx_uprect;	/* damage rect that goes with a WM_UPDATE */

/* Per-window client bookkeeping, indexed by window id (0..MAX_WINDOWS-1). */
struct win {
	int	used;
	int	pid;		/* client process               */
	int	min;		/* 1 if minimised to a desktop icon */
	int	islot;		/* desktop-icon slot while minimised */
	int	sx, sy, sw, sh;	/* saved geometry while minimised */
	WSTRUCT	*wp;		/* stashed WSTRUCT while minimised (out of wtbl) */
	char	title[24];	/* displayed title (base, plus " #N" if duplicated) */
	char	base[16];	/* app name, before any #N suffix */
	int	inst;		/* stable instance number among same-base windows */
	char	icon[40];	/* .icn file for the desktop icon */
	int	appi;		/* catalog entry it was launched from (-1 unknown) */
	int	stretch;	/* 1 = the client allows the user to resize it */
	int	confirm;	/* 1 = window-menu Quit asks first (HRF_CONFIRM) */
	int	menu;		/* HRM_* the client wants in its window menu */
	int	ourkid;		/* 1 = we forked this client (launchapp), so  */
				/* its corpse is OURS to reap (see deadpool)  */
} wins[MAX_WINDOWS];

int	cmdfd;			/* server end (read) of the shared command pipe */
int	cmdwr;			/* write end kept so children can inherit it    */

/* Launchable applications, read from /usr/hr/etc/apps at startup (GUI.md: a
 * data-driven launcher menu so new software installs without recompiling the
 * server).  One line per app: name:execpath:multi:icon:X,Y (the last two
 * optional).  The catalog says only what the SERVER has to decide -- what the
 * launcher menu reads and whether a second copy may be started; the window's
 * title, size, icon and flags belong to the application and reach us in its
 * C_CONNECT (wire.h), so an app can also be run with a bare argv from a shell.
 *
 * An entry with an `icon' field additionally gets a PLACEHOLDER icon on the
 * desktop: it is drawn like a minimised window's icon (minus the live-process
 * badge, drawiconc) but NO process exists behind it -- clicking it launches
 * the program (at X,Y if given).
 * This replaced starting every app from the rc script minimised with -H,
 * which held a whole resident process per icon on a machine where memory is
 * the scarce thing.  A single-instance entry's placeholder leaves the desktop
 * while its window is open and comes back when it closes (placesync); a
 * multi-instance entry's is permanent, labelled "New <name>", and every click
 * starts a fresh copy.  The placeholder icon is named HERE, not by the app,
 * because it must exist before the app does; once the app connects, its own
 * declared artwork wins for the real minimised icon.
 *
 * The table is fixed-size bss (no heap) and the strings are filled at
 * runtime, so nothing here bloats the near-full data segment. */
#define MAX_APPS	12
struct app {
	char	name[16];	/* menu label                                     */
	char	path[40];	/* executable                                     */
	int	multi;		/* 1 = allow many instances; 0 = single (re-raise) */
	char	icon[20];	/* placeholder .icn; "" = no placeholder icon     */
	int	px, py;		/* window origin for a placeholder launch, -1 =   */
				/* none given (window opens where the click was)  */
	int	show;		/* 1 = placeholder icon currently on the desktop  */
	int	islot;		/* its desktop-icon slot while shown              */
} apps[MAX_APPS];
int	napps;

/* Launches that have been forked but have not yet sent their C_CONNECT.  The
 * window does not exist yet -- all we hold is where the click asked for it, which
 * catalog entry it is, and the event pipe the child already inherited; the client
 * supplies the rest.  Matched to the incoming record by pid. */
struct pend {
	int	used;
	int	pid;
	int	ai;		/* catalog index */
	int	x, y;		/* requested window origin */
	int	seq;		/* launch order, for reclaiming the oldest slot */
} pends[MAX_WINDOWS];
int	pendseq;

/* Children whose death is expected (a client whose window was killed, the rc
 * script's shell) wait here to be reaped.  The server can never call a bare
 * wait() -- it would block the whole desktop behind the input pump -- so each
 * entry is PROBED with kill(pid, 0) first: this kernel answers ESRCH for a
 * zombie (PSDEAD, coh/sys1.c ukill), and only a probe that says "dead" makes
 * the wait() call, which then cannot block -- a dead unreaped child of ours
 * guarantees at least one zombie to collect.  Without this every closed
 * window left a zombie in the process table for the life of the session,
 * which is process slots gone exactly when the desktop is starved (the
 * memory-exhaustion complaints started here).
 * A full pool just drops the entry -- that child stays a zombie until quitwm,
 * which is the pre-pool behaviour, not a new failure. */
#define NDEAD	(2 * MAX_WINDOWS)
int	deadpid[NDEAD];
int	ndead;

deadadd(pid)
{
	if ( pid > 0 && ndead < NDEAD )
		deadpid[ndead++] = pid;
}

/* Probe the pool; on the first corpse found, reap ONE zombie child.  wait()
 * may hand back a different child than the one probed (any zombie of ours) --
 * drop whichever entry it names (the probed one stays for the next pass, and
 * every zombie gets collected within a few passes).  At most one wait() per
 * call keeps this O(small) inside the main loop. */
reapdead()
{
	register int i, w;
	int st;

	for ( i = 0; i < ndead; i++ )
		if ( kill(deadpid[i], 0) < 0 )
		{
			w = wait(&st);
			if ( w < 0 )
				w = deadpid[i];	/* no child?? drop the entry */
			for ( i = 0; i < ndead; i++ )
				if ( deadpid[i] == w )
				{
					deadpid[i] = deadpid[--ndead];
					break;
				}
			return;
		}
}

int	nwins;			/* count of live windows          */
int	focuswid = -1;		/* window that receives keystrokes (Phase 2) */

/* Fonts live in the shared VRAM tail (shmem.h): the server loads three .hf files
 * into it once at startup, then the server AND every direct-render client blit
 * glyphs straight from that single copy with the asm bitblt (a whole glyph ROW
 * per masked word op -- never the per-pixel path the old kernel CIOGLYPH used).
 * SHM_FTERM = gacha regular 8x15 (terminal), SHM_FUI = gacha bold 9x16 (title
 * bars/menus), SHM_FICON = sail 6x8 (minimized-icon labels).  Terminal cell
 * metrics drive the (retired) C_TEXT/C_ERASE only: a terminal client reads the
 * font's own cellw/cellh out of the tail and sizes itself, so no cell metric is
 * handed out any more. */
#define HRFW	8		/* gacha.r (terminal) glyph cell width  (px) */
#define HRFH	15		/* gacha.r (terminal) glyph cell height (px) */

int	termcw = HRFW;		/* terminal cell width  (px) */
int	termch = HRFH;		/* terminal cell height (px) */
RECT	srvhrclip;		/* extra clip for glyphs (title bar); off if empty */

/* mouse / interaction state (Phase 1b) */
int	mx, my;			/* current pointer position (global coords) */

/* Pointer grab, for the selection gesture (wire.h E_BUTTON/E_MOTION).  A left
 * press inside a window's CONTENT grabs the pointer for that window and the
 * matching release lets it go; motion is forwarded to the client only in
 * between.  A client that is not dragging therefore never sees a motion stream
 * at all -- which matters because sendev() writes to a pipe, so a client that
 * fell behind could otherwise block the whole server.  lastgx/lastgy suppress a
 * motion record identical to the one before it. */
int	grabwid = -1;		/* window holding the pointer grab, or -1   */
int	lastgx = -1, lastgy = -1;	/* last motion forwarded (dedupe)   */
int	selwid = -1;		/* window that owns the selection, or -1    */

/* Client dialog overlay (wire.h C_DLGOPEN): at most ONE in the system.  While
 * dlgwid >= 0 the desktop is system-modal: every client is frozen (overlay =
 * OV_DLG|dlgwid), ALL input is routed to the owner as E_D* events (dlginput),
 * and window creation / kills / new dialogs are deferred until it closes. */
int	dlgwid = -1;		/* window owning the dialog, or -1          */
RECT	dlgbox;			/* the whole box, screen coords             */
RECT	dlgint;			/* the interior (the published surface)     */
int	*dlgsave;		/* saved pixels under the box (0 = malloc   */
				/* failed: restore falls back to repaint)   */
int	dlgwpl, dlgrows;	/* save-under geometry (words/row, rows)    */
int	dlggrab;		/* 1 = left button went down in the interior */
int	dlastx = -1, dlasty = -1;	/* E_DMOTION dedupe (interior coords) */
int	dlgput;			/* consecutive full-ring sends: dead owner  */
unsigned dlgbye;		/* wid bitmask: C_BYEs deferred to close    */
extern int who_top_at();
int	curfd = -1;		/* a driver fd for cursor on/off (CIOMSE*) */
int	pumppid = -1;		/* the input-pump child (killed on WM quit) */

/* Cursor arbitration hooks (installed into libhrgfx): the driver paints an
 * opaque save-under cursor, so hide it around each server blit or the blit is
 * stomped when the saved background is put back (GUI.md 6.2). */
/* Reference-counted so nested hide/show compose (the driver's cursor flag is a
 * plain boolean): only the OUTERMOST hide actually removes the cursor and
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
 * client (or the driver's cursor) can interleave -- this is what stops a
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
int	pex0[MAX_WINDOWS], pey0[MAX_WINDOWS];	/* and the damaged content rect, */
int	pex1[MAX_WINDOWS], pey1[MAX_WINDOWS];	/* accumulated as a union        */
flushexp()
{
	int w;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( pendexp & (1 << w) )
		{
			pendexp &= ~(1 << w);
			if ( wins[w].used && wtbl[w] )
				sendev(w, E_EXPOSE, pex0[w], pey0[w],
					pex1[w] - pex0[w], pey1[w] - pey0[w]);
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

/* Request an E_EXPOSE for the CONTENT-relative, half-open rect (x0,y0)-(x1,y1).
 * While the lock is held it is queued (flushed by srvunlock); otherwise it goes
 * out at once.
 *
 * Damage for one window accumulates as a BOUNDING UNION rather than a list of
 * rects.  A client repaints one rectangle per event, so N rects would cost N
 * events and N repaint passes; the union costs one, and the slack it adds is
 * cells the client would usually have to walk anyway.  What matters is that the
 * common cases stay small -- uncovering a strip of a terminal repaints that
 * strip, not the 80x25 grid behind it. */
qexposer(wid, x0, y0, x1, y1)
{
	if ( wid < 0 || wid >= MAX_WINDOWS || !wins[wid].used || !wtbl[wid] )
		return;
	if ( x0 < 0 ) x0 = 0;
	if ( y0 < 0 ) y0 = 0;
	if ( x1 > wtbl[wid]->wn_Psize.x ) x1 = wtbl[wid]->wn_Psize.x;
	if ( y1 > wtbl[wid]->wn_Psize.y ) y1 = wtbl[wid]->wn_Psize.y;
	if ( x0 >= x1 || y0 >= y1 )
		return;				/* nothing of the content was hit */
	if ( pendexp & (1 << wid) )
	{
		if ( x0 < pex0[wid] ) pex0[wid] = x0;
		if ( y0 < pey0[wid] ) pey0[wid] = y0;
		if ( x1 > pex1[wid] ) pex1[wid] = x1;
		if ( y1 > pey1[wid] ) pey1[wid] = y1;
	}
	else
	{
		pex0[wid] = x0;  pey0[wid] = y0;
		pex1[wid] = x1;  pey1[wid] = y1;
		pendexp |= 1 << wid;
	}
	if ( locklevel == 0 )
		flushexp();
}

/* Request a FULL-content E_EXPOSE for wid: everything this window has is gone
 * (it was just mapped, resized, or restacked from underneath). */
qexpose(wid)
{
	if ( wid < 0 || wid >= MAX_WINDOWS || !wins[wid].used || !wtbl[wid] )
		return;
	qexposer(wid, 0, 0, wtbl[wid]->wn_Psize.x, wtbl[wid]->wn_Psize.y);
}

/* Cursor sprites.  Each is TWO 16-word planes for CIOMOUSE: the ink (1 =
 * black pixel) followed by the opacity mask (ink | its 1-pixel white
 * outline, an 8-connected dilation; scratch script cursors.py regenerates
 * these).  The driver paints them as proper opaque cursors -- black ink,
 * white rim -- so they stay visible over black title bars.
 *
 * the default arrow (the old smgr wedge, nudged right+down 1px so the
 * outline can wrap its tip and top/left edges; the ink tip is at (1,1) and
 * the hotspot (0,0) lands on the outline's corner pixel) */
int DEF_MOUSE[] = { 0x0000, 0x7ffe, 0x7ffc, 0x7ff8,
		    0x7ff0, 0x7fe0, 0x7fe0, 0x7ff0,
		    0x7ff8, 0x7ffc, 0x7ffe, 0x79ff,
		    0x70ff, 0x407f, 0x003f, 0x001f,
		    /* mask */
		    0xffff, 0xffff, 0xffff, 0xfffe,
		    0xfffc, 0xfff8, 0xfff8, 0xfffc,
		    0xfffe, 0xffff, 0xffff, 0xffff,
		    0xffff, 0xf9ff, 0xe0ff, 0x007f };

/* the move/resize "hand" cursor (original desktop dmouse.c MOV_MOUSE): shown
 * while a ghost-drag is active, then DEF_MOUSE is restored. */
int MOV_MOUSE[] = { 0x0000, 0x01b0, 0x19b0, 0x19b6,
		    0x0db6, 0x0db6, 0x0ffe, 0x0ffe,
		    0x07fe, 0x67fe, 0x7ffe, 0x3ffe,
		    0x1ffc, 0x07fc, 0x07f8, 0x03f8,
		    /* mask (bottom outline clips at the cell edge -- the ink
		     * reaches row 15; harmless, it is the wrist) */
		    0x03f8, 0x3ff8, 0x3fff, 0x3fff,
		    0x3fff, 0x1fff, 0x1fff, 0x1fff,
		    0xffff, 0xffff, 0xffff, 0xffff,
		    0x7fff, 0x3ffe, 0x0ffe, 0x0ffc };

/* ...and ITS hotspot: the middle of the hand.  Unlike the arrows this shape has
 * no tip -- it is a fist -- so what it grabs with is its centre, which is the
 * centre of its ink (x 1..14, y 1..15) and so of its cell.
 *
 * Only RESIZE needs this.  Move is immune by construction: ghostdrag() records
 * gx = mx - origin at the grab and then tracks nx = mx - gx, so any constant
 * offset cancels and the window keeps whatever relationship to the hand it had
 * when it was grabbed.  Resize instead puts the frame's corner AT the pointer,
 * which without this correction is the sprite's top-left -- the corner ends up
 * outside the hand, above and left of it, instead of in its grip. */
#define MOV_HOTX	8
#define MOV_HOTY	8

/* the menu cursor (original desktop dmouse.c MNU_MOUSE): a right-pointing arrow
 * shown while a pop-up menu is open, then DEF_MOUSE is restored. */
int MNU_MOUSE[] = { 0x0000, 0x0180, 0x01c0, 0x01e0,
		    0x01f0, 0x7ff8, 0x7ffc, 0x7ffe,
		    0x7fff, 0x7ffe, 0x7ffc, 0x7ff8,
		    0x01f0, 0x01e0, 0x01c0, 0x0180,
		    /* mask (butt pulled in from col 0 so the left outline
		     * fits; the tip keeps col 15 -- its outline clips there,
		     * invisible since this cursor lives over white menus) */
		    0x03c0, 0x03e0, 0x03f0, 0x03f8,
		    0xfffc, 0xfffe, 0xffff, 0xffff,
		    0xffff, 0xffff, 0xffff, 0xfffe,
		    0xfffc, 0x03f8, 0x03f0, 0x03e0 };

/* ...and its HOTSPOT.  The driver has no notion of one: hrdraw() (drv/hr2.c)
 * puts the sprite's top-left corner at the pointer position.  That is right for
 * the shapes whose point IS that corner -- DEF_MOUSE is a wedge aimed up-left
 * out of (0,0) -- but MNU_MOUSE points RIGHT, so the pixel it aims with is the
 * middle of its right edge, 15 across and 8 down.  Left uncorrected the menu
 * highlights the item under the sprite's top-left corner while the arrow
 * visibly points at a different one, and near the bottom of the box the arrow
 * points at an item that cannot be selected at all.
 *
 * So while this cursor is up the menu works in TIP coordinates throughout: the
 * box is placed at the tip and mnu_item() hit-tests the tip.  Doing both keeps
 * the top-margin rule intact (the menu still opens with the tip ABOVE item 0,
 * so a press-and-release with no drag selects nothing, as in the original). */
#define MNU_HOTX	15
#define MNU_HOTY	8

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
/* Global pointer position -> a window's CONTENT-relative coordinates.  Reads the
 * origin and size straight out of the clip descriptor the server already
 * publishes for every window (shmem.h HRSURF), so there is no second source of
 * truth for where a window's content is.  Returns 0 when the point is outside
 * the content rect -- a hit on the title bar or the shadow is the window
 * manager's, not the client's. */
static
toclient(wid, gx, gy, cx, cy)
int *cx, *cy;
{
	register HRSURF *sp;
	register int x, y;

	if ( wid < 0 || wid >= MAX_WINDOWS || !wins[wid].used || wins[wid].min )
		return 0;
	sp = hr_surf(wid);
	if ( !sp->mapped )
		return 0;
	x = gx - sp->ox;
	y = gy - sp->oy;
	if ( x < 0 || y < 0 || x >= sp->cw || y >= sp->ch )
		return 0;
	*cx = x;
	*cy = y;
	return 1;
}

sendev(wid, type, a0, a1, a2, a3)
{
	WMSG e;

	if ( wid < 0 || wid >= MAX_WINDOWS || !wins[wid].used )
		return;
	e.wm_type = type;
	e.wm_wid = wid;
	e.wm_arg[0] = a0; e.wm_arg[1] = a1;
	e.wm_arg[2] = a2; e.wm_arg[3] = a3;
	e.wm_arg[4] = e.wm_arg[5] = 0;
	/* Into the window's ring in the shared tail (shmem.h SHM_EVQ), not a pipe.
	 * This CANNOT block: a client that stopped draining gets its ring marked
	 * overflowed and the event dropped, where a pipe write would have stalled
	 * the whole server -- the hazard qexpose/flushexp exists to dodge. */
	hr_evput(wid, (short *)&e);
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
	POINT o;

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
		/* Expose only what the engine says was damaged (gfx_uprect, absolute
		 * screen coords -- the regions this window was L_OBSCURED in), made
		 * content-relative; qexposer clips it to the content and unions it with
		 * any other damage still queued.  A raise therefore costs the client the
		 * strip it was covered by, not a repaint of its whole window.  QUEUED,
		 * not sent: this hook runs deep inside a locked layer op, and sending
		 * here could block on a client that is spinning on the lock (deadlock). */
		o = wtbl[wid]->wn_Crect.origin;
		qexposer(wid, gfx_uprect.origin.x - o.x, gfx_uprect.origin.y - o.y,
			      gfx_uprect.corner.x - o.x, gfx_uprect.corner.y - o.y);
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
	HRSURF *sp, t;
	WSTRUCT *wp;
	LAYER *lp;
	SM_REGION *rp;
	RECT cr, dr;
	int n, i;

	sp = hr_surf(wid);
	wp = wtbl[wid];
	t.mapped = 0;
	t.nvis = 0;
	t.ox = t.oy = t.cw = t.ch = 0;
	if ( wins[wid].used && !wins[wid].min && wp != (WSTRUCT *)NULL
	     && wp->wn_Layer != (LAYER *)NULL )
	{
		cr = wp->wn_Crect;
		lp = (LAYER *)wp->wn_Layer;
		t.ox = cr.origin.x;  t.oy = cr.origin.y;
		t.cw = cr.corner.x - cr.origin.x;
		t.ch = cr.corner.y - cr.origin.y;
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
			t.vis[n].x0 = dr.origin.x;  t.vis[n].y0 = dr.origin.y;
			t.vis[n].x1 = dr.corner.x;  t.vis[n].y1 = dr.corner.y;
			n++;
		}
		t.nvis = n;
		t.mapped = 1;
	}
	else
		hr_setdraw(wid, 0);	/* unmapped: not drawing -> clear drain flag */

	/* Publish only a descriptor that actually CHANGED.  publish_all() runs over
	 * every window after any op, but one window's raise/move leaves most of the
	 * others' clips identical -- and a client watches this seqlock to decide that
	 * the server restacked it, so an unconditional bump made every terminal on
	 * the screen throw away its diff and repaint in full whenever any other
	 * window was touched.  Comparing ~110 bytes is far cheaper than the repaint
	 * it saves. */
	if ( t.mapped == sp->mapped && t.nvis == sp->nvis && t.ox == sp->ox &&
	     t.oy == sp->oy && t.cw == sp->cw && t.ch == sp->ch )
	{
		for ( i = 0; i < t.nvis; i++ )
			if ( t.vis[i].x0 != sp->vis[i].x0 || t.vis[i].y0 != sp->vis[i].y0 ||
			     t.vis[i].x1 != sp->vis[i].x1 || t.vis[i].y1 != sp->vis[i].y1 )
				break;
		if ( i == t.nvis )
			return;				/* nothing to say */
	}
	sp->seq++;					/* odd: writing */
	sp->mapped = t.mapped;
	sp->ox = t.ox;  sp->oy = t.oy;
	sp->cw = t.cw;  sp->ch = t.ch;
	/* field by field, not a struct assignment: the destination is a far pointer
	 * into the shared VRAM tail, and every other write to the tail in this file
	 * is a plain scalar store through hr_*() -- keep it that way. */
	for ( i = 0; i < t.nvis; i++ )
	{
		sp->vis[i].x0 = t.vis[i].x0;
		sp->vis[i].y0 = t.vis[i].y0;
		sp->vis[i].x1 = t.vis[i].x1;
		sp->vis[i].y1 = t.vis[i].y1;
	}
	sp->nvis = t.nvis;
	sp->seq++;					/* even: done */
}

/* Mirror wins[] into the shared window list (shmem.h SHM_WINLIST) so any
 * client can enumerate the desktop.  Display-only fields -- the authoritative
 * wins[] stays private (its pids drive kill/reaping; the tail is writable by
 * everyone).  Same change-compare as publish_surf: wl_seq moves only when the
 * list really changed, so a poller can watch it.  Scalar far-pointer stores,
 * like every other write to the tail in this file. */
publish_wins()
{
	register HRWIN *pw;
	register HRWLIST *wl;
	register char *s;
	int w, i, diff;

	wl = hr_wlist();
	diff = 0;
	for ( w = 0; w < MAX_WINDOWS && !diff; w++ )
	{
		pw = &wl->wl_win[w];
		if ( wins[w].used )
		{
			if ( !pw->ww_used || pw->ww_pid != wins[w].pid ||
			     pw->ww_min != wins[w].min )
				diff = 1;
			else
				for ( s = wins[w].title, i = 0; i < 24; i++ )
					if ( pw->ww_title[i] != s[i] )
					{
						diff = 1;
						break;
					}
		}
		else if ( pw->ww_used )
			diff = 1;
	}
	if ( !diff )
		return;
	wl->wl_seq++;				/* odd: writing */
	for ( w = 0; w < MAX_WINDOWS; w++ )
	{
		pw = &wl->wl_win[w];
		if ( wins[w].used )
		{
			pw->ww_pid = wins[w].pid;
			pw->ww_min = wins[w].min;
			for ( s = wins[w].title, i = 0; i < 24; i++ )
				pw->ww_title[i] = s[i];
			pw->ww_used = 1;
		}
		else
			pw->ww_used = 0;
	}
	wl->wl_seq++;				/* even: done */
}

/* Republish every window: any single op can cover/uncover others. */
publish_all()
{
	int w;
	for ( w = 0; w < MAX_WINDOWS; w++ )
		publish_surf(w);
	publish_wins();
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

	/* Out of memory refusals are ALL-OR-NOTHING: either the window comes up
	 * complete or the screen is left exactly as it was.  newlayer() returns
	 * NULL before painting anything; the WSTRUCT failure comes after the
	 * layer was linked and its rect filled, so that one must tear the layer
	 * down and repaint what it briefly covered.  Either way wtbl[wid] stays
	 * NULL, doconnect clears the slot, and the unanswered client gives up
	 * and exits by itself (hrapp.c). */
	gkLayer = newlayer(r);
	if ( gkLayer == (LAYER *)NULL )
	{
		srvlogn("mkwin: no memory for layer, wid ", wid);
		srvunlock();
		return;
	}
	dbg("  mk: newlayer\n");
	gkLayer->base = screen_addr(r.origin.x, r.origin.y);
	gkLayer->width = 1024;

	wp = (WSTRUCT *)malloc(sizeof(WSTRUCT));
	if ( wp == (WSTRUCT *)NULL )
	{
		srvlogn("mkwin: no memory for window, wid ", wid);
		dellayer(gkLayer);	/* unlink + background the filled rect */
		perform_update();	/* repaint windows it briefly covered */
		publish_all();
		srvunlock();
		expose_covered(wid, r.origin.x, r.origin.y,
				    r.corner.x, r.corner.y);
		return;
	}
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
	RECT old;
	int hadr;

	if ( !wins[wid].used )
		return;
	if ( wid == dlgwid )		/* its dialog dies with it: restore the */
		dlgclose();		/* screen before the layer teardown     */
	if ( grabwid == wid )			/* died mid-drag: drop the pointer grab */
	{
		grabwid = -1;
		lastgx = lastgy = -1;
	}
	if ( selwid == wid )			/* its highlight goes with the window */
		selwid = -1;
	hr_ackclr(wins[wid].pid);	/* release its connect-ack slot */
	if ( wins[wid].ourkid )		/* our fork: reap the corpse when it turns up */
		deadadd(wins[wid].pid);
	sendev(wid, E_QUIT, 0, 0, 0, 0);	/* outside the lock: may block */
	hr_setdraw(wid, 0);		/* client is gone: drop its drain flag so the */
					/* srvlock() below (and later ops) don't spin */
	srvlock();
	srvlogn("killwin ", wid);
	strcpy(base, wins[wid].base);
	gfx_cursor_hide();
	if ( wins[wid].min )
		drawicon(wid, 0);		/* erase its desktop icon */
	hadr = 0;
	if ( wtbl[wid] )
	{
		LOADW(wid);
		if ( gkLayer )
		{
			old = gkLayer->rect;	/* what we are about to stop covering */
			hadr = 1;
		}
		dellayer(gkLayer);		/* uncovers + exposes those beneath */
		perform_update();
		free((char *)wtbl[wid]);
		wtbl[wid] = (WSTRUCT *)NULL;
	}
	wins[wid].used = 0;
	nwins--;
	relabel(base);				/* drop #N from a surviving sibling */
	gfx_cursor_show();
	publish_all();
	srvunlock();
	/* Repaint what this window was covering.  dellayer's own update list says the
	 * same thing through the reply hook, but that list is the one minwin records
	 * as not always delivered -- and now that an expose carries a RECT instead of
	 * "repaint everything", a region it misses is a region nobody repaints. */
	if ( hadr )
		expose_covered(wid, old.origin.x, old.origin.y,
				    old.corner.x, old.corner.y);
	redraw_icons();				/* closing a window may uncover icons */
	placesync();		/* last window of a single-instance app gone: its
				 * placeholder icon returns to the desktop */
}

/* Raise a window to the front (click-to-raise / demo cycling). */
raisewin(wid)
{
	LAYER *fp;
	RECT r, ir, dam;
	int got;

	if ( !wins[wid].used || !wtbl[wid] )
		return;
	/* Already frontmost?  Then nothing is restacked and nothing is uncovered,
	 * so there is nothing to expose.  upfront() itself returns early in that
	 * case, but the rest of this function did not -- so every click-to-raise on
	 * a window that was already in front still sent its client a full-content
	 * E_EXPOSE and made it repaint everything for nothing.  That is once per
	 * click, and it is also the press that STARTS a text selection, which is
	 * why a drag began with the whole terminal flashing through a repaint.
	 * publish_all() is skipped for the same reason: a clip descriptor cannot
	 * have changed if the z-order did not. */
	focuswid = wid;				/* raised window takes the keyboard */
	if ( wtbl[wid]->wn_Layer == DM_frontmost )
		return;
	srvlock();
	LOADW(wid);
	/* What this raise actually damages is the part of us that the windows IN
	 * FRONT were covering -- everything else of ours is still on screen and must
	 * not be repainted.  Work it out from the z-order BEFORE the restack (the
	 * layer list runs ->front towards the front), as a bounding union.  upfront()
	 * reports the same thing from the layer's L_OBSCURED regions and it unions in
	 * via qexposer, but that list is the one minwin's comment records as not
	 * always delivered, so do not depend on it alone.
	 *
	 * This replaces the qexpose(wid) that used to follow upfront(): a full-content
	 * expose made every click-to-raise repaint the whole terminal (~2000 cells) to
	 * recover the strip a neighbour had been covering. */
	r = gkLayer->rect;
	got = 0;
	for ( fp = gkLayer->front; fp != (LAYER *)NULL; fp = fp->front )
	{
		ir = R_Intersection(fp->rect, r);
		if ( R_null(ir) )
			continue;
		if ( !got )
		{
			dam = ir;
			got = 1;
		}
		else
		{
			if ( ir.origin.x < dam.origin.x ) dam.origin.x = ir.origin.x;
			if ( ir.origin.y < dam.origin.y ) dam.origin.y = ir.origin.y;
			if ( ir.corner.x > dam.corner.x ) dam.corner.x = ir.corner.x;
			if ( ir.corner.y > dam.corner.y ) dam.corner.y = ir.corner.y;
		}
	}
	gfx_cursor_hide();
	upfront(gkLayer);			/* exposes whatever it now covers... */
	SAVEW(wid);
	gfx_cursor_show();
	if ( got )
	{
		POINT o;

		o = wtbl[wid]->wn_Crect.origin;	/* damage is content-relative */
		qexposer(wid, dam.origin.x - o.x, dam.origin.y - o.y,
			      dam.corner.x - o.x, dam.corner.y - o.y);
	}
	publish_all();				/* z-order changed: refresh all clips */
	srvunlock();				/* flushes the queued exposes */
}

/* Expose every mapped window (except exclwid) whose layer rect intersects the
 * rect (rx0,ry0)-(rx1,ry1): used when a window is hidden or pushed back to
 * repaint what it was covering (the engine reply hook does not reliably do it). */
expose_covered(exclwid, rx0, ry0, rx1, ry1)
{
	int w;
	LAYER *lp;
	HRSURF *sp;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( w != exclwid && wins[w].used && !wins[w].min && wtbl[w] &&
		     (lp = wtbl[w]->wn_Layer) &&
		     lp->rect.origin.x < rx1 && lp->rect.corner.x > rx0 &&
		     lp->rect.origin.y < ry1 && lp->rect.corner.y > ry0 )
		{
			/* The client repaints only its CONTENT; the frame, title
			 * bar and shadow are the server's.  The engine redraws
			 * those from dellayer/pushback's update list -- but that
			 * list lives in fixed-size tables (update[] / the per-
			 * layer reg[]), and with many overlapping windows they
			 * saturate and entries drop, leaving an uncovered
			 * window's decoration background-filled (the "closing
			 * one of many windows eats the title bars below" bug).
			 * Redraw the decoration unconditionally: outline/
			 * srvtitle clip to the layer's L_VISIBLE regions, so
			 * this can never paint over a covering window. */
			redecorate(w);
			/* Only the part this window had COVERED is damaged, so send
			 * that and not the whole content: sp->ox/oy is where the
			 * content sits on screen (shmem.h HRSURF), and qexposer
			 * clips the result back to the content rect. */
			sp = hr_surf(w);
			qexposer(w, rx0 - sp->ox, ry0 - sp->oy,
				    rx1 - sp->ox, ry1 - sp->oy);
		}
}

/* Expose only the VACATED part of a move/resize: the strips of `old' that the
 * window's new rect no longer covers.  Windows under old-INTERSECT-new were
 * covered before AND after -- the op uncovered nothing of theirs, so exposing
 * the whole old rect (as this used to) made everything under a dragged or
 * grown window repaint for no reason.  A grow vacates nothing and exposes
 * nothing; a small drag costs the two trailing strips. */
expose_vacated(wid, old, new)
RECT old, new;
{
	int y0, y1;

	if ( new.corner.x <= old.origin.x || new.origin.x >= old.corner.x ||
	     new.corner.y <= old.origin.y || new.origin.y >= old.corner.y )
	{
		/* disjoint: the whole old rect was vacated */
		expose_covered(wid, old.origin.x, old.origin.y,
				    old.corner.x, old.corner.y);
		return;
	}
	if ( old.origin.y < new.origin.y )		/* top strip */
		expose_covered(wid, old.origin.x, old.origin.y,
				    old.corner.x, new.origin.y);
	if ( old.corner.y > new.corner.y )		/* bottom strip */
		expose_covered(wid, old.origin.x, new.corner.y,
				    old.corner.x, old.corner.y);
	y0 = old.origin.y > new.origin.y ? old.origin.y : new.origin.y;
	y1 = old.corner.y < new.corner.y ? old.corner.y : new.corner.y;
	if ( old.origin.x < new.origin.x )		/* left strip */
		expose_covered(wid, old.origin.x, y0, new.origin.x, y1);
	if ( old.corner.x > new.corner.x )		/* right strip */
		expose_covered(wid, new.corner.x, y0, old.corner.x, y1);
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
 * keep it out of the 64K resident kernel; zterm needs it to allocate its
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
/* input: the pump child                                              */
/* ------------------------------------------------------------------ */

/* Fork + exec the input pump.  The pump used to be a plain fork of this
 * process (the V7 two-process split: it blocks in CIOGETM while the server
 * blocks in read(), neither needs select()) -- but zview is one ~69Kb
 * non-shared software segment, so that fork parked a full contiguous copy
 * of the server image in RAM for the whole session.  The pump, and the
 * scancode->ASCII keymap with it, now lives in the TINY libc-only
 * /usr/hr/bin/zvpump, exactly as zterm's pumps live in hrpump (zterm.c:
 * "a fork clones this process's whole data/BSS").  It inherits the command
 * pipe's write end ON HR_CMDFD and nothing else. */
startpump()
{
	int pid, f;

	pid = fork();
	if ( pid == 0 )
	{
		close(cmdfd);
		dup2(cmdwr, HR_CMDFD);	/* zvpump writes the constant HR_CMDFD */
		for ( f = 5; f < 20; f++ )
			close(f);
		execl("/usr/hr/bin/zvpump", "zvpump", (char *)0);
		_exit(1);
	}
	return pid;
}

/* ------------------------------------------------------------------ */
/* window interaction (move / resize / minimise / raise)              */
/* ------------------------------------------------------------------ */

/* Hidden windows iconify to icons on the desktop (period-authentic: no dock).
 * Icons start at the TOP-LEFT of the desktop and pack left-to-right (lowest free
 * slot); each is a 48x48 .icn glyph with the window title beneath.  A row holds
 * ICONPR of them and further slots wrap onto the next row down, so the desktop
 * takes as many rows as the minimised windows need. */
#define ICONW	48			/* icon glyph size */
#define ICONLH	11			/* label height: 1px gap under the icon + a
					 * white plate of 1px pad + sail 6x8 + 1px pad */
#define ICONPAD	8			/* cell margin around the 48px glyph */
#define ICONCW	(ICONW + 2 * ICONPAD)	/* per-icon cell width == column pitch */
#define ICONCH	(ICONW + ICONLH + 8)	/* per-icon cell height == row pitch */
#define ICONLMAX ((ICONCW - 2) / 6)	/* label chars that fit (sail is 6 wide) */
#define ICONX0	12			/* left margin of the icon field */
#define ICONY0	12			/* top margin of the icon field */
#define ICONPR	((XMAX - 2 * ICONX0) / ICONCW)	/* icons per row */
#define iconx(slot) (ICONX0 + ((slot) % ICONPR) * ICONCW)
#define icony(slot) (ICONY0 + ((slot) / ICONPR) * ICONCH)

/* Fallback artwork when an icon is not installed (see iconok below). */
#define HR_DEFICON	"icon0.icn"

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
 * same hrfont glyphs as the terminal, clipped to the bar so the 22px cell never
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
		ty = r.origin.y + 3 + (WD_TITLEH - 1 - hr_font(SHM_FUI)->cellh) / 2;
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

/* Lowest desktop-icon slot not currently occupied by a minimised window or by
 * an app placeholder -- the two kinds share the one icon field. */
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
		for ( w = 0; w < napps && !used; w++ )
			if ( apps[w].show && apps[w].islot == s )
				used = 1;
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

/* Core icon-cell painter: draw (on) or erase (off) the 48px artwork `iname' +
 * text `label' in desktop-icon slot `slot', CLIPPED to the parts of its cell
 * not covered by any window -- icons are not layers, so we clip them to the
 * bare desktop ourselves (a partially covered icon shows its visible piece; a
 * fully covered one shows nothing, and moving a window off it makes it
 * reappear via redraw_icons).  Shared by a minimised window's icon (drawicon,
 * live=1) and an app placeholder's (drawplace, live=0): a LIVE icon has a
 * running process behind it and carries a marker -- a small black square badge
 * in the glyph's bottom-right corner -- so it reads apart from a placeholder,
 * which looks the same but starts a fresh process when clicked. */
drawiconc(slot, iname, label, on, live)
char *iname, *label;
{
	RECT cell, plate, badge, vr[ICONRB], lc;
	HRFONT *f;
	char lab[ICONLMAX + 1];
	int x, y, nvr, i, lx, ly, nc;

	x = iconx(slot);
	y = icony(slot);
	cell.origin.x = x - ICONPAD;             cell.origin.y = y - 2;
	cell.corner.x = x + ICONW + ICONPAD;     cell.corner.y = y + ICONW + ICONLH + 2;
	/* The label is black-on-white; the glyph cells alone would butt straight
	 * against the (also white) icon artwork and read as one blob.  So sit the
	 * text 2px below the icon and paint a white plate one pixel bigger than the
	 * text on every side: 1px of bare desktop separates plate from icon, and
	 * 1px of white pads the glyphs inside the plate.  The label is centred under
	 * the glyph and truncated to what the cell holds. */
	f = hr_font(SHM_FICON);
	nc = strlen(label);
	if ( nc > ICONLMAX )
		nc = ICONLMAX;
	strncpy(lab, label, nc);
	lab[nc] = '\0';
	lx = x + (ICONW - nc * f->cellw) / 2;
	ly = y + ICONW + 2;
	plate.origin.x = lx - 1;           plate.origin.y = ly - 1;
	plate.corner.x = lx + nc * f->cellw + 1;
	plate.corner.y = ly + f->cellh + 1;
	/* The live marker: a 6x6 black square inside a 10x10 white surround at the
	 * glyph's bottom-right corner.  The white ring keeps it readable even where
	 * the artwork's own strokes reach the corner. */
	badge.origin.x = x + ICONW - 10;   badge.origin.y = y + ICONW - 10;
	badge.corner.x = x + ICONW;        badge.corner.y = y + ICONW;
	nvr = desktop_rects(cell, vr, ICONRB);
	gfx_cursor_hide();
	for ( i = 0; i < nvr; i++ ) {
		srvfill(vr[i], 10, L_TRUE);		/* clear this visible piece */
		if ( !on )
			continue;
		srvicon(x, y, iname, vr[i]);	/* app icon, clipped */
		if ( live ) {
			lc = R_Intersection(badge, vr[i]);
			if ( !R_null(lc) )
				srvfill(lc, 0, L_TRUE);	/* white surround */
			lc.origin.x = badge.origin.x + 2;  lc.origin.y = badge.origin.y + 2;
			lc.corner.x = badge.corner.x - 2;  lc.corner.y = badge.corner.y - 2;
			lc = R_Intersection(lc, vr[i]);
			if ( !R_null(lc) )
				srvfill(lc, 0, L_FALSE);	/* black square */
		}
		if ( nc <= 0 )
			continue;
		lc = R_Intersection(plate, vr[i]);
		if ( !R_null(lc) ) {
			srvfill(lc, 0, L_TRUE);		/* white plate under the text */
			srvmenuglyphs(SHM_FICON, lx, ly, lab, lc);
		}
	}
	gfx_cursor_show();
}

/* A minimised window's desktop icon: artwork from its connect record, label =
 * its (possibly "#N"-suffixed) title. */
drawicon(wid, on)
{
	drawiconc(wins[wid].islot, wins[wid].icon, wins[wid].title, on, 1);
}

/* Catalog entry `ai's placeholder icon.  A multi-instance entry is labelled
 * "New <name>" -- clicking it always starts a FRESH copy, and the label is
 * what tells the user so (a plain "Shell" icon would read as one particular
 * shell); a single-instance entry just shows its name, since its placeholder
 * and its minimised window are never on screen together. */
drawplace(ai, on)
{
	char lab[24];

	if ( apps[ai].multi )
		sprintf(lab, "New %s", apps[ai].name);
	else
		strcpy(lab, apps[ai].name);
	drawiconc(apps[ai].islot, apps[ai].icon, lab, on, 0);
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
	for ( w = 0; w < napps; w++ )
		if ( apps[w].show )
			drawplace(w, 1);
	srvunlock();
}

/* Recompute which catalog entries should be showing a placeholder icon and
 * draw/erase the deltas.  Visible = has artwork AND (multi-instance, or no
 * live window launched from that entry).  Called at startup (everything
 * shows), after a connect (a single-instance app now has its window: its
 * placeholder comes off) and after a window teardown (the last window of a
 * single-instance app closed: its placeholder returns) -- so from the user's
 * side the app seems to minimise itself back to its icon, but the process and
 * its memory are gone. */
placesync()
{
	int i, vis;

	srvlock();
	for ( i = 0; i < napps; i++ )
	{
		if ( !apps[i].icon[0] )
			continue;
		vis = apps[i].multi || appwindow(i) < 0;
		if ( vis && !apps[i].show )
		{
			apps[i].islot = iconslot();
			apps[i].show = 1;
			drawplace(i, 1);
		}
		else if ( !vis && apps[i].show )
		{
			drawplace(i, 0);
			apps[i].show = 0;
			apps[i].islot = -1;
		}
	}
	srvunlock();
}

/* Rebuild window `wid's layer at rectangle r (used to restore a minimised
 * window; the WSTRUCT and its graphics state are preserved).  wtbl[wid] is
 * temporarily removed so make_vis_list() (run inside newlayer) never touches
 * this window's not-yet-existent layer.  Returns 1, or 0 when out of memory
 * -- the window is then left exactly as it was (all-or-nothing). */
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
	if ( gkLayer == (LAYER *)NULL )
	{
		srvlogn("relayout: no memory, wid ", wid);
		wtbl[wid] = wp;		/* untouched: still a valid icon */
		return 0;
	}
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
	return 1;
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
	if ( !relayout(wid, r) )
	{
		/* Out of memory: stay minimised, redraw the icon we erased.
		 * Nothing else changed -- all-or-nothing. */
		wtbl[wid] = (WSTRUCT *)NULL;
		drawicon(wid, 1);
		gfx_cursor_show();
		srvunlock();
		return;
	}
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
	RECT r, old;
	int w, h;

	if ( !wtbl[wid] )
		return;
	srvlock();
	old = wtbl[wid]->wn_Layer->rect;	/* the area we are vacating */
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
	/* and everyone under the part of the old spot we VACATED repaints it (see
	 * the same call in killwin: an expose now carries a rect, so damage the
	 * engine's update list happens to miss is damage nobody would repaint).
	 * Only old minus new: whoever is under the still-covered intersection was
	 * covered before and after and repaints nothing. */
	expose_vacated(wid, old, wtbl[wid]->wn_Layer->rect);
	redraw_icons();			/* moving off an icon must repaint it */
}

/* Resize window wid so its outer corner is (cx,cy).  Only for a client that
 * allowed it (HRF_STRETCH); the window menu already hides "Stretch" otherwise,
 * this is the belt-and-braces check. */
resizewin(wid, cx, cy)
{
	RECT r, old;
	int nw, nh;

	if ( !wtbl[wid] || !wins[wid].stretch )
		return;
	srvlock();
	old = wtbl[wid]->wn_Layer->rect;	/* a shrink vacates part of this */
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
	/* a shrink uncovers the vacated strips of the old rect: repaint whoever
	 * was under THOSE (a grow vacates nothing and exposes nothing) */
	expose_vacated(wid, old, wtbl[wid]->wn_Layer->rect);
	redraw_icons();			/* resizing off an icon must repaint it */
}

/* ------------------------------------------------------------------ */
/* application launcher (config-driven)                               */
/* ------------------------------------------------------------------ */

/* Read /usr/hr/etc/apps into apps[].  One line per app:
 * name:execpath:multi:icon:X,Y  -- the last two optional ('#'/blank lines
 * skipped).  `icon' makes the entry a desktop placeholder (struct app above);
 * artwork that is not installed falls back to the generic icon, like a
 * client's connect record does. */
/* One catalog line -> an apps[] entry (comment/blank/overlong-comment lines
 * come through here too and are skipped). */
static
appline(p)
char *p;
{
	char *fld[5];
	char *f;
	int nf;

	if ( !*p || *p == '#' || napps >= MAX_APPS )
		return;
	nf = 0;
	fld[nf++] = p;
	f = p;
	while ( nf < 5 )
	{
		while ( *f && *f != ':' )
			f++;
		if ( !*f )
			break;
		*f++ = 0;
		fld[nf++] = f;
	}
	if ( nf >= 2 )
	{
		struct app *a;

		a = &apps[napps++];
		strncpy(a->name, fld[0], sizeof(a->name) - 1);
		strncpy(a->path, fld[1], sizeof(a->path) - 1);
		a->multi = (nf > 2) ? atoi(fld[2]) : 0;
		a->px = a->py = -1;
		a->show = 0;
		a->islot = -1;
		if ( nf > 3 && fld[3][0] )
			strncpy(a->icon,
				iconok(fld[3]) ? fld[3] : HR_DEFICON,
				sizeof(a->icon) - 1);
		if ( nf > 4 )
		{
			char *q;

			a->px = atoi(fld[4]);
			for ( q = fld[4]; *q && *q != ','; q++ )
				;
			if ( *q )
				a->py = atoi(q + 1);
			if ( a->px < 0 || a->py < 0 )
				a->px = a->py = -1;
		}
	}
}

loadapps()
{
	int fd, nb, ll;
	char rb[256];		/* read chunk                             */
	char line[160];		/* one line; longer ones are truncated,   */
				/* which only ever bites a comment        */
	register int i;

	napps = 0;
	fd = open("/usr/hr/etc/apps", 0);
	if ( fd < 0 )
		return;
	/* Stream the file a chunk at a time: the catalog must never be
	 * silently truncated by a fixed whole-file buffer (it once was, at
	 * 2048 bytes -- the entries past the header comment just vanished). */
	ll = 0;
	while ( (nb = read(fd, rb, sizeof(rb))) > 0 )
	{
		for ( i = 0; i < nb; i++ )
		{
			if ( rb[i] != '\n' )
			{
				if ( ll < sizeof(line) - 1 )
					line[ll++] = rb[i];
				continue;
			}
			line[ll] = 0;
			ll = 0;
			appline(line);
		}
	}
	if ( ll > 0 )
	{
		line[ll] = 0;
		appline(line);
	}
	close(fd);
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
	publish_wins();		/* titles changed after mkwin's publish_all
				 * (doconnect relabels last): republish */
	srvunlock();
}

/* Start apps[ai], asking for its window at (x,y).  The child is exec'd with a
 * BARE argv -- it takes no window id, size or cell metrics from us -- and the
 * window itself is not created here: it appears when the child answers with its
 * C_CONNECT (doconnect below), which is what carries the title/size/icon/flags.
 * All we keep meanwhile is a pending slot.  Returns 0, or -1 if it could not be
 * started. */
/* Is pending-launch slot p still backed by a live process?  A launch that
 * died before its C_CONNECT -- under memory exhaustion the exec or the app's
 * own start-up fails -- must not hold its slot: apppending() would then say
 * "on its way" for ever and every later click on that icon would do nothing,
 * even long after the memory came back (the stuck-icon bug).  kill(pid, 0)
 * probes liveness (a zombie is PSDEAD: ESRCH); a dead launch frees the slot
 * and is reaped on the spot -- wait() cannot block, the pid is our own
 * unreaped child, so a zombie child of ours exists right now. */
static
pendlive(p)
{
	int st;

	if ( !pends[p].used )
		return 0;
	if ( kill(pends[p].pid, 0) >= 0 )
		return 1;
	pends[p].used = 0;	/* died before its C_CONNECT: free + reap */
	wait(&st);
	return 0;
}

launchapp(ai, x, y)
{
	int i, p, ev[2], pid;
	struct app *a;

	if ( ai < 0 || ai >= napps )
		return -1;
	a = &apps[ai];

	p = -1;
	for ( i = 0; i < MAX_WINDOWS; i++ )
		if ( !pendlive(i) )		/* free, or freed just now: a dead */
		{				/* launch must not hold its slot   */
			p = i;
			break;
		}
	if ( p < 0 )
	{
		/* Every slot is held by a launch that never connected (an app that
		 * died in its own start-up).  Rather than refuse for ever, drop the
		 * oldest: its event pipe closes, so if it is somehow still alive its
		 * connect write fails and it exits. */
		p = 0;
		for ( i = 1; i < MAX_WINDOWS; i++ )
			if ( pends[i].seq < pends[p].seq )
				p = i;
		srvlogn("launch: reclaiming pending slot ", p);
		pends[p].used = 0;
	}

	pid = fork();
	if ( pid == 0 )
	{
		dup2(cmdwr, HR_CMDFD);
		{ int f; for ( f = 5; f < 20; f++ ) close(f); }
		execl(a->path, a->name, (char *)0);
		_exit(1);
	}
	if ( pid < 0 )
		return -1;
	pends[p].used = 1;
	pends[p].pid = pid;
	pends[p].ai = ai;
	pends[p].x = x;
	pends[p].y = y;
	pends[p].seq = ++pendseq;
	return 0;
}

/* Run the desktop start-up script: an ordinary /bin/sh script naming the apps
 * the user wants on screen, with their -P/-S/-H options (hrapp.h), so what the
 * desktop comes up with is configuration rather than server code.
 *
 * Its apps are grandchildren, not children, which no longer matters: all they
 * need to inherit is the shared command pipe, and that passes down the whole
 * process tree.  Their events come from their window's ring in the shared tail
 * (shmem.h SHM_EVQ), which needs nothing inherited.  Every app in the script
 * must therefore be started in the BACKGROUND (&): they do not exit, and sh runs
 * the lines one at a time.
 *
 * We do not wait for the script -- the desktop has to come up whatever it does,
 * and the connects it provokes are answered from the main loop. */
#define HRRCFILE	"/usr/hr/etc/rc"
runrc()
{
	int pid, f;

	pid = fork();
	if ( pid == 0 )
	{
		close(cmdfd);			/* our read end of the command pipe */
		dup2(cmdwr, HR_CMDFD);
		for ( f = 5; f < 20; f++ )
			close(f);
		execl("/bin/sh", "sh", HRRCFILE, (char *)0);
		_exit(1);
	}
	return pid;
}

/* Is `name' an installed icon?  A client may ask for artwork that was never
 * shipped (or ask for none at all); rather than leave a minimised window with a
 * bare label, fall back to the generic application icon (HR_DEFICON) -- and if
 * even that is missing, to nothing, which drawiconc already tolerates. */
iconok(name)
char *name;
{
	char path[64];
	int fd;

	if ( !name || !*name )
		return 0;
	if ( strlen(name) > sizeof(path) - 16 )
		return 0;
	strcpy(path, "/usr/hr/icons/");
	strcat(path, name);
	if ( (fd = open(path, 0)) < 0 )
		return 0;
	close(fd);
	return 1;
}

/* Where to put a window nobody placed: an app started from a shell names no
 * position unless it was given -P, so step them down the desktop the way every
 * window manager has since. */
#define CASC_X0		64
#define CASC_Y0		48
#define CASC_STEP	28
#define CASC_MAX	8
int	cascade;

/* A client has declared its window (C_CONNECT).  Create it now -- at the size
 * the client asked for, and at the position the client asked for (-P), else the
 * one the launcher click asked for, else the next cascade step -- and answer
 * with E_CONNECTED carrying the window id and the GRANTED content size.
 *
 * The client is not necessarily one we forked: an unknown pid is an app started
 * from a shell, which is a normal way in.  All a pending slot
 * adds is where the click was and which catalog entry it came from. */
doconnect(hc)
HRCONN *hc;
{
	int i, p, wid, cw, ch, ww, hh, x, y, ai;
	RECT r;

	p = -1;
	for ( i = 0; i < MAX_WINDOWS; i++ )
		if ( pends[i].used && pends[i].pid == hc->hc_pid )
		{
			p = i;
			break;
		}
	if ( p >= 0 )
	{
		ai = pends[p].ai;
		x = pends[p].x;
		y = pends[p].y;
		pends[p].used = 0;
	}
	else
	{
		ai = -1;		/* not one of ours: the rc script started it */
		x = CASC_X0 + cascade * CASC_STEP;
		y = CASC_Y0 + cascade * CASC_STEP;
		cascade = (cascade + 1) % CASC_MAX;
	}
	if ( hc->hc_flags & HRF_POS )		/* -P: the client places itself */
	{
		x = hc->hc_x;
		y = hc->hc_y;
	}
	hc->hc_title[HRC_TITLE-1] = 0;		/* never trust the wire */
	hc->hc_icon[HRC_ICON-1] = 0;

	for ( wid = 0; wid < MAX_WINDOWS; wid++ )
		if ( !wins[wid].used && wtbl[wid] == (WSTRUCT *)NULL )
			break;
	if ( wid == MAX_WINDOWS )		/* desktop full */
	{
		/* Nothing to answer on: the client has no window, so it has no ring
		 * either.  It gives up by itself when no ack appears (hrapp.c). */
		srvlogn("desktop full, refusing pid ", hc->hc_pid);
		return;
	}

	/* Fit the requested content size, then the whole window, on the screen. */
	cw = hc->hc_w;  ch = hc->hc_h;
	if ( cw < 16 ) cw = 16;
	if ( ch < 16 ) ch = 16;
	ww = cw + 2 * WD_BORDER + WD_SHADOW;
	hh = ch + WD_TITLEH + WD_BORDER + WD_SHADOW;
	if ( ww > XMAX ) { ww = XMAX; cw = ww - 2 * WD_BORDER - WD_SHADOW; }
	if ( hh > YMAX ) { hh = YMAX; ch = hh - WD_TITLEH - WD_BORDER - WD_SHADOW; }
	if ( x + ww > XMAX ) x = XMAX - ww;
	if ( y + hh > YMAX ) y = YMAX - hh;
	if ( x < 0 ) x = 0;
	if ( y < 0 ) y = 0;
	r.origin.x = x;       r.origin.y = y;
	r.corner.x = x + ww;  r.corner.y = y + hh;

	strncpy(wins[wid].base, hc->hc_title, sizeof(wins[wid].base) - 1);
	wins[wid].base[sizeof(wins[wid].base) - 1] = 0;
	strcpy(wins[wid].title, wins[wid].base);
	strcpy(wins[wid].icon,
	       iconok(hc->hc_icon) ? hc->hc_icon : HR_DEFICON);
	wins[wid].appi = ai;
	wins[wid].stretch = (hc->hc_flags & HRF_STRETCH) != 0;
	wins[wid].confirm = (hc->hc_flags & HRF_CONFIRM) != 0;
	wins[wid].menu = hc->hc_menu & HRM_ALL;	/* its own window-menu entries */
	wins[wid].pid = hc->hc_pid;
	wins[wid].ourkid = (p >= 0);	/* forked by launchapp: reap it when it dies */
	wins[wid].min = 0;
	hr_evinit(wid);			/* a reused wid must not inherit the old
					 * occupant's queued events */
	wins[wid].used = 1;		/* mark used BEFORE mkwin so its publish_all
					 * marks the descriptor mapped=1 before the
					 * client is told its id (else the client
					 * races a mapped=0 descriptor and its first
					 * paint is dropped). */
	mkwin(wid, r);
	if ( wtbl[wid] == (WSTRUCT *)NULL )	/* out of memory: no window */
	{
		wins[wid].used = 0;
		return;
	}
	nwins++;
	/* instance number = 1 + highest among the live windows of this app, then
	 * relabel so #N appears on all of them once there are two or more. */
	{
		int w, hi;
		hi = 0;
		for ( w = 0; w < MAX_WINDOWS; w++ )
			if ( w != wid && wins[w].used &&
			     !strcmp(wins[w].base, wins[wid].base) && wins[w].inst > hi )
				hi = wins[w].inst;
		wins[wid].inst = hi + 1;
	}
	relabel(wins[wid].base);
	/* A single-instance app now has its window: its placeholder icon (if it
	 * had one) comes off the desktop.  Done BEFORE a -H minimise below picks
	 * an icon slot, so the slot the placeholder frees is the lowest one again
	 * and the minimised icon lands where the placeholder sat. */
	placesync();

	/* The granted content size, read out BEFORE anything else can happen to the
	 * window: -H takes wtbl[wid] away (minwin stashes the WSTRUCT). */
	cw = wtbl[wid]->wn_Crect.corner.x - wtbl[wid]->wn_Crect.origin.x;
	ch = wtbl[wid]->wn_Crect.corner.y - wtbl[wid]->wn_Crect.origin.y;
	if ( hc->hc_flags & HRF_MIN )
		minwin(wid);		/* -H: straight to a desktop icon, and it */
					/* does not take the keyboard either      */
	else
		focuswid = wid;
	/* The window exists and its clip descriptor is published: hand the client
	 * its id and the content size it really got.  A minimised window is
	 * unmapped, so the client's first paint is simply dropped and it draws for
	 * real on the E_EXPOSE it gets when the user restores it. */
	/* Publish the answer in the tail BEFORE writing it to the pipe: the pipe is
	 * not trustworthy for a client that made its own named one (shmem.h SHM_ACK),
	 * and the client takes whichever reaches it first. */
	hr_ackput(hc->hc_pid, wid, cw, ch);
	sendev(wid, E_CONNECTED, wid, cw, ch, 0);
	{				/* wake anyone waiting to be connected */
		WMSG k;
		int i;

		k.wm_type = E_CONNECTED;
		k.wm_wid = wid;
		for ( i = 0; i < WM_NARG; i++ )
			k.wm_arg[i] = 0;
		hr_evput(EVQ_CONNECT, (short *)&k);
	}
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

/* Which item (0..n-1) the menu arrow POINTS AT, or -1 if outside the box or
 * still in the top margin (nothing selected yet).  The pointer is (mx,my), the
 * sprite's top-left; what the user aims with is its tip (see MNU_HOTX). */
static
mnu_item(box, itemh, n)
RECT box;
{
	int it, tx, ty;

	tx = mx + MNU_HOTX;			/* where the arrow actually points */
	ty = my + MNU_HOTY;
	if ( tx < box.origin.x || tx >= box.corner.x ||
	     ty < box.origin.y || ty >= box.corner.y )
		return -1;
	if ( ty < box.origin.y + MNU_TOP )
		return -1;			/* in the top margin: no item */
	it = (ty - box.origin.y - MNU_TOP) / itemh;
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

/* Dialog-kit helpers (defined below, with the dialog section) that the menu
 * shares.  dlg_save MUST be declared before use: it returns a pointer, and an
 * implicit-int declaration would truncate it to a 16-bit word. */
static int	*dlg_save();
static int	dlg_down();
static int	dlg_border();

/* Pop up a menu of n text items at (mx0,my0); track the pointer in a nested
 * command-pipe loop (client draws are frozen meanwhile); return the selected
 * index or -1.  The pixels under the box are saved and restored, so whatever was
 * there -- desktop or a window -- reappears intact. */
srvmenu(items, n, mx0, my0)
char *items[];
{
	RECT box;
	int i, w, itemh, boxw, boxh, sel, last, it, wpl;
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
	/* Place the box at the MENU CURSOR'S TIP, not at the raw pointer position:
	 * the sprite is about to become MNU_MOUSE, whose point is (MNU_HOTX,
	 * MNU_HOTY) into its cell, and mnu_item() hit-tests that same tip. */
	box.origin.x = (mx0 + MNU_HOTX) & ~0x0f;	/* word-align for row copy */
	box.origin.y = my0 + MNU_HOTY;
	if ( box.origin.x + boxw > XMAX ) box.origin.x = (XMAX - boxw) & ~0x0f;
	if ( box.origin.y + boxh > YMAX ) box.origin.y = YMAX - boxh;
	if ( box.origin.x < 0 ) box.origin.x = 0;
	if ( box.origin.y < 0 ) box.origin.y = 0;
	box.corner.x = box.origin.x + boxw;
	box.corner.y = box.origin.y + boxh;

	buf = dlg_save(box, &wpl, OV_MENU);

	srvfill(box, 0, L_TRUE);			/* white body */
	dlg_border(box);
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
		if ( !getcmd(&c) )
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

	dlg_down(box, buf, wpl);
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, DEF_MOUSE);	/* restore arrow */
	return sel;
}

/* ------------------------------------------------------------------ */
/* server-internal modal dialog (message + up to 3 buttons)           */
/* ------------------------------------------------------------------ */

/* 1-px black border around rect r (four srvfill strips, srvmenu's idiom). */
static
dlg_border(r)
RECT r;
{
	RECT e;

	e = r; e.corner.y = r.origin.y + 1;   srvfill(e, 0, L_FALSE);
	e = r; e.origin.y = r.corner.y - 1;   srvfill(e, 0, L_FALSE);
	e = r; e.corner.x = r.origin.x + 1;   srvfill(e, 0, L_FALSE);
	e = r; e.origin.x = r.corner.x - 1;   srvfill(e, 0, L_FALSE);
}

/* Window-style stepped drop shadow: r is the whole rect INCLUDING the d-px
 * shadow margin (right + bottom), so the card it falls from is r minus that.
 * The same pseudo-3D bands outline() steps in for a window (gfx/layer.c),
 * drawn with srvfill because a dialog is an overlay, not a layer.  Depth is a
 * parameter so the dialog card (WD_SHADOW) and its buttons (DLG_BSHAD, a
 * mini version of the same shadow) share one routine and one look. */
static
dlg_shadow(r, d)
RECT r;
{
	RECT card, e;
	int k;

	card = r;
	card.corner.x -= d;
	card.corner.y -= d;
	e = r;  e.origin.x = card.corner.x;  srvfill(e, 0, L_TRUE);
	e = r;  e.origin.y = card.corner.y;  srvfill(e, 0, L_TRUE);
	for ( k = 0; k < d; k++ )
	{
		e = r;					/* right band */
		e.origin.x = card.corner.x + k;   e.corner.x = e.origin.x + 1;
		e.origin.y = r.origin.y + k;
		e.corner.y = r.corner.y - (d - 1) + k;
		srvfill(e, 0, L_FALSE);
		e = r;					/* bottom band */
		e.origin.y = card.corner.y + k;   e.corner.y = e.origin.y + 1;
		e.origin.x = r.origin.x + k;
		e.corner.x = r.corner.x - (d - 1) + k;
		srvfill(e, 0, L_FALSE);
	}
}

/* Which button rect (0..nb-1) the pointer is in, or -1.  The arrow cursor's
 * hotspot is its top-left (0,0), so raw (mx,my) is what the user aims with --
 * unlike the menu's tip-offset sprite. */
static
dlg_bhit(brc, nb)
RECT brc[];
{
	int i;

	for ( i = 0; i < nb; i++ )
		if ( mx >= brc[i].origin.x && mx < brc[i].corner.x &&
		     my >= brc[i].origin.y && my < brc[i].corner.y )
			return i;
	return -1;
}

/* XOR-invert button i's interior (self-erasing pressed-state highlight). */
static
dlg_binvert(brc, i)
RECT brc[];
{
	RECT e;

	e = brc[i];
	e.origin.x += 1;  e.origin.y += 1;
	e.corner.x -= 1;  e.corner.y -= 1;
	gfx_cursor_hide();
	srvfill(e, 0, L_NDST);
	gfx_cursor_show();
}

/* Centre a bw x bh dialog on screen (x word-aligned for the row save/restore)
 * and give it the window-style drop-shadow margin: *pbox is what gets saved
 * and painted (card + shadow), *pcard the card the chrome is drawn on. */
static
dlg_place(bw, bh, pbox, pcard)
RECT *pbox, *pcard;
{
	pbox->origin.x = ((XMAX - bw - WD_SHADOW) / 2) & ~0x0f;
	pbox->origin.y = (YMAX - bh - WD_SHADOW) / 2;
	if ( pbox->origin.x < 0 ) pbox->origin.x = 0;
	if ( pbox->origin.y < 0 ) pbox->origin.y = 0;
	pbox->corner.x = pbox->origin.x + bw + WD_SHADOW;
	pbox->corner.y = pbox->origin.y + bh + WD_SHADOW;
	*pcard = *pbox;
	pcard->corner.x -= WD_SHADOW;
	pcard->corner.y -= WD_SHADOW;
}

/* Paint one dialog button: border, mini drop shadow, centred label.  The
 * +1,+1: the FUI glyphs sit high-left in their cells, so plain integer
 * centring lands the label a pixel off (see memory: recurring). */
static
dlg_button(r, label)
RECT r;
char *label;
{
	RECT s;

	dlg_border(r);
	s = r;
	s.corner.x += DLG_BSHAD;
	s.corner.y += DLG_BSHAD;
	dlg_shadow(s, DLG_BSHAD);
	srvmenuglyphs(SHM_FUI,
		      r.origin.x + (r.corner.x - r.origin.x -
				    strlen(label) * hr_font(SHM_FUI)->cellw) / 2 + 1,
		      r.origin.y + (r.corner.y - r.origin.y -
				    hr_font(SHM_FUI)->cellh) / 2 + 1,
		      label, r);
}

/* Put a dialog/menu box up: save the pixels under it (NULL on malloc failure
 * -- tolerated, the take-down repaints instead) and enter the overlay bracket
 * with freeze flag `ov'.  Freeze clients (the box is a transient overlay, NOT
 * a layer, so a client's clip can't exclude it and it would otherwise paint
 * over it -- the flood-covers-the-menu bug) BEFORE taking the lock, so a
 * client that checks the flag while holding the lock sees it; take the lock
 * around the save+paint so any in-flight client primitive finishes first and
 * the box lands on top of it.  Hide the cursor BEFORE saving, or its
 * sprite is captured into the buffer and painted back on restore, leaving a
 * stray arrow.  Returns with the lock held and the cursor hidden -- the
 * caller paints, then closes the bracket itself (gfx_cursor_show +
 * srvunlock). */
static int *
dlg_save(box, pwpl, ov)
RECT box;
int *pwpl;
{
	int rows, wpl, yy;
	int *buf;

	rows = box.corner.y - box.origin.y;
	wpl = words_between(box.origin.x, box.corner.x);
	buf = (int *)malloc(wpl * rows * 2);
	hr_glob()->overlay = ov;
	srvlock();
	gfx_cursor_hide();
	if ( buf )
		for ( yy = 0; yy < rows; yy++ )
			mnu_wcpy(screen_addr(box.origin.x, box.origin.y + yy),
				 buf + yy * wpl, wpl);
	*pwpl = wpl;
	return buf;
}

/* The srvdialog/srvswitch arm/track loop over nb rects: a left press inside
 * rect i arms and XOR-inverts it, dragging out disarms, and only a release
 * inside the armed rect commits -- a slip of the mouse commits nothing.
 * MOUSE ONLY and modal (keys and clients wait).  The opening click's release
 * arrives first and commits nothing: press == -1 until a press lands inside.
 * Tracks with the plain arrow (its hotspot is already the aim point; no
 * sprite swap, unlike the menu).  Returns the committed rect's index. */
static
dlg_track(brc, nb)
RECT brc[];
{
	int sel, press, in, hit;
	WMSG c;

	sel = -1;  press = -1;  in = 0;
	while ( sel < 0 )
	{
		if ( !getcmd(&c) )
			continue;
		if ( c.wm_type != C_INPUT || c.wm_arg[0] == IN_KEY )
			continue;
		mx = c.wm_arg[1];
		my = c.wm_arg[2];
		SM_Mouse_Pos.x = mx;  SM_Mouse_Pos.y = my;
		hit = dlg_bhit(brc, nb);
		if ( c.wm_arg[0] == IN_BUTTON )
		{
			if ( c.wm_arg[4] & c.wm_arg[3] & SM_LFT )
			{			/* left press: arm */
				if ( hit >= 0 )
				{
					press = hit;
					in = 1;
					dlg_binvert(brc, press);
				}
			}
			else if ( (c.wm_arg[4] & SM_LFT) &&
				  !(c.wm_arg[3] & SM_LFT) )
			{			/* left release: commit or disarm */
				if ( press >= 0 )
				{
					if ( in )
						dlg_binvert(brc, press);
					if ( hit == press )
						sel = press;
					press = -1;  in = 0;
				}
			}
		}
		else if ( press >= 0 && (hit == press) != in )
		{				/* motion: track in/out of the arm */
			dlg_binvert(brc, press);
			in = !in;
		}
	}
	return sel;
}

/* Take a dialog/menu box down: restore the saved pixels and lift the overlay
 * freeze (clients full-repaint).  When the save-under malloc had failed the
 * box is still on screen -- exactly the "menu never goes away" bug -- so
 * repaint the desktop over it and ask the windows it overlapped to repaint
 * (outside the lock: sendev may block), which guarantees it is dismissed. */
static
dlg_down(box, buf, wpl)
RECT box;
int *buf;
{
	int rows, yy;

	rows = box.corner.y - box.origin.y;
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
		background(box);
	gfx_cursor_show();
	srvunlock();
	hr_glob()->overlay = 0;
	if ( !buf )		/* no save-under: re-expose what the box covered */
		expose_covered(-1, box.origin.x, box.origin.y,
			       box.corner.x, box.corner.y);
}

/* The server's own modal dialog: a centred box with a message (msg; '|' breaks
 * it into up to 4 lines) and 1..3 buttons (b1/b2 may be NULL), drawn with the
 * same save-under / overlay-freeze bracket as srvmenu and tracked in the same
 * nested command-pipe loop.  MOUSE ONLY (keys are ignored, like the menu);
 * classic arm/track buttons: a left press INSIDE a button arms and inverts it,
 * dragging out disarms, and only a release inside the armed button commits --
 * a release anywhere else leaves the dialog up (it is modal; every caller
 * provides a safe button, so there is no escape hatch to need).  Returns the
 * committed button's index (0-based).  Chrome metrics come from hrdlg.h so a
 * server confirmation looks exactly like a client dialog. */
srvdialog(msg, b0, b1, b2)
char *msg, *b0, *b1, *b2;
{
	RECT box, card, brc[3];
	char mbuf[128];
	char *lines[4], *bl[3];
	int nl, nb, fw, fh, i, w, lw, bw, boxw, boxh;
	int wpl, bx, by, sel;
	int *buf;

	fw = hr_font(SHM_FUI)->cellw;
	fh = hr_font(SHM_FUI)->cellh;

	/* split the message on '|' (into a local copy: we plant NULs) */
	strncpy(mbuf, msg, sizeof(mbuf) - 1);
	mbuf[sizeof(mbuf) - 1] = 0;
	nl = 0;
	lines[nl++] = mbuf;
	for ( i = 0; mbuf[i] && nl < 4; i++ )
		if ( mbuf[i] == '|' )
		{
			mbuf[i] = 0;
			lines[nl++] = &mbuf[i + 1];
		}

	bl[0] = b0;  bl[1] = b1;  bl[2] = b2;
	nb = 1 + (b1 != 0) + (b1 != 0 && b2 != 0);

	/* box size: whichever is wider, the message block or the button row */
	w = 0;
	for ( i = 0; i < nl; i++ )
	{
		lw = strlen(lines[i]) * fw;
		if ( lw > w ) w = lw;
	}
	bw = 0;
	for ( i = 0; i < nb; i++ )
		bw += strlen(bl[i]) * fw + 2 * DLG_BTNPAD;
	bw += (nb - 1) * DLG_GAPX + DLG_BSHAD;	/* the row's own drop shadow */
	if ( bw > w ) w = bw;
	boxw = w + 2 * DLG_MARG;
	boxh = DLG_MARG + nl * fh + DLG_GAPY + DLG_BTNH + DLG_BSHAD + DLG_MARG;

	dlg_place(boxw, boxh, &box, &card);

	/* button rects: a centred row along the bottom */
	by = box.origin.y + DLG_MARG + nl * fh + DLG_GAPY;
	bx = box.origin.x + (boxw - bw) / 2;
	for ( i = 0; i < nb; i++ )
	{
		brc[i].origin.x = bx;
		brc[i].origin.y = by;
		brc[i].corner.x = bx + strlen(bl[i]) * fw + 2 * DLG_BTNPAD;
		brc[i].corner.y = by + DLG_BTNH;
		bx = brc[i].corner.x + DLG_GAPX;
	}

	buf = dlg_save(box, &wpl, OV_MENU);

	srvfill(card, 0, L_TRUE);			/* white body */
	dlg_border(card);
	dlg_shadow(box, WD_SHADOW);
	for ( i = 0; i < nl; i++ )
		srvmenuglyphs(SHM_FUI,
			      card.origin.x + (boxw - strlen(lines[i]) * fw) / 2,
			      card.origin.y + DLG_MARG + i * fh, lines[i], card);
	for ( i = 0; i < nb; i++ )
		dlg_button(brc[i], bl[i]);
	gfx_cursor_show();
	srvunlock();		/* painted; clients stay frozen via overlay */

	sel = dlg_track(brc, nb);
	dlg_down(box, buf, wpl);
	return sel;
}

/* The desktop-menu "Switch to" dialog: a centred card listing every open
 * window BY NAME -- one full-width SHM_FUI text row per window, above a
 * Cancel button.  No icons: the 48px artwork lives on disk (srvicon opens
 * /usr/hr/icons/<name> per call), and one file read per window under the
 * overlay freeze is an unacceptable cost.  The rows track exactly like
 * srvdialog's buttons -- a left press inside a row arms and XOR-inverts it
 * (the alt-tab highlight), dragging out disarms, and only a release inside
 * the armed row commits -- so a slip of the mouse cannot switch windows.
 * The chosen window is brought forward (restorewin if minimised, else
 * raisewin) only AFTER the dialog is taken down: both repaint and sendev,
 * neither of which may happen under the overlay freeze. */
#define SW_ROWPAD	3	/* text pad above/below a row's label */
srvswitch()
{
	RECT box, card, rc[MAX_WINDOWS + 1];
	int wid[MAX_WINDOWS];
	int nw, fw, fh, rowh, i, w, lw, boxw, boxh, btnw;
	int wpl, ry, sel;
	int *buf;

	nw = 0;
	for ( i = 0; i < MAX_WINDOWS; i++ )
		if ( wins[i].used )
			wid[nw++] = i;
	if ( nw == 0 )
	{
		srvdialog("No windows are open.", "OK", (char *)0, (char *)0);
		return;
	}

	fw = hr_font(SHM_FUI)->cellw;
	fh = hr_font(SHM_FUI)->cellh;
	rowh = fh + 2 * SW_ROWPAD;

	/* box size: the widest name row or the Cancel button, whichever wins */
	w = 0;
	for ( i = 0; i < nw; i++ )
	{
		lw = strlen(wins[wid[i]].title) * fw + 2 * DLG_BTNPAD;
		if ( lw > w ) w = lw;
	}
	btnw = strlen("Cancel") * fw + 2 * DLG_BTNPAD;
	if ( btnw + DLG_BSHAD > w ) w = btnw + DLG_BSHAD;
	boxw = w + 2 * DLG_MARG;
	boxh = DLG_MARG + nw * rowh + DLG_GAPY + DLG_BTNH + DLG_BSHAD + DLG_MARG;

	dlg_place(boxw, boxh, &box, &card);

	/* row rects span the card's usable width -- the whole row is the click
	 * target AND the invert highlight; rc[nw] is the Cancel button so one
	 * dlg_bhit/dlg_binvert pass covers rows and button alike */
	ry = box.origin.y + DLG_MARG;
	for ( i = 0; i < nw; i++ )
	{
		rc[i].origin.x = box.origin.x + DLG_MARG;
		rc[i].origin.y = ry + i * rowh;
		rc[i].corner.x = box.origin.x + boxw - DLG_MARG;
		rc[i].corner.y = rc[i].origin.y + rowh;
	}
	ry += nw * rowh;
	rc[nw].origin.x = box.origin.x + (boxw - btnw - DLG_BSHAD) / 2;
	rc[nw].origin.y = ry + DLG_GAPY;
	rc[nw].corner.x = rc[nw].origin.x + btnw;
	rc[nw].corner.y = rc[nw].origin.y + DLG_BTNH;

	buf = dlg_save(box, &wpl, OV_MENU);

	srvfill(card, 0, L_TRUE);			/* white body */
	dlg_border(card);
	dlg_shadow(box, WD_SHADOW);
	/* each row: the title centred (+1,+1: FUI glyphs sit high-left in the
	 * cell).  Drawn straight from wins[]: titles are stable while the
	 * dialog is up -- kills only happen in the main loop. */
	for ( i = 0; i < nw; i++ )
	{
		lw = strlen(wins[wid[i]].title) * fw;
		srvmenuglyphs(SHM_FUI,
			      rc[i].origin.x +
			      (rc[i].corner.x - rc[i].origin.x - lw) / 2 + 1,
			      rc[i].origin.y + SW_ROWPAD + 1,
			      wins[wid[i]].title, card);
	}
	dlg_button(rc[nw], "Cancel");
	gfx_cursor_show();
	srvunlock();		/* painted; clients stay frozen via overlay */

	sel = dlg_track(rc, nw + 1);	/* nw rows + the Cancel button */
	dlg_down(box, buf, wpl);

	if ( sel >= 0 && sel < nw )
	{
		w = wid[sel];
		if ( wins[w].used )		/* kills only happen in the main
						 * loop, but guard anyway */
		{
			if ( wins[w].min )
				restorewin(w);
			else
				raisewin(w);
		}
	}
}

/* ------------------------------------------------------------------ */
/* client dialog overlay (wire.h C_DLGOPEN / C_DLGCLOSE)              */
/* ------------------------------------------------------------------ */

/* Open requests taken from the pipe.  QUEUED, never acted on inline: a
 * C_DLGOPEN can arrive while a nested tracker (menu, srvdialog, drag) owns
 * the pipe, or while another dialog is up -- the main loop drains this when
 * the desktop is quiescent, exactly like connq.  Small on purpose: unlike a
 * dropped connect, a full queue can ANSWER (refuse) instead of dropping, so
 * no client is ever left waiting. */
#define NDLGQ	4
static HRDLGO	dlgq[NDLGQ];
static int	ndlgq;

/* Publish / retract the dialog-interior surface (shmem.h SHM_DLGSURF), under
 * the same seqlock discipline as publish_surf.  Topmost and not a layer, so
 * it is always one fully visible rect. */
static
publish_dlg(on)
{
	HRSURF *sp;

	sp = hr_dlgsurf();
	sp->seq++;					/* odd: writing */
	sp->mapped = on;
	if ( on )
	{
		sp->ox = dlgint.origin.x;  sp->oy = dlgint.origin.y;
		sp->cw = dlgint.corner.x - dlgint.origin.x;
		sp->ch = dlgint.corner.y - dlgint.origin.y;
		sp->nvis = 1;
		sp->vis[0].x0 = dlgint.origin.x;  sp->vis[0].y0 = dlgint.origin.y;
		sp->vis[0].x1 = dlgint.corner.x;  sp->vis[0].y1 = dlgint.corner.y;
	}
	else
		sp->nvis = 0;
	sp->seq++;					/* even: done */
}

/* Send a dialog event to the owner, with the DEAD-OWNER BACKSTOP: nothing in
 * this system reaps a silently killed client (kill(pid,0) is EINVAL on this
 * kernel), and a dialog whose owner is gone would swallow the very menus that
 * could clear it.  A live owner is BLOCKED on this ring, so consecutive
 * full-ring puts mean it is not draining -- presume it dead and close. */
#define DLGDEAD	64
static
dlgsend(type, a0, a1, a2, a3)
{
	WMSG e;

	e.wm_type = type;
	e.wm_wid = dlgwid;
	e.wm_arg[0] = a0; e.wm_arg[1] = a1;
	e.wm_arg[2] = a2; e.wm_arg[3] = a3;
	e.wm_arg[4] = e.wm_arg[5] = 0;
	if ( hr_evput(dlgwid, (short *)&e) < 0 )
	{
		if ( ++dlgput >= DLGDEAD )
		{
			srvlogs("dialog owner dead: closing\n");
			dlgclose();
		}
	}
	else
		dlgput = 0;
}

/* Close the dialog: restore the pixels, retract the surface, unfreeze, then
 * deliver whatever the modal phase deferred.  Idempotent (killwin and the
 * dead-owner backstop can race the owner's own C_DLGCLOSE). */
dlgclose()
{
	int w, yy, saved;

	if ( dlgwid < 0 )
		return;
	srvlock();
	gfx_cursor_hide();
	saved = (dlgsave != (int *)0);
	if ( saved )
	{
		for ( yy = 0; yy < dlgrows; yy++ )
			mnu_wcpy(dlgsave + yy * dlgwpl,
				 screen_addr(dlgbox.origin.x, dlgbox.origin.y + yy),
				 dlgwpl);
		free((char *)dlgsave);
		dlgsave = (int *)0;
	}
	else
		background(dlgbox);
	gfx_cursor_show();
	srvunlock();
	publish_dlg(0);
	hr_glob()->overlay = 0;
	w = dlgwid;
	dlgwid = -1;
	dlggrab = 0;
	/* A full-content expose to the owner: its dialog library discarded every
	 * event that arrived while it was modal, and this one repaint subsumes
	 * them all.  With a pixel-exact restore nothing else changed under the box
	 * (clients were frozen, kills and connects deferred); without one, repaint
	 * whatever the box covered the hard way. */
	sendev(w, E_EXPOSE, 0, 0, hr_surf(w)->cw, hr_surf(w)->ch);
	if ( !saved )
	{
		expose_covered(-1, dlgbox.origin.x, dlgbox.origin.y,
			       dlgbox.corner.x, dlgbox.corner.y);
		redraw_icons();		/* the box may have covered desktop icons */
	}
	/* C_BYEs deferred while the box was up (killwin repaints): honour them. */
	while ( dlgbye )
		for ( yy = 0; yy < MAX_WINDOWS; yy++ )
			if ( dlgbye & (1 << yy) )
			{
				dlgbye &= ~(1 << yy);
				killwin(yy);
			}
}

/* Serve one queued open request.  Refusals answer E_DLGOPEN 0 -- every
 * request gets exactly one answer, because the requester blocks for it. */
static
dodlgopen(dp)
HRDLGO *dp;
{
	int wid, iw, ih, bw, bh;
	RECT card;

	wid = dp->hd_wid;
	if ( wid < 0 || wid >= MAX_WINDOWS || !wins[wid].used )
		return;				/* died while queued: no ring to answer */
	if ( dlgwid >= 0 )
	{
		sendev(wid, E_DLGOPEN, 0, 0, 0, 0);	/* one at a time */
		return;
	}
	iw = dp->hd_w;
	ih = dp->hd_h;
	if ( iw < 64 ) iw = 64;
	if ( ih < 32 ) ih = 32;
	if ( iw > XMAX - 32 ) iw = XMAX - 32;
	if ( ih > YMAX - 32 ) ih = YMAX - 32;
	/* untitled card (1px border) + the window-style drop-shadow margin; the
	 * saved box covers card AND shadow */
	bw = iw + 2;
	bh = ih + 2;
	dlg_place(bw, bh, &dlgbox, &card);
	dlgint.origin.x = card.origin.x + 1;
	dlgint.origin.y = card.origin.y + 1;
	dlgint.corner.x = dlgint.origin.x + iw;
	dlgint.corner.y = dlgint.origin.y + ih;

	/* An open pointer grab cannot outlive the freeze: its client would wait
	 * forever for the release the modal phase swallows.  Synthesize it, the
	 * way the menu path does. */
	if ( grabwid >= 0 )
	{
		sendev(grabwid, E_BUTTON, lastgx, lastgy, 0, SM_LFT);
		grabwid = -1;
		lastgx = lastgy = -1;
	}

	/* Save/restore geometry from the BOX, never from the card: the box is
	 * what gets painted (card + drop shadow), so it is what must be saved
	 * and put back.  A card-height row count left the shadow's rows unsaved
	 * -- and so still on the desktop after the dialog closed.  The dlgclose
	 * restore needs dlgrows too, so keep it alongside dlg_save's wpl.  The
	 * owner is exempted from the OV_DLG|wid freeze, but only for the dialog
	 * surface -- published below, after the frame is painted. */
	dlgrows = dlgbox.corner.y - dlgbox.origin.y;
	dlgsave = dlg_save(dlgbox, &dlgwpl, OV_DLG | wid);
	srvfill(card, 0, L_TRUE);			/* white body */
	dlg_border(card);
	dlg_shadow(dlgbox, WD_SHADOW);
	gfx_cursor_show();
	srvunlock();

	publish_dlg(1);
	dlgwid = wid;
	dlggrab = 0;
	dlgput = 0;
	dlastx = dlasty = -1;
	srvlogn("dlgopen wid ", wid);
	sendev(wid, E_DLGOPEN, 1, iw, ih, 0);
}

/* Serve the opens taken while a tracker owned the pipe (main loop, and only
 * while no dialog is up). */
draindlg()
{
	HRDLGO q[NDLGQ];
	int i, n;

	if ( (n = ndlgq) == 0 )
		return;
	for ( i = 0; i < n; i++ )	/* copy out first: nothing here recurses */
		q[i] = dlgq[i];		/* today, but connq's discipline is cheap */
	ndlgq = 0;
	for ( i = 0; i < n; i++ )
	{
		dodlgopen(&q[i]);
		if ( dlgwid >= 0 )
		{
			/* One opened: the rest must wait for it to close.  Put
			 * them back so the next drain (after dlgclose) serves
			 * them rather than refusing them. */
			while ( ++i < n )
				dlgq[ndlgq++] = q[i];
			break;
		}
	}
}

/* ALL input while a client dialog is up.  System-modal: nothing reaches the
 * windows, the icons, the menus or the title bars -- a left press inside the
 * interior opens the dialog's own grab (E_DBUTTON/E_DMOTION, interior coords,
 * UNCLAMPED so a button widget sees the pointer leave it), keys go to the
 * owner as E_DKEY, and everything else is swallowed. */
static
dlginput(c)
WMSG *c;
{
	int ix, iy, down, changed;

	if ( c->wm_arg[0] == IN_MOVE )
	{
		mx = c->wm_arg[1];
		my = c->wm_arg[2];
		hr_glob()->curx = mx;	/* cursor-overlap gating must stay live */
		hr_glob()->cury = my;
		SM_Mouse_Pos.x = mx;
		SM_Mouse_Pos.y = my;
		if ( dlggrab )
		{
			ix = mx - dlgint.origin.x;
			iy = my - dlgint.origin.y;
			if ( ix != dlastx || iy != dlasty )
			{
				dlastx = ix;  dlasty = iy;
				dlgsend(E_DMOTION, ix, iy, SM_LFT, 0);
			}
		}
	}
	else if ( c->wm_arg[0] == IN_BUTTON )
	{
		mx = c->wm_arg[1];
		my = c->wm_arg[2];
		SM_Mouse_Pos.x = mx;
		SM_Mouse_Pos.y = my;
		down = c->wm_arg[3];
		changed = c->wm_arg[4];
		ix = mx - dlgint.origin.x;
		iy = my - dlgint.origin.y;
		if ( changed & down & SM_LFT )
		{			/* left press: only inside the interior */
			if ( mx >= dlgint.origin.x && mx < dlgint.corner.x &&
			     my >= dlgint.origin.y && my < dlgint.corner.y )
			{
				dlggrab = 1;
				dlastx = ix;  dlasty = iy;
				dlgsend(E_DBUTTON, ix, iy, down, changed);
			}
		}
		else if ( (changed & SM_LFT) && !(down & SM_LFT) )
		{			/* left release: always ends the grab */
			if ( dlggrab )
			{
				dlggrab = 0;
				dlgsend(E_DBUTTON, ix, iy, down, changed);
			}
		}
		/* middle / right presses: swallowed (no paste, no menus) */
	}
	else if ( c->wm_arg[0] == IN_KEY )
		dlgsend(E_DKEY, c->wm_arg[1], 0, 0, 0);
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

/* The drag-tracking half of a move/resize: the LEFT button is already DOWN (the
 * grab exists), the hand cursor is up, and (mx,my) is where the grab took hold.
 * Drag the XOR ghost while the button is HELD; release commits -- movewin for a
 * move (skipped when the ghost never left home, so a plain click on the title
 * bar costs no repaint), resizewin for a stretch -- and the arrow comes back.
 * Move translates the whole frame; resize drags the bottom-right corner. */
dodrag(wid, mode)
{
	RECT r, g;
	int w, h, gx, gy;
	WMSG c;

	r = wtbl[wid]->wn_Layer->rect;
	g = r;
	w = r.corner.x - r.origin.x;
	h = r.corner.y - r.origin.y;
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
		if ( !getcmd(&c) || c.wm_type != C_INPUT || c.wm_arg[0] == IN_KEY )
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
			cx = mx + MOV_HOTX;		/* the hand's grip, not its corner */
			cy = my + MOV_HOTY;
			if ( cx < r.origin.x + 48 ) cx = r.origin.x + 48;
			if ( cy < r.origin.y + 48 ) cy = r.origin.y + 48;
			if ( cx > XMAX ) cx = XMAX;
			if ( cy > YMAX ) cy = YMAX;
			g.corner.x = cx;  g.corner.y = cy;
		}
		ghostframe(g);					/* draw new */
		gfx_cursor_show();
	}

	if ( mode == 1 )
	{
		if ( g.origin.x != r.origin.x || g.origin.y != r.origin.y )
			movewin(wid, g.origin.x, g.origin.y);
	}
	else
		resizewin(wid, g.corner.x, g.corner.y);
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, DEF_MOUSE);	/* restore arrow */
}

/* Interactively move (mode 1) or resize (mode 2) window wid with an XOR ghost
 * frame -- faithful to the original desktop (main.c fmove/fstretch: dnLwait then
 * upLwait(ghost)).  After the menu picks Move/Stretch the pointer button is up;
 * press the LEFT button to grab, drag the ghost while HELD, and release to
 * commit.  A RIGHT/MIDDLE press instead cancels. */
ghostdrag(wid, mode)
{
	WMSG c;

	if ( !wtbl[wid] || !wtbl[wid]->wn_Layer )
		return;

	/* Switch to the move/resize "hand" cursor the moment the item is chosen
	 * (original fmove sets MOV_MOUSE before waiting for the grab), so the shape
	 * itself signals the mode and the pending left-button grab. */
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, MOV_MOUSE);

	/* Wait for the grab: LEFT down grabs, RIGHT/MIDDLE down cancels (dnLwait). */
	for (;;)
	{
		if ( !getcmd(&c) || c.wm_type != C_INPUT || c.wm_arg[0] == IN_KEY )
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
	dodrag(wid, mode);
}

/* A LEFT press landed on window wid's TITLE BAR: move the window by dragging it
 * directly, no menu round-trip.  The press itself is the grab -- the button is
 * already down when we get here -- so show the "hand" cursor at once and go
 * straight into the ghost drag; the release drops the window (and a press with
 * no drag is just the click-to-raise it always was, committing nothing). */
titledrag(wid)
{
	if ( !wtbl[wid] || !wtbl[wid]->wn_Layer )
		return;
	if ( curfd >= 0 ) ioctl(curfd, CIOMOUSE, MOV_MOUSE);
	dodrag(wid, 1);
}

/* Quit the whole window manager (desktop-menu "Quit"): tell every client to
 * exit and kill it (so it releases /dev/dmgr), stop the input pump, reap all
 * children, then unload the hi-res driver -- which restores the kernel text
 * console -- and return to the shell that launched us. */
quitwm()
{
	int w, pid, st;

	hr_glob()->magic = 0;	/* session over: anything the kills below miss
				 * (a client mid-fork, a stray pump) exits at
				 * its next hr_evwait instead of lingering */
	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used )
		{
			sendev(w, E_QUIT, 0, 0, 0, 0);
			if ( wins[w].pid > 0 )
				kill(wins[w].pid, SIGKILL);
			wins[w].used = 0;
		}
	if ( curfd >= 0 )
		ioctl(curfd, CIOMSEOFF, (char *)0);	/* erase the cursor */
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

/* Find a live window launched from catalog entry `ai', or -1.  Matched by the
 * entry it came from, not by title: the title is the application's to choose
 * and need not be the launcher label. */
appwindow(ai)
{
	int w;

	for ( w = 0; w < MAX_WINDOWS; w++ )
		if ( wins[w].used && wins[w].appi == ai )
			return w;
	return -1;
}

/* Reap the window of a client that DIED without saying C_BYE.  A SIGSEGV'd
 * app (memory exhaustion kills this way: a failed stack/heap grow is a hard
 * fault) leaves its window standing for ever -- an empty shell nobody can
 * close, holding a window slot, and if it died mid-primitive its SHM_INDRAW
 * flag wedges every later srvlock into the megaspin drain.  killwin cures all
 * of that (it clears the drain flag first thing), the server just never knew
 * the client was gone: pipes don't say, and the rings never block.  So PROBE:
 * one window per main-loop pass (one kill(pid,0) per input event at worst),
 * round robin, deferred while a dialog overlay is up -- killwin restacks and
 * repaints, which must not land on a box that is not a layer. */
int	pollw;

reapwins()
{
	if ( dlgwid >= 0 )
		return;
	pollw = (pollw + 1) & (MAX_WINDOWS - 1);
	if ( wins[pollw].used && wins[pollw].pid > 0 &&
	     kill(wins[pollw].pid, 0) < 0 )
	{
		srvlogn("client died, reaping window ", pollw);
		killwin(pollw);
	}
}

/* Has catalog entry `ai' been started but not yet shown its window? */
apppending(ai)
{
	int i;

	for ( i = 0; i < MAX_WINDOWS; i++ )
		if ( pends[i].used && pends[i].ai == ai && pendlive(i) )
			return 1;
	return 0;
}

/* Right-click on the empty desktop: menu of launchable apps, a divider,
 * "Switch to..." (the running-window list dialog, srvswitch), another divider,
 * then "Quit" (quit the whole window manager).  Multi-instance apps read
 * "New <name>...", single-instance apps read "<name>...". */
deskmenu(x, y)
{
	char labels[MAX_APPS][24];
	char *items[MAX_APPS + 4];
	int i, n, qidx, swidx, sel;

	n = 0;
	for ( i = 0; i < napps; i++ )
	{
		if ( apps[i].multi )
			sprintf(labels[i], "New %s...", apps[i].name);
		else
			sprintf(labels[i], "%s...", apps[i].name);
		items[n++] = labels[i];
	}
	items[n++] = MNU_DIV;			/* divider after the app list */
	swidx = n;
	items[n++] = "Switch to...";
	items[n++] = MNU_DIV;			/* divider before Quit */
	qidx = n;
	items[n++] = "Quit";
	sel = srvmenu(items, n, x, y);
	if ( sel < 0 )
		return;
	if ( sel == swidx )
		srvswitch();
	else if ( sel == qidx )
	{
		/* Quit here means the whole desktop: every client dies with it,
		 * so a slip of the menu drag must not be enough. */
		if ( srvdialog("Do you want to quit ZView?",
			       "Yes", "No", (char *)0) == 0 )
			quitwm();
	}
	else if ( sel < napps )
	{
		/* single-instance app already open? bring it forward instead of
		 * launching a duplicate -- including one that has been started but
		 * has not put its window up yet (it is on its way; do nothing). */
		int w;
		if ( !apps[sel].multi && (w = appwindow(sel)) >= 0 )
		{
			if ( wins[w].min )
				restorewin(w);
			else
				raisewin(w);
		}
		else if ( apps[sel].multi || !apppending(sel) )
			launchapp(sel, x, y);
	}
}

/* Right-click on a window: per-window operations.  "Front"/"Back" raise/lower;
 * "Quit" closes that window (vs. the desktop menu's Quit = quit the WM).
 *
 * Above those come whatever the client declared in its connect record (wire.h
 * hc_menu, HRM_*), in HRM_ bit order, separated from them by a divider.  The
 * labels live here rather than in the client because a fixed vocabulary is the
 * whole point: "Save" reads the same and sits in the same place in every
 * application.  The server does not act on those entries -- it sends the client
 * an E_MENU carrying the bit and forgets about it. */
char	*g_winitems[] = { "Move", "Stretch", "Front", "Back", "Hide", "Quit" };
#define NWINITEMS	6

/* Labels for the HRM_* bits, LSB first: entry i is bit (1 << i). */
char	*g_appitems[] = { "New", "Open", "Save", "Cut", "Copy", "Paste",
			  "Settings", "Help" };
#define NAPPITEMS	8

winmenu(w, x, y)
{
	char *items[NAPPITEMS + 2 + NWINITEMS];
	int act[NAPPITEMS + 2 + NWINITEMS];
	int i, n, sel, mbits;

	/* The client's own entries first.  act[] tells the two kinds apart: >= 0
	 * is an index into g_winitems, < 0 is -(bit index + 1). */
	n = 0;
	mbits = wins[w].menu;
	for ( i = 0; i < NAPPITEMS; i++ )
		if ( mbits & (1 << i) )
		{
			act[n] = -(i + 1);
			items[n++] = g_appitems[i];
		}
	if ( n > 0 )
	{
		act[n] = 0;			/* never looked at: a divider */
		items[n++] = MNU_DIV;		/* selects nothing (srvmenu)  */
	}

	/* "Stretch" only for a client that said it can be resized (HRF_STRETCH in
	 * its connect record): offering it for a fixed-size window would just send
	 * an E_RESIZE the app cannot honour.
	 *
	 * "Quit" is fenced off behind a divider of its own, like the desktop menu's
	 * Quit: it is the one entry here that destroys something, so it should not
	 * sit flush against Hide where a slightly long drag lands on it. */
	for ( i = 0; i < NWINITEMS; i++ )
	{
		if ( i == 1 && !wins[w].stretch )
			continue;
		if ( i == NWINITEMS - 1 )
		{
			act[n] = 0;		/* never looked at: a divider */
			items[n++] = MNU_DIV;
		}
		act[n] = i;
		items[n++] = g_winitems[i];
	}
	sel = srvmenu(items, n, x, y);
	if ( sel < 0 || sel >= n )
		return;
	sel = act[sel];
	if ( sel < 0 )		/* one of the client's own: just route it there */
	{
		sendev(w, E_MENU, 1 << (-sel - 1), 0, 0, 0);
		return;
	}
	if ( sel == 0 )      ghostdrag(w, 1);
	else if ( sel == 1 ) ghostdrag(w, 2);
	else if ( sel == 2 ) raisewin(w);
	else if ( sel == 3 ) backwin(w);
	else if ( sel == 4 ) minwin(w);
	else if ( sel == 5 )
	{
		/* An app that declared HRF_CONFIRM holds live state behind this
		 * window (a shell with running jobs), so ask first. */
		if ( wins[w].confirm )
		{
			char qb[48];	/* title is <= 24 incl NUL */

			sprintf(qb, "Do you want to quit %s?", wins[w].title);
			if ( srvdialog(qb, "Yes", "No", (char *)0) != 0 )
				return;
		}
		killwin(w);
	}
}

/* Is the pointer (mx,my) on desktop-icon slot `slot'?  One test for both icon
 * kinds (minimised windows and app placeholders), pulled out of handlebtn --
 * icons may occupy several rows, so each one's own cell is tested rather than
 * a single row band. */
static
iconhit(slot)
{
	return mx >= iconx(slot) - ICONPAD &&
	       mx <  iconx(slot) + ICONW + ICONPAD &&
	       my >= icony(slot) - 2 &&
	       my <  icony(slot) + ICONW + ICONLH + 2;
}

/* A pointer button changed.  Right = pop up the desktop (launcher) or window
 * menu; left = click-to-raise, and inside a window's content it also starts the
 * pointer grab that carries the selection gesture to the client; middle = paste
 * into the window under the pointer.  A click on a dock icon restores that
 * window (temporary until the desktop-icon phase).
 *
 * Unlike before, RELEASES are seen here too -- a grab has to end somewhere, and
 * the client needs the release to know the drag finished.  Everything that acted
 * on a press still tests (changed & down) explicitly, so the window-management
 * behaviour is unchanged. */
handlebtn(c)
WMSG *c;
{
	int down, changed, w, cx, cy;
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

	/* LEFT RELEASE: end the grab, and hand the client the release that closes
	 * its gesture.  Done before anything else and regardless of where the
	 * pointer now is -- a drag legitimately ends outside the window. */
	if ( (changed & SM_LFT) && !(down & SM_LFT) )
	{
		if ( grabwid >= 0 )
		{
			if ( toclient(grabwid, mx, my, &cx, &cy) )
				sendev(grabwid, E_BUTTON, cx, cy, down, changed);
			else
				sendev(grabwid, E_BUTTON, lastgx, lastgy, down, changed);
			grabwid = -1;
			lastgx = lastgy = -1;
		}
		return;
	}

	if ( !(changed & down) )		/* otherwise act only on a fresh press */
		return;

	/* a minimised-window desktop icon? restore it.  Icons may occupy several
	 * rows, so test each one's own cell rather than a single row band. */
	{
		for ( w = 0; w < MAX_WINDOWS; w++ )
			if ( wins[w].used && wins[w].min &&
			     iconhit(wins[w].islot) )
			{
				restorewin(w);
				return;
			}
	}

	/* an app placeholder icon? launch that catalog entry -- the whole point
	 * of a placeholder is that no process exists until this click.  The
	 * window goes to the catalog's X,Y if given, else it opens at the click.
	 * While a launch is pending (forked, no C_CONNECT yet) further clicks do
	 * nothing, so a double-click does not start two copies -- for a multi
	 * entry too: an INTENDED second copy is a second click a moment later.
	 * The single-instance live-window case is only reachable for an app
	 * started outside the catalog (its window has appi -1, so the
	 * placeholder stayed); surface that window rather than duplicate it,
	 * like the desktop menu does. */
	{
		int i;

		for ( i = 0; i < napps; i++ )
			if ( apps[i].show &&
			     iconhit(apps[i].islot) )
			{
				if ( !apps[i].multi && (w = appwindow(i)) >= 0 )
				{
					if ( wins[w].min )
						restorewin(w);
					else
						raisewin(w);
				}
				else if ( !apppending(i) )
					launchapp(i,
					    apps[i].px >= 0 ? apps[i].px : mx,
					    apps[i].px >= 0 ? apps[i].py : my);
				return;
			}
	}

	if ( changed & down & SM_RGHT )		/* RIGHT: menus */
	{
		/* A menu runs its own nested getcmd() loop and swallows everything,
		 * including the left release that would have ended a grab -- so end it
		 * here, with a synthetic release, or the client would keep extending a
		 * selection for every mouse move after the menu closed. */
		if ( grabwid >= 0 )
		{
			sendev(grabwid, E_BUTTON, lastgx, lastgy, 0, SM_LFT);
			grabwid = -1;
			lastgx = lastgy = -1;
		}
		w = who_top_at(p);
		if ( w >= 0 && w < MAX_WINDOWS && wins[w].used )
			winmenu(w, mx, my);
		else
			deskmenu(mx, my);
		return;
	}
	if ( changed & down & SM_MID )		/* MIDDLE: paste */
	{
		/* Into the window under the pointer, and deliberately WITHOUT
		 * raising it: select in one window, paste into another without
		 * restacking either.  The event carries only the position; the
		 * client reads the bytes from the shared store itself. */
		w = who_top_at(p);
		if ( toclient(w, mx, my, &cx, &cy) )
		{
			sendev(w, E_PASTE, cx, cy, 0, 0);
			/* The gesture is finished, so the highlight has done its job:
			 * drop it in whichever window was showing it.  Only the
			 * HIGHLIGHT goes -- the bytes stay in the shared store and the
			 * same selection can be pasted again -- but leaving a window
			 * lit up after the text has been delivered reads as "this is
			 * still pending", and it makes the next selection's highlight
			 * ambiguous when it lands in the same window. */
			if ( selwid >= 0 )
			{
				sendev(selwid, E_SELCLEAR, 0, 0, 0, 0);
				selwid = -1;
			}
		}
		return;
	}
	if ( changed & down & SM_LFT )		/* LEFT: click-to-raise + grab */
	{
		w = who_top_at(p);
		if ( w >= 0 && w < MAX_WINDOWS && wins[w].used )
		{
			raisewin(w);
			/* A press on the CONTENT belongs to the client (it starts a
			 * selection); a press on the title bar or the shadow is the
			 * window manager's and grabs nothing. */
			if ( toclient(w, mx, my, &cx, &cy) )
			{
				grabwid = w;
				lastgx = cx;  lastgy = cy;
				sendev(w, E_BUTTON, cx, cy, down, changed);
			}
			/* ... and a press on the TITLE BAR picks the window up: drag
			 * to move it, release to drop it (titledrag).  who_top_at()
			 * already put (mx,my) inside the layer rect, so the bar is
			 * just the top strip clear of the right shadow margin. */
			else if ( !wins[w].min && wtbl[w] && wtbl[w]->wn_Layer &&
				  my < wtbl[w]->wn_Layer->rect.origin.y + WD_TITLEH &&
				  mx < wtbl[w]->wn_Layer->rect.corner.x - WD_SHADOW )
				titledrag(w);
		}
	}
}

/* ------------------------------------------------------------------ */
/* command dispatch                                                   */
/* ------------------------------------------------------------------ */

/* Every read of the shared command pipe goes through getcmd().  Records are one
 * WMSG except C_CONNECT, which is three (HRCONN, wire.h) written atomically:
 * getcmd pulls the two continuation records in -- so no reader can mistake them
 * for commands -- and QUEUES the connect instead of acting on it, because the
 * pointer trackers (menu, ghost drag) also read this pipe and must not have a
 * window created under them mid-drag.  The queue is drained by the main loop.
 * Returns 1 when *c holds a plain command, 0 when the caller should read again. */
/* One slot per possible window: a desktop-full stampede of simultaneous
 * connects (start-up rc with many apps) must never drop one -- a dropped
 * connect is a client that hangs unanswered until it times out and dies. */
#define NCONNQ	MAX_WINDOWS
union crec {
	WMSG	c;
	HRCONN	hc;
	HRDLGO	hd;
};
static union crec cbuf;
static HRCONN	connq[NCONNQ];
static int	nconnq;

getcmd(c)
WMSG *c;
{
	int n, got;

	n = read(cmdfd, (char *)&cbuf, sizeof(WMSG));
	if ( n != sizeof(WMSG) )
		return 0;
	if ( cbuf.hc.hc_type == C_CONNECT )
	{
		for ( got = sizeof(WMSG); got < sizeof(HRCONN); got += n )
		{
			n = read(cmdfd, (char *)&cbuf + got, sizeof(HRCONN) - got);
			if ( n <= 0 )
				return 0;
		}
		if ( nconnq < NCONNQ )
			connq[nconnq++] = cbuf.hc;
		else
			srvlogs("connect queue full\n");
		return 0;
	}
	if ( cbuf.hd.hd_type == C_DLGOPEN )
	{
		/* same continuation-read + queue discipline as the connect */
		for ( got = sizeof(WMSG); got < sizeof(HRDLGO); got += n )
		{
			n = read(cmdfd, (char *)&cbuf + got, sizeof(HRDLGO) - got);
			if ( n <= 0 )
				return 0;
		}
		/* sanitize before anything trusts it: a client bug must not
		 * become a server one (sizes are re-clamped in dodlgopen) */
		if ( cbuf.hd.hd_wid >= 0 && cbuf.hd.hd_wid < MAX_WINDOWS &&
		     wins[cbuf.hd.hd_wid].used )
		{
			if ( ndlgq < NDLGQ )
				dlgq[ndlgq++] = cbuf.hd;
			else
			{	/* full: ANSWER (refuse), never leave the
				 * requester blocked on a reply that won't come */
				sendev(cbuf.hd.hd_wid, E_DLGOPEN, 0, 0, 0, 0);
				srvlogs("dialog queue full\n");
			}
		}
		return 0;
	}
	*c = cbuf.c;
	return 1;
}

/* Create the windows for any connects taken while a menu/drag owned the pipe. */
drainconn()
{
	HRCONN q[NCONNQ];
	int i, n;

	if ( (n = nconnq) == 0 )
		return;
	for ( i = 0; i < n; i++ )		/* copy out first: doconnect draws, */
		q[i] = connq[i];		/* and could queue another connect  */
	nconnq = 0;
	for ( i = 0; i < n; i++ )
		doconnect(&q[i]);
}

docmd(c)
WMSG *c;
{
	int wid;

	wid = c->wm_wid;
	if ( c->wm_type == C_INPUT )
	{
		if ( dlgwid >= 0 )
		{
			/* a client dialog is up: system-modal, everything is
			 * routed (or swallowed) by the dialog tracker */
			dlginput(c);
			return;
		}
		if ( c->wm_arg[0] == IN_MOVE )
		{
			int cx, cy;

			mx = c->wm_arg[1];
			my = c->wm_arg[2];
			hr_glob()->curx = mx;	/* clients read this for overlap */
			hr_glob()->cury = my;
			SM_Mouse_Pos.x = mx;	/* bitblt cursor-hide gating (bug #1) */
			SM_Mouse_Pos.y = my;
			/* Forward to the grab holder only -- see grabwid.  Clamped to the
			 * content and de-duplicated, so dragging off the window or jittering
			 * in place costs no pipe traffic. */
			if ( grabwid >= 0 && toclient(grabwid, mx, my, &cx, &cy) &&
			     (cx != lastgx || cy != lastgy) )
			{
				lastgx = cx;  lastgy = cy;
				sendev(grabwid, E_MOTION, cx, cy, SM_LFT, 0);
			}
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
	/* A client published a new selection.  Only one window may show a highlight
	 * for it, so tell the previous owner to drop its own (wire.h E_SELCLEAR).
	 * The server keeps no selection data -- that is in the shared store; all it
	 * arbitrates is which window is entitled to say it holds it. */
	if ( c->wm_type == C_SELOWN )
	{
		if ( selwid >= 0 && selwid != wid )
			sendev(selwid, E_SELCLEAR, 0, 0, 0, 0);
		selwid = wid;
		return;
	}
	if ( c->wm_type == C_DLGCLOSE )
	{
		if ( wid == dlgwid )	/* only the owner may close it */
			dlgclose();
		return;
	}
	if ( c->wm_type == C_BYE )
	{
		srvlogn("C_BYE wid ", wid);
		/* killwin restacks and repaints -- not under a dialog box.  The
		 * OWNER's own C_BYE must act now, though: killwin closes the
		 * dialog first (below) and the teardown follows cleanly. */
		if ( dlgwid >= 0 && wid != dlgwid &&
		     wid >= 0 && wid < MAX_WINDOWS )
		{
			dlgbye |= 1 << wid;
			return;
		}
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

/* Crash insurance.  /drv/hr owns the keyboard interrupt vector for as long as
 * it is loaded -- hrload() rewires it to its own ISR, hruload() puts hrtty's
 * back (drv/hr.c) -- and its ISR posts every scancode to a message queue only
 * the server reads.  So a server that dies WITHOUT reaching quitwm() leaves the
 * text console on screen but stone deaf: the driver keeps eating the keys, and
 * nothing is left to unload it.  That is a machine you can only reset.
 *
 * The fix is to have something outlive the server: fork here, before the driver
 * is loaded, and keep the ORIGINAL process as a watchdog that does nothing but
 * wait().  Whatever kills the server -- SIGSEGV, SIGKILL, an exit(1) from a
 * failed start-up -- the parent wakes up and unloads the driver, which is all
 * it takes to get the console back.  A clean quitwm() has already unloaded it
 * and exits 0, so that status is the one case we leave alone.  The watchdog
 * half execs the tiny /usr/hr/bin/zvwatch so the vigil does not hold a full
 * copy of this image in RAM (see below); the code after the exec is its
 * in-process fallback.
 *
 * It cannot help a server that HANGS rather than dies; the driver's own
 * Ctrl-Alt-HELP escape (hr2.c hrkey: hruload + SIGSEGV to the server) is the
 * answer to that, and it is what makes this watchdog fire afterwards.
 *
 * Returns 0 in the server; the watchdog never returns.  A fork() failure is not
 * fatal -- coming up without crash insurance beats not coming up. */
srvwatch()
{
	int srv, w, st, pid;

	if ( (srv = fork()) <= 0 )
		return srv;

	close(cmdfd);			/* the command pipe is the server's alone */
	close(cmdwr);
	/* Outlive a stray ^C / ^\ / hangup on the launching terminal: dying here
	 * would silently throw away the only cleanup the system has left.
	 * SIG_IGN survives the exec below, so zvwatch inherits the immunity. */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	/* Hand the vigil to the TINY libc-only /usr/hr/bin/zvwatch: this process
	 * is a full fork of the ~69Kb non-shared zview image, and exec is what
	 * gives that contiguous RAM back while keeping our pid, our one child
	 * (the server) and the ignored signals.  Everything below this point is
	 * both zvwatch's job and our FALLBACK if the exec fails -- coming up as
	 * a fat watchdog beats coming up without crash insurance. */
	{
		int f;
		for ( f = 5; f < 20; f++ )
			close(f);	/* the trace log, if any */
	}
	execl("/usr/hr/bin/zvwatch", "zvwatch", (char *)0);
	while ( (w = wait(&st)) != srv && w >= 0 )
		;
	if ( w == srv && st == 0 )
		_exit(0);			/* quitwm(): already cleaned up */
	/* (zvwatch also declares the session dead to the surviving clients --
	 * tail magic + doorbells, see zvwatch.c.  This in-process fallback
	 * only runs when that exec failed and keeps to the old minimum, the
	 * driver unload: clients still exit off the dead magic below.) */
	hr_glob()->magic = 0;
	if ( (pid = fork()) == 0 )
	{
		execl("/etc/uload", "uload", "/drv/hr", (char *)0);
		_exit(1);
	}
	while ( pid > 0 && (w = wait(&st)) != pid && w >= 0 )
		;
	_exit(0);	/* (zvwatch, the normal vigil, also restores + labels
			 * the console; this bare fallback just unloads) */
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
	int cp[2];
	WMSG c;

	gfx_reply_hook = onreply;
	signal(SIGPIPE, onpipe);		/* survive a client's broken pipe */

	/* REFUSE to start on a machine with no hi-res card (a serial-console or
	 * lo-res machine).  Without the card there is no framebuffer and no
	 * hi-res keyboard, so a server that starts anyway just wedges the
	 * machine's keyboard vector into /drv/hr and sits idle forever.  All the
	 * hardware behind this program is at segments 0x3A/0x3B, and with no
	 * responder there stores are dropped and loads float -- so probe the
	 * VRAM tail the way the boot ROM probes the framebuffer, write and read
	 * back (hr_selok, hrsel.c), and say so instead of appearing to work. */
	if ( !hr_selok() )
	{
		printf("zview: no hi-res display on this machine\n");
		exit(1);
	}

	/* REFUSE to start inside a running session.  zview can be launched from
	 * any shell -- including a zterm of a server that is already up -- and a
	 * second server is pure destruction: it re-zeroes the drawing lock while
	 * it is in use, backgrounds the whole framebuffer over the live desktop,
	 * and wipes the session's connect-ack slots and event rings.  The probe
	 * is /dev/dmgr: /drv/hr is loaded for exactly the life of a server
	 * session (loaded below, unloaded by quitwm, the watchdog and the
	 * Ctrl-Alt-HELP escape), and an unloaded loadable major fails open with ENXIO
	 * (bio.c drvmap: d_conp NULL) -- so an openable dmgr means a server is
	 * up.  DMGR is the shared non-exclusive minor, so the probe disturbs
	 * nothing.  Checked BEFORE the /dev/null redirect below so the refusal
	 * lands on the terminal it was typed into.  (Should a wreck ever leave
	 * the driver loaded with no server behind it, /etc/uload /drv/hr clears
	 * the way.) */
	{
		int pf;

		if ( (pf = open("/dev/dmgr", 0)) >= 0 )
		{
			close(pf);
			printf("zview: a window server is already running\n");
			exit(1);
		}
	}

	if ( pipe(cp) < 0 )
	{
		printf("zview: pipe failed\n");
		exit(1);
	}
	cmdfd = cp[0];
	cmdwr = cp[1];
	/* The trace log is opened AFTER the pipe on purpose, so cmdwr still
	 * lands on fd 4 == HR_CMDFD (wire.h) and every child's dup2 of it is
	 * the identity.  (An fd opened before the pipe once shifted both ends
	 * up by one, and the pump's input went to the wrong fd -- a desktop
	 * that painted but never saw the mouse.  The pump dup2's now, but
	 * keep the safe order anyway.) */
	if ( trace )
		srvlog = creat("/wslog", 0644);	/* debug log, extract with disk.py */

	/* From here on WE own the framebuffer, and the console (hrtty) is drawing
	 * on the very same pixels -- there is no graphics-mode handshake yet
	 * (GUI.md sec 2.6), so ANY write to the console scribbles over the desktop.
	 * Several things would otherwise do exactly that, none of them ours:
	 *   - /bin/sh prints the pid of every `&' job (exec1.c NBACK), so the two
	 *     background lines in /usr/hr/etc/rc land on the screen as bare
	 *     numbers -- the visible symptom that led here;
	 *   - /etc/load reports driver errors on stderr;
	 *   - the salvaged engine still has debug printf()s (gfx/bitblt.c, f2.c).
	 * Started from /etc/rc these all go to /dev/null already and the problem is
	 * invisible; started BY HAND from the console they corrupt the display.  So
	 * point our own stdout/stderr at /dev/null and let every child inherit it.
	 * Anything genuinely diagnostic belongs in the trace log (a file), not here.
	 * Done after the pipe() check above so a failure to start is still seen. */
	{
		int nfd;

		if ( (nfd = open("/dev/null", 2)) >= 0 )
		{
			dup2(nfd, 1);
			dup2(nfd, 2);
			if ( nfd > 2 )
				close(nfd);
		}
	}

	/* Split off the crash watchdog BEFORE the driver that it exists to undo
	 * is loaded, so there is no window in which a death goes uncovered. */
	srvwatch();

	/* Take over the screen + input: load /drv/hr (keyboard + polled mouse +
	 * hardware cursor), paint the desktop, then fork the input pump. */
	loaddriver();
	loadpty();				/* pty pairs for the terminal windows */
	/* A second driver fd (any minor) for cursor on/off; wire it into the
	 * engine so blits hide the driver's cursor and leave no artifacts. */
	curfd = open("/dev/dmgr", 2);
	gfx_curhide_hook = srv_curhide;
	gfx_curshow_hook = srv_curshow;

	/* Initialise the global drawing lock BEFORE anything can take it.  It
	 * lives in the VRAM tail, which is uninitialised RAM at power-on and keeps
	 * whatever a previous zview (or a client that died holding it) left there
	 * -- and nothing else zeroes it.  Both words matter:
	 *   SHM_LOCK  garbage reads as "held" (hr_tas tests the top bit), so every
	 *             client's first primitive traps into the driver and blocks;
	 *   SHM_WAIT  garbage is worse -- it is the blocked-waiter count, so our
	 *             own first hr_unlock takes the CIOMUNLOCK hand-off path, which
	 *             deliberately leaves the word HELD for a waiter that does not
	 *             exist (hrlock.c: fairness), wedging every later acquirer
	 *             including us.  That is a desktop whose chrome paints once and
	 *             where no client ever draws again.
	 * We own the screen and no client exists yet, so this is ours to reset. */
	*(short *)(HRTAIL + SHM_LOCK)  = 0;
	*(short *)(HRTAIL + SHM_WAIT)  = 0;
	*(short *)(HRTAIL + SHM_OWNER) = 0;

	reset_screen();

	/* Load the system fonts into the shared VRAM tail before any glyph is
	 * drawn (chrome in launchapp->mkwin->srvtitle needs them).  One copy each;
	 * the server and every direct-render client blit from here. */
	loadfont(SHM_FTERM, "/usr/hr/fonts/gacha.r.hf");
	loadfont(SHM_FUI,   "/usr/hr/fonts/gacha.b.hf");
	loadfont(SHM_FICON, "/usr/hr/fonts/sail.hf");
	hr_glob()->curon = 1;			/* driver draws its cursor by default */
	hr_glob()->overlay = 0;			/* no menu/overlay up yet */
	hr_glob()->stacking = 0;		/* no layer op in flight yet */
	{ int w;
	  for ( w = 0; w < MAX_WINDOWS; w++ )
	  {
		hr_setdraw(w, 0);
		/* Same garbage RAM: publish_surf only republishes a descriptor that
		 * differs from what is there, so what is there must be defined. */
		hr_surf(w)->seq = 0;
		hr_surf(w)->mapped = 0;
		hr_surf(w)->nvis = 0;
		/* The window list too: publish_wins change-compares against it, and
		 * a garbage-odd wl_seq would spin every reader (parity survives the
		 * paired ++s, like the dialog surface below). */
		hr_wlist()->wl_win[w].ww_used = 0;
	  }
	  hr_wlist()->wl_seq = 0;
	}
	hr_glob()->magic = HR_MAGIC;
	/* The selection store lives in the same uninitialised tail RAM, so stamp it
	 * before any client can read it -- and drop the file a previous session's
	 * owner left behind (shmem.h).  All those clients are gone with the server
	 * that launched them, so nothing outlives this. */
	hr_selinit();
	hr_ackclr(0);			/* connect-ack slots: same garbage RAM */
	hr_evinit(-1);			/* ... and every event ring */
	{
		/* The dialog surface is the same garbage RAM.  An odd leftover
		 * seq would spin every later seqlock read forever (publish_dlg
		 * only ever adds 2, which preserves parity), so zero it here. */
		HRSURF *sp;

		sp = hr_dlgsurf();
		sp->seq = 0;
		sp->mapped = 0;
		sp->nvis = 0;
	}

	pumppid = startpump();

	/* Load the launchable-app catalog (/usr/hr/etc/apps) -- what the right-click
	 * desktop menu offers -- and put up the placeholder icons of every entry
	 * that names one: the start-up desktop is a row of icons with NOTHING
	 * running behind them, and an app is only launched (and only then takes
	 * memory) when its icon is clicked.  The rc script still runs for anything
	 * the user wants genuinely resident from power-on. */
	loadapps();
	placesync();
	deadadd(runrc());	/* the rc shell exits when the script ends: reap it */

	for (;;)
	{
		if ( getcmd(&c) )
			docmd(&c);
		reapwins();		/* notice a client that died window-up */
		if ( ndead )		/* collect children whose windows are gone */
			reapdead();
		if ( dlgwid < 0 )
		{
			/* Deferred while a client dialog is up: doconnect/mkwin
			 * paints chrome and restacks -- straight over a box that
			 * is not a layer.  Both queues hold until it closes. */
			drainconn();	/* windows for clients that just connected */
			draindlg();	/* dialog opens taken mid-tracker */
		}
	}
}
