/*
 * gfxhooks.c - the libhrgfx "divorce" shim (GUI.md sec 6).
 *
 * Definitions for the few symbols the rendering engine used to import from the
 * old smgr.c / wmgr.c message layer.  Each forwards to a server-installable
 * hook (default null == no-op) so the engine is a self-contained archive:
 * `nm libhrgfx.a' shows no undefined sendmsg/getdata/peteprint/mouse-ioctl
 * references (the completeness gate, GUI.md sec 6.1).
 */
#include <stdio.h>
#include "smgr.h"

void (*gfx_curhide_hook)() = 0;
void (*gfx_curshow_hook)() = 0;
int  (*gfx_reply_hook)() = 0;
int  (*gfx_getdata_hook)() = 0;

/*
 * Cursor arbitration - replaces ioctl(myfd,CIOMSEOFF/CIOMSEON) in bitblt.c and
 * gline.c.  The server owns a save-under sprite (GUI.md sec 2.5.4); it installs
 * hooks that restore the pixels under the pointer before a blit and repaint the
 * sprite afterwards.  Standalone (no server): nothing to hide.
 */
void
gfx_cursor_hide()
{
	if ( gfx_curhide_hook )
		(*gfx_curhide_hook)();
}

void
gfx_cursor_show()
{
	if ( gfx_curshow_hook )
		(*gfx_curshow_hook)();
}

/*
 * sendmsg - the engine's reply / expose-damage path.  Query verbs (SM_GetPoint,
 * SM_GetPhy, ...) and layer.c's WM_UPDATE damage notifications call this with
 * the global `msg' filled in.  The server installs a hook that translates `msg'
 * into a control-pipe event to the owning client (GUI.md sec 6.3).
 */
sendmsg(m)
MESSAGE *m;
{
	if ( gfx_reply_hook )
		return (*gfx_reply_hook)(m);
	return 0;
}

/*
 * getdata - bulk pull of a client's string/bitmap payload (was the driver
 * CIOGETD XFER channel).  Used by the text verbs; the server installs a hook
 * that copies from the client's ring / shared region.
 */
getdata(src, n, srcp, dstp)
unsigned src, n;
char *srcp, *dstp;
{
	if ( gfx_getdata_hook )
		return (*gfx_getdata_hook)(src, n, srcp, dstp);
	return 0;
}

/*
 * peteprint - the engine's internal debug logger (formerly wrote /dev/console).
 * Silent by default; the standalone test and the server do not need it.
 */
/*VARARGS1*/
peteprint(fmt)
char *fmt;
{
}

/*
 * SM_ClrClip - clear a window's content rectangle to a solid pattern.  It lived
 * in wmgr.c but is a pure drawing op (gkBitMap/gkCrect/lblt/texture only), and
 * gtext2.c's scroll path calls it, so it belongs with the engine.  Faithful to
 * the original: msgBytL0 selects background vs foreground pattern.
 */
extern BITMAP	display;
extern int	*screen_addr();
extern int	words_between();

void
SM_ClrClip()
{
	BLTSTRUCT blt;
	BITMAP s;

	/* Fill the content rect via the display bitmap (base SEG0), exactly as
	 * layer.c's rectf() does -- the proven-working blit path.  Blitting with
	 * the destination base set to a mid-VRAM screen_addr() faults here, so we
	 * do not use lblt(&gkBitMap) as the original did. */
	s.rect = gkCrect;
	s.width = 16 * words_between(gkCrect.origin.x, gkCrect.corner.x);
	s.base = screen_addr(gkCrect.origin.x, gkCrect.origin.y);
	blt.src = &s;
	blt.sp = gkCrect.origin;
	blt.dst = &display;
	blt.dr = gkCrect;
	blt.op = L_TRUE;
	blt.pat = texture[ msgBytL0 ? gkBpat : gkFpat ];
	bitblt(&blt, 1, 0);
}
