/*
 * wire.h - hrgui client <-> server wire protocol (GUI.md Phase 1, server-render).
 *
 * Transport (a deliberate simplification of GUI.md sec 3 for Phase 1):
 *   - ONE shared command pipe, client -> server.  Every client (and the input
 *     pump) inherits its write end at fork; the server owns the read end and
 *     blocks on it -- so the whole system waits on a single read(), no select()
 *     needed (GUI.md sec 3.5 doorbell, sec 4.4).  Records are fixed size and
 *     small (<= PIPE_BUF), so concurrent writers interleave atomically.
 *   - ONE private event pipe per client, server -> client, for connect
 *     acknowledgement, expose/damage, resize and routed input.
 *
 * The shared-VRAM ring of GUI.md sec 3.4 is a later optimisation for high-rate
 * direct-render clients; a clock ticking once a second does not need it, and a
 * pipe avoids the "verify the free VRAM tail" open item.  Rule zero still holds:
 * only control records travel over IPC, never pixels (GUI.md sec 3.2).
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

/* Inherited fd numbers handed to a launched client (dup2'd before exec). */
#define HR_EVFD		3		/* client reads events here   */
#define HR_CMDFD	4		/* client writes commands here */

/* ---- client -> server command opcodes ---- */
#define C_CONNECT	1		/* arg0=req width, arg1=req height     */
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

/* C_INPUT kinds (wm_arg[0]) */
#define IN_KEY		0		/* arg1 = ascii                        */
#define IN_MOVE		1		/* arg1,arg2 = abs x,y                  */
#define IN_BUTTON	2		/* arg1,arg2 = x,y  arg3 = button bits  */

/* ---- server -> client event codes ---- */
#define E_CONNECTED	1		/* arg0=wid arg1=width arg2=height      */
#define E_EXPOSE	2		/* arg0..3 = x,y,w,h (content-relative) */
#define E_RESIZE	3		/* arg0=width arg1=height               */
#define E_QUIT		4		/* window closed; client should exit    */
#define E_KEY		5		/* arg0 = ascii (focused window)        */
#define E_BUTTON	6		/* arg0,arg1=x,y content-rel arg2=btns  */

#endif /* HRWIRE_H */
