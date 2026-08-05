#include <stdio.h>
#include "smgr.h"


BITMAP	display;
LAYER	*DM_rearmost;
LAYER	*DM_frontmost;
WSTRUCT	*wtbl[MAX_WINDOWS]; 
WSTRUCT	gk;
MESSAGE	msg;
char	gkmsg[sizeof(GRAPH)+4];
POINT	SM_Mouse_Pos;
UPDATE	update[MAX_UPBUF];
/* The damage rect that goes with the WM_UPDATE perform_update() is about to
 * send, in absolute screen coordinates (see gfxhooks.c sendmsg).  MESSAGE has
 * only msg_Data[3] and msg_Data[0] already carries the wid, so there is no room
 * for four coordinates in the message -- and that rect is what lets a client
 * repaint the strip that was covered instead of its whole window.  A side
 * channel is safe because sendmsg() is a direct call: the hook runs to
 * completion before perform_update() looks at the next update[] entry.
 * It lives HERE, with the engine's other globals, and not in gfxhooks.c: this
 * file is linked directly by every consumer precisely because Coherent's
 * one-pass ld does not pull a tentative definition out of an archive. */
RECT	gfx_uprect;
