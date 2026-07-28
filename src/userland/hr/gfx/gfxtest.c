/*
 * gfxtest.c - Phase 0 standalone draw test for libhrgfx (GUI.md sec 8).
 *
 * Links ONLY libhrgfx.a + libc.a - no window server, no IPC, no jlib - and
 * draws straight into the framebuffer at 0x3A/0x3B.  Its two jobs:
 *
 *   1. Completeness gate (GUI.md sec 6.1): the fact that it links with no
 *      undefined sendmsg / getdata / mouse-ioctl / jlib symbols proves the
 *      rendering engine was cleanly divorced from the message layer.
 *   2. Prove the engine still draws once divorced: it sets up one window
 *      (LAYER + gk context, mirroring SM_Reset/SM_Create minus the message
 *      plumbing), then draws a border, a cross, and a filled patch.
 *
 * It writes VRAM directly, so it must run with the hi-res card present (real
 * hardware or the web emulator); the headless emulator has no framebuffer.
 */
#include <stdio.h>
#include "smgr.h"

extern BITMAP	display;
extern LAYER	*newlayer();
extern int	*screen_addr();
extern void	gkLine();
extern void	background();
extern void	outline();
extern RECT	R_inset();
extern char	*malloc();

/* Set gk's graphics state to the engine defaults (== gctrl.c's gkReset, which
 * lives server-side and is not part of libhrgfx). */
static
rstgraph()
{
	gkPen.pn_Width  = 1;
	gkPen.pn_Height = 1;
	gkPen.pn_Pat    = FP_FORE;
	gkLogop  = L_TRUE;
	gkFpat   = FP_FORE;
	gkBpat   = FP_BACK;
	gkFcolor = BLACK;
	gkBcolor = WHITE;
	gkFont.fi_Id = SYS_FID;
}

/* Build one window at rectangle r (mirrors the drawing-relevant half of
 * SM_Reset once, then SM_Create) and leave gk loaded for it. */
static
makewin(r)
RECT r;
{
	WSTRUCT *wp;

	gkWid = 0;
	gkLorigin.x = gkLorigin.y = 0;
	gkCrect = r;
	gkPsize.x = r.corner.x - r.origin.x;
	gkPsize.y = r.corner.y - r.origin.y;
	gkWmgr = 2;
	gkEvmask = DEF_EVMASK;
	gkFlags = WT_FULLY_VIS;
	gkType = WT_OUTPUT;
	gk.wn_ascii = (int *)NULL;
	rstgraph();
	gkDp = r.origin;			/* SM_RstPoint */

	gkLayer = newlayer(r);
	gkLayer->base = screen_addr(r.origin.x, r.origin.y);
	gkLayer->width = 1024;

	wp = (WSTRUCT *)malloc(sizeof(WSTRUCT));
	wtbl[0] = wp;
	*wtbl[0] = gk;
	outline(0);				/* title/frame outline */
	gkCrect = R_inset(gkCrect, 7, 7);	/* content inside the frame */
	*wtbl[0] = gk;
}

main()
{
	RECT r;

	/* --- SM_Reset essentials: display bitmap + dithered desktop --- */
	display.base = SEG0;
	display.rect.origin.x = XMIN;
	display.rect.origin.y = YMIN;
	display.rect.corner.x = XMAX;
	display.rect.corner.y = YMAX;
	display.width = DIS_WIDTH;
	{ int i; for (i = 0; i < MAX_WINDOWS; i++) wtbl[i] = (WSTRUCT *)NULL; }
	DM_frontmost = DM_rearmost = (LAYER *)NULL;
	background(display.rect);

	/* --- one window --- */
	r.origin.x = 128;  r.origin.y = 96;
	r.corner.x = 128 + 448;
	r.corner.y = 96 + 320;
	makewin(r);

	/* --- draw into the content rect: a border box + a cross --- */
	gkLine(gkCrect.origin.x,   gkCrect.origin.y,   gkCrect.corner.x-1, gkCrect.origin.y,   0, L_FALSE);
	gkLine(gkCrect.corner.x-1, gkCrect.origin.y,   gkCrect.corner.x-1, gkCrect.corner.y-1, 0, L_FALSE);
	gkLine(gkCrect.corner.x-1, gkCrect.corner.y-1, gkCrect.origin.x,   gkCrect.corner.y-1, 0, L_FALSE);
	gkLine(gkCrect.origin.x,   gkCrect.corner.y-1, gkCrect.origin.x,   gkCrect.origin.y,   0, L_FALSE);
	gkLine(gkCrect.origin.x,   gkCrect.origin.y,   gkCrect.corner.x-1, gkCrect.corner.y-1, 0, L_FALSE);
	gkLine(gkCrect.origin.x,   gkCrect.corner.y-1, gkCrect.corner.x-1, gkCrect.origin.y,   0, L_FALSE);

	/* Done; keep the drawing on screen (no server to return to). */
	for (;;)
		;
}
