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
	short	hc_pad;
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

/* ---- server -> client event codes ---- */
#define E_CONNECTED	1		/* arg0=wid arg1=width arg2=height      */
#define E_EXPOSE	2		/* arg0..3 = x,y,w,h (content-relative) */
#define E_RESIZE	3		/* arg0=width arg1=height               */
#define E_QUIT		4		/* window closed; client should exit    */
#define E_KEY		5		/* arg0 = ascii (focused window)        */

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

/* Button bits in E_BUTTON/E_MOTION's masks.  Same values as SM_LFT/SM_MID/
 * SM_RGHT (gfx/smgr_defs.h) -- repeated here so a client needs only this header
 * and not the engine's. */
#define EB_LEFT		0x8000
#define EB_MID		0x4000
#define EB_RIGHT	0x2000

#endif /* HRWIRE_H */
