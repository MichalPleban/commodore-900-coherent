#include <stdio.h>
#include "smgr.h"

/* The engine globals a CLIENT reaches.  This file is linked directly by
 * every consumer -- including libhrgfx.sl, whose private half is COPIED
 * INTO EVERY PROCESS at exec -- precisely because Coherent's one-pass ld
 * does not pull a tentative definition out of an archive
 * ([[hrgfx-global-must-be-in-globals-c]]).  That is also why what lives
 * here is a per-process COST: the server-only globals moved to
 * globals_srv.c, which only the server and gfxtest link.  Anything added
 * here is paid for by every GUI process on the machine. */

BITMAP	display;
WSTRUCT	gk;
MESSAGE	msg;
/* These two look server-side and are not: the .sl link (which is the
 * enforcement) says the CLIENT stack reads them.  gtext.c switches the
 * context through wtbl[] when it draws into a window other than gk's --
 * in a client every entry is NULL, so the branch is never taken, but it
 * still has to link.  bitblt.c reads SM_Mouse_Pos to decide whether a
 * blit's rect is near enough the pointer to hide it first. */
WSTRUCT	*wtbl[MAX_WINDOWS];
POINT	SM_Mouse_Pos;
/* The damage rect that goes with the WM_UPDATE perform_update() is about to
 * send, in absolute screen coordinates (see gfxhooks.c sendmsg).  MESSAGE has
 * only msg_Data[3] and msg_Data[0] already carries the wid, so there is no room
 * for four coordinates in the message -- and that rect is what lets a client
 * repaint the strip that was covered instead of its whole window.  A side
 * channel is safe because sendmsg() is a direct call: the hook runs to
 * completion before perform_update() looks at the next update[] entry. */
RECT	gfx_uprect;
