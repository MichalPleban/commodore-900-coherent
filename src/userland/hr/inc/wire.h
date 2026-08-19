/*
 * wire.h - hrgui client <-> server wire protocol (GUI.md Phase 1, server-render).
 *
 * Transport:
 *   - ONE shared command pipe, client -> server.  Every client (and the input
 *     pump) inherits its write end at fork; the server owns the read end and
 *     blocks on it -- so the whole system waits on a single read(), no select()
 *     needed (GUI.md sec 3.5 doorbell, sec 4.4).  Records are fixed size and
 *     small (<= PIPE_BUF), so concurrent writers interleave atomically.
 *   - A per-window RING in the shared VRAM tail, server -> client, for expose,
 *     resize, routed input and the quit notice (inc/shmem.h SHM_EVQ).  This is
 *     GUI.md sec 3.4's shared-VRAM ring, and it replaced the per-client event
 *     pipe: on this kernel a pipe is a file, so a pipe cost block allocation and
 *     buffer-cache traffic per event, and the FIFO a shell-started client had to
 *     mknod for itself was not even reliable.  The connect acknowledgement comes
 *     through shmem.h SHM_ACK, because until it arrives a client does not know
 *     which ring is its own.
 *
 * Rule zero still holds: only control records travel between processes, never
 * pixels (GUI.md sec 3.2).
 *
 * Coordinates in draw commands are WINDOW-RELATIVE (0,0 = content origin); the
 * server adds the physical origin and clips (GUI.md sec 2.10).
 */
#ifndef HRWIRE_H
#define HRWIRE_H

#define WM_NARG		6

typedef struct {
	short	wm_type;		/* C_* (to server) or E_* (to client) */
	short	wm_wid;			/* window id (stamped by the client)  */
	short	wm_arg[WM_NARG];
} WMSG;

/* The ONE inherited fd a launched client gets.  A client is exec'd with its own
 * plain argv (the server appends nothing) and everything the server needs to
 * know about it -- title, size, icon, flags, where it wants to be -- arrives
 * afterwards in its C_CONNECT record (HRCONN below, inc/hrapp.h).
 *
 * There is no event fd.  Server -> client events travel in the per-window rings
 * in the shared VRAM tail (inc/shmem.h SHM_EVQ), not over a pipe: on this kernel
 * a pipe IS a file (coh/pipe.c pread/pwrite go through fread/fwrite on the
 * inode), so an event channel per client meant block allocation, buffer-cache
 * traffic and -- for a client the server did not fork -- a mknod'd FIFO in /tmp
 * that also proved unreliable.  A client is told its window id, and therefore
 * which ring is its own, in the connect acknowledgement (shmem.h SHM_ACK).
 *
 * HR_CMDFD stays a pipe: it is ONE shared channel, client -> server, inherited
 * down the whole process tree (so an app started from the desktop rc script or
 * from a terminal has it too), and the server blocks on that single read(). */
#define HR_CMDFD	4		/* client writes commands here */

/* ---- client -> server command opcodes ---- */
#define C_CONNECT	1		/* an HRCONN record, see below         */
#define C_CLRCLIP	2		/* clear the content rect              */
#define C_SETLOGOP	3		/* arg0 = logical op (L_* from smgr)   */
#define C_MOVE		4		/* arg0,arg1 = logical x,y (pen up)    */
#define C_LINE		5		/* arg0,arg1 = logical x,y (draw to)   */
#define C_POINT		6		/* arg0,arg1 = logical x,y (plot pen)  */
#define C_FLUSH		7		/* no-op sync marker                   */
#define C_BYE		8		/* client is exiting; reap the window  */
#define C_INPUT		9		/* from the input pump (wm_wid unused) */
#define C_TEXT		10		/* arg0=col arg1=row arg2..5=8 chars   */
#define C_ERASE		11		/* arg0=col arg1=row arg2=ncol arg3=nrow*/
#define C_SELOWN	12		/* "I now own the selection" (see below) */
#define C_DLGOPEN	13		/* an HRDLGO record, see below           */
#define C_DLGCLOSE	14		/* close my dialog overlay               */
#define C_ACTIVATE	15		/* arg0 = a window id: bring that
					 * window's APPLICATION forward -- the
					 * server raises the topmost visible
					 * window sharing its title base, or
					 * restores the named one when every
					 * window of the app is hidden.  Sent
					 * by the dock (zdock); wm_wid is the
					 * sender's own window as usual.       */

/* C_INPUT kinds (wm_arg[0]) */
#define IN_KEY		0		/* arg1 = ascii                        */
#define IN_MOVE		1		/* arg1,arg2 = abs x,y                  */
#define IN_BUTTON	2		/* arg1,arg2 = x,y  arg3 = button bits  */

/* ---- the connect record (client -> server, C_CONNECT) --------------------- *
 * A launched client owns its own window description: the server forks it
 * knowing only WHERE to put the window (the menu click position) and WHICH
 * catalog entry it came from, and the client then declares its title, desired
 * content size, desktop icon and flags here.  The server allocates the window
 * id at this point and answers E_CONNECTED with the id and the GRANTED content
 * size (it may clamp to the screen), so nothing about a window's geometry has
 * to be duplicated in /usr/hr/etc/apps.
 *
 * HRCONN is exactly 3 * sizeof(WMSG) and its first two shorts overlay WMSG's
 * wm_type/wm_wid, so the server's fixed-size record reader dispatches it like
 * any other command and then consumes the two continuation records -- which are
 * guaranteed to be there, because one write() of 48 bytes (<= PIPE_BUF) lands
 * on the shared command pipe atomically, so no other client can interleave.
 * hc_pid is how the server matches the record to the launch it forked (the
 * client has no window id to stamp into wm_wid yet). */
#define HRC_TITLE	16		/* including the NUL */
#define HRC_ICON	16
typedef struct {
	short	hc_type;		/* C_CONNECT                            */
	short	hc_pid;			/* getpid(): matches a pending launch   */
	short	hc_w, hc_h;		/* desired content size, px             */
	short	hc_flags;		/* HRF_*                                */
	short	hc_x, hc_y;		/* wanted window origin, if HRF_POS     */
	short	hc_menu;		/* HRM_*: our own window-menu entries   */
	char	hc_title[HRC_TITLE];	/* window title / instance base name    */
	char	hc_icon[HRC_ICON];	/* .icn under /usr/hr/icons ("" = default) */
} HRCONN;

/* hc_flags bits (also the ha_flags of an HRAPP, inc/hrapp.h).  STRETCH is the
 * application's own property; POS and MIN are normally set by the command-line
 * options -P and -H, which every GUI client parses through hr_open(). */
#define HRF_STRETCH	0x0001		/* user may resize the window (else the  */
					/* window menu offers no "Stretch" and   */
					/* the size the client asked for stands) */
#define HRF_POS		0x0002		/* hc_x/hc_y are a wanted window origin  */
					/* (else the server places the window)   */
#define HRF_MIN		0x0004		/* open minimised to a desktop icon      */
#define HRF_CONFIRM	0x0008		/* window-menu Quit asks "really quit?"  */
					/* first (srvdialog) -- for an app whose */
					/* window holds live state (a shell)     */
#define HRF_NODECOR	0x0010		/* NO decoration: no title bar, no frame,*/
					/* no drop shadow -- the content is the  */
					/* whole window rect.  For desktop       */
					/* furniture (the zdock icon bar): such  */
					/* a window never takes keyboard focus,  */
					/* right-click over it opens the DESKTOP */
					/* menu, and it is left out of the       */
					/* "Switch to" list.                     */

/* ---- the dialog-open record (client -> server, C_DLGOPEN) ----------------- *
 * A connected client asks for a MODAL DIALOG OVERLAY: the server saves the
 * pixels under a centred box, draws the frame (1px border + the window-style
 * stepped drop shadow; dialogs are untitled), publishes the interior as a
 * drawable surface (shmem.h SHM_DLGSURF), and routes ALL input to this client
 * as E_D* events until C_DLGCLOSE.  Every other client is frozen meanwhile
 * (hr_glob()->overlay = OV_DLG|wid), and so is this client's own window --
 * only the dialog surface may be drawn.
 *
 * Exactly 2 * sizeof(WMSG); first two shorts overlay wm_type/wm_wid so the
 * server's fixed-size reader dispatches it like any command and consumes the
 * one continuation record (one 32-byte write() <= PIPE_BUF, atomic).  Unlike
 * HRCONN the second short is a WID, not a pid: the requester is connected by
 * definition and stamps the id it was granted.
 *
 * The answer is E_DLGOPEN on the requester's ring: arg0 = 1 and the granted
 * interior size (the server may clamp), or arg0 = 0 = refused (another dialog
 * is already up -- there is at most ONE in the system).  Every request gets
 * exactly one answer, so the requester may block for it. */
typedef struct {
	short	hd_type;		/* C_DLGOPEN                            */
	short	hd_wid;			/* requester's window id                */
	short	hd_w, hd_h;		/* wanted interior size, px             */
	short	hd_res;			/* 0 (future flags)                     */
	char	hd_pad[22];		/* pad to 2 * sizeof(WMSG) == 32        */
} HRDLGO;

/* ---- application entries in the window menu (hc_menu, HRAPP ha_menu) ------ *
 * There is no menu bar in this GUI: a window's only menu is the right-button
 * pop-up the server puts up over it, and the server owns it (it is drawn as an
 * overlay while every client is frozen, so a client could not draw one itself).
 * So an application that wants commands of its own DECLARES them here, as bits,
 * and the server puts the corresponding standard entries at the TOP of that
 * window's menu, then a divider, then the usual window operations.
 *
 * The labels and their order are fixed -- that is the point: "Save" sits in the
 * same place with the same wording in every application, which no per-app string
 * list would give.  An application that needs something outside this vocabulary
 * puts it in its own content, not in the window menu.
 *
 * Picking one sends E_MENU with the bit in arg0, and nothing else: the server
 * does not know what "Save" means to the client, it only routes the click.  The
 * bits are what a client declared, so it can dispatch on them directly. */
#define HRM_NEW		0x0001		/* "New"        */
#define HRM_OPEN	0x0002		/* "Open"       */
#define HRM_SAVE	0x0004		/* "Save"       */
#define HRM_CUT		0x0008		/* "Cut"        */
#define HRM_COPY	0x0010		/* "Copy"       */
#define HRM_PASTE	0x0020		/* "Paste"      */
#define HRM_SETTINGS	0x0040		/* "Settings"   */
#define HRM_HELP	0x0080		/* "Help"       */
#define HRM_ALL		0x00ff		/* every bit above: what the server knows */

/* ---- server -> client event codes ---- */
#define E_CONNECTED	1		/* arg0=wid arg1=width arg2=height      */
#define E_EXPOSE	2		/* arg0..3 = x,y,w,h (content-relative) */
#define E_RESIZE	3		/* arg0=width arg1=height               */
#define E_QUIT		4		/* window closed; client should exit    */
#define E_KEY		5		/* arg0 = ascii, or an HRK_* function   */
					/* key (below) -- one short either way  */

/* ---- function keys in E_KEY arg0 ------------------------------------------ *
 * arg0 is ASCII (0..0x7f); the function keys ride just above it, so a client
 * that only handles ASCII needs no change at all -- the dialog kit ignores
 * them, and zterm's pump drops them at the pty (there is no byte to write).
 * The C900 keyboard has F1-F10 and five specials, presented as F11-F15:
 *   F11 = Help    F12 = Clear/Home    F13 = Pop/Push
 *   F14 = Screen/Print    F15 = Stop/Continue
 * (zvpump maps the scancodes; see the table comment there for which). */
#define HRK_F1		0x81
#define HRK_F2		0x82
#define HRK_F3		0x83
#define HRK_F4		0x84
#define HRK_F5		0x85
#define HRK_F6		0x86
#define HRK_F7		0x87
#define HRK_F8		0x88
#define HRK_F9		0x89
#define HRK_F10		0x8a
#define HRK_HELP	0x8b		/* F11 */
#define HRK_CLRHOME	0x8c		/* F12 */
#define HRK_POPPUSH	0x8d		/* F13 */
#define HRK_SCRPRT	0x8e		/* F14 */
#define HRK_STOP	0x8f		/* F15 */

/* Alt+F1..Alt+F5: the window-operation shortcuts (Move, Stretch, Back, Hide,
 * Quit on the focused window -- the same order the window menu lists them).
 * zvpump adds HRK_ALTFN to HRK_F1..F5 when Alt is held; the SERVER acts on
 * the result itself and never forwards it, so no client ever sees one of
 * these -- they are named here only because producer (zvpump) and consumer
 * (zview) share this header.  Alt+F6..F15 stay bare on purpose: nothing is
 * bound to them, and eating them would cost clients keys for no gain. */
#define HRK_ALTFN	0x10		/* Alt's offset on HRK_F1..HRK_F5 */
#define HRK_AF1		(HRK_F1+HRK_ALTFN)	/* 0x91  Move            */
#define HRK_AF5		(HRK_F5+HRK_ALTFN)	/* 0x95  Quit            */

/* ---- pointer events (the selection gesture) ------------------------------- *
 * Coordinates are CONTENT-relative, like everything else a client sees.  Button
 * masks are the SM_LFT/SM_MID/SM_RGHT bits of smgr_defs.h (0x8000/0x4000/0x2000)
 * exactly as they arrive from the driver.
 *
 * E_BUTTON/E_MOTION are sent under an IMPLICIT GRAB: a left press inside a
 * window's content starts the grab and the matching release ends it, and motion
 * is forwarded ONLY in between.  So a client sees the whole press-drag-release
 * gesture it needs to track a selection, and the server never streams motion at
 * a client that is not dragging -- pointless traffic that would just overflow a
 * ring whose owner is not listening for it.
 *
 * E_PASTE is the middle-click "insert the selection here".  It goes to the window
 * UNDER THE POINTER and does NOT raise it: pasting into a partly covered window
 * must not restack the desktop, which is the whole point of the gesture.  The
 * bytes are not in the event -- the client reads them from the shared selection
 * store itself (shmem.h hr_selread), so nothing large ever crosses a pipe. */
#define E_BUTTON	6		/* arg0,arg1=x,y arg2=down arg3=changed */
#define E_MOTION	7		/* arg0,arg1=x,y arg2=buttons held      */
#define E_PASTE		8		/* arg0,arg1=x,y ; read the selection    */

/* There is ONE selection, so at most one window may show a highlight for it.
 * A client that publishes a new selection sends C_SELOWN, and the server tells
 * the PREVIOUS owner to drop its highlight with E_SELCLEAR -- otherwise two
 * windows would both appear to hold the selection while only the newer one
 * actually does (X11 calls this SelectionClear).
 *
 * A PASTE clears it too: once the text has been delivered the gesture is over,
 * so the server sends E_SELCLEAR to the owner and forgets it.
 *
 * Note E_SELCLEAR concerns only the HIGHLIGHT.  The bytes stay in the shared
 * store, so clearing costs nothing and the same selection can be pasted again;
 * what goes away is only the claim that some window is still holding it. */
#define E_SELCLEAR	9		/* another window took the selection     */

/* The user picked one of the entries this client declared in hc_menu: arg0 is
 * the single HRM_* bit that was chosen.  It arrives only for bits the client
 * actually asked for, so a client may switch on arg0 with no default case --
 * though an unknown bit must of course be ignored rather than acted on. */
#define E_MENU		10		/* arg0 = HRM_* the user selected        */

/* ---- dialog events (C_DLGOPEN above) -------------------------------------- *
 * Everything a client sees while its dialog is up.  DISTINCT types on purpose:
 * an E_BUTTON that was already queued in the ring when the dialog opened must
 * not be mistakable for a dialog click, so the dialog loop can simply discard
 * every event that is not E_D* (the server sends a full-content E_EXPOSE at
 * close, which subsumes anything dropped meanwhile).
 *
 * Coordinates are DIALOG-INTERIOR-relative.  E_DBUTTON follows E_BUTTON's
 * grab shape: a left press INSIDE the interior starts it, motion is forwarded
 * only while the button is held, and the release always arrives -- with
 * coordinates deliberately UNCLAMPED, because a button widget disarms by
 * seeing the pointer leave it.  E_DKEY is routed to the dialog owner
 * regardless of which window has focus. */
#define E_DLGOPEN	11		/* arg0=ok arg1,arg2=granted interior w,h */
#define E_DBUTTON	12		/* arg0,1=x,y arg2=down arg3=changed     */
#define E_DMOTION	13		/* arg0,1=x,y (unclamped) arg2=held      */
#define E_DKEY		14		/* arg0 = ascii                          */

/* Button bits in E_BUTTON/E_MOTION's masks.  Same values as SM_LFT/SM_MID/
 * SM_RGHT (gfx/smgr_defs.h) -- repeated here so a client needs only this header
 * and not the engine's. */
#define EB_LEFT		0x8000
#define EB_MID		0x4000
#define EB_RIGHT	0x2000

#endif /* HRWIRE_H */
