/*
 * gfxhooks.h - libhrgfx server-installable hooks (GUI.md sec 6.1-6.3).
 *
 * The rendering engine was extracted verbatim from the old `smgr' screen
 * manager and divorced from its message switch.  Where the engine used to
 * reach the driver / message layer directly it now calls through the hooks
 * below.  All default to no-ops (see gfxhooks.c), so a program that links
 * libhrgfx and draws straight into VRAM works with no server present.  The
 * window server installs real hooks at startup.
 *
 *	old direct coupling		replaced by
 *	------------------------	-----------------------------------
 *	ioctl(myfd,CIOMSEOFF/ON)	gfx_cursor_hide()/gfx_cursor_show()
 *	sendmsg(&msg)			forwards `msg' to gfx_reply_hook
 *	getdata(src,n,srcp,dstp)	forwards to gfx_getdata_hook
 *	peteprint(fmt,...)		silent by default
 */
#ifndef GFXHOOKS_H
#define GFXHOOKS_H

/* Save-under cursor arbitration (was the driver mouse on/off ioctls). */
extern void gfx_cursor_hide();
extern void gfx_cursor_show();

/* Server-installed callbacks.  When null the engine action is a no-op. */
extern void (*gfx_curhide_hook)();	/* hide the save-under cursor sprite  */
extern void (*gfx_curshow_hook)();	/* redraw the save-under cursor sprite */
extern int  (*gfx_reply_hook)();	/* turn the global `msg' into an event */
extern int  (*gfx_getdata_hook)();	/* pull bulk data from a client        */

#endif /* GFXHOOKS_H */
