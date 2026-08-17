/*
 * hrsbar.h - hrgui common control: the vertical scrollbar (clgfx/hrsbar.c).
 *
 * A scrollbar is a struct the CLIENT owns plus four calls, exactly like the
 * dialog widget kit (hrdlg.h): the library draws the chrome and turns pointer
 * events into a new position, the client turns the position into a view.  It
 * draws through the cl_* primitives, so the same control works on a window
 * surface (a terminal, an editor) and, under cl_dopen, on a dialog surface.
 *
 * The bar models a DOCUMENT of sb_total units (lines, usually) of which
 * sb_page are visible at once; sb_pos is the first visible unit, clamped to
 * 0..sb_total-sb_page.  The arrows -- both stacked at the BOTTOM of the bar,
 * up above down, as NeXT drew them -- move one unit, the trough one page, and
 * the thumb -- proportional, as GEM drew it -- tracks the pointer.  When the
 * whole document fits (sb_total <= sb_page) the thumb fills the trough and
 * nothing moves.
 *
 * Event wiring, given E_BUTTON/E_MOTION under the server's implicit grab:
 *     press  (left, hr_sbhit says it is ours):  changed = hr_sbpress(sb, y);
 *     motion (while sb->sb_drag):               changed = hr_sbmotion(sb, y);
 *     release:                                  hr_sbrelease(sb);
 * `changed' means sb_pos moved: re-derive the view and repaint, then
 * hr_sbdraw(sb, 0) to move the thumb.  hr_sbdraw(sb, 1) repaints the whole
 * bar (first draw, or expose damage touching it).
 *
 * All coordinates are CONTENT pixels of whatever surface is current.  The bar
 * is HRSB_W wide -- 16 px, ONE VRAM word, so content laid out beside a bar at
 * x 0 keeps its byte/word alignment (why zterm's text starts at x 16).
 */
#ifndef HRSBAR_H
#define HRSBAR_H

#define	HRSB_W	16		/* bar width, px: one VRAM word              */
#define	HRSB_MINTH 8		/* minimum thumb height, px                  */

typedef struct {
	int	sb_x, sb_y;	/* bar top-left, content px                  */
	int	sb_h;		/* bar height, px (>= 2*HRSB_W + HRSB_MINTH) */
	int	sb_total;	/* document length, units                    */
	int	sb_page;	/* units visible at once                     */
	int	sb_pos;		/* first visible unit, 0..total-page         */
	int	sb_drag;	/* 1 = thumb grabbed (read-only to clients)  */
	/* private */
	int	sb_ty, sb_th;	/* thumb top/height as last drawn            */
	int	sb_grab;	/* grab offset within the thumb, px          */
} HRSBAR;

extern int	hr_sbdraw();	/* hr_sbdraw(sb, force): thumb delta, or all */
extern int	hr_sbhit();	/* hr_sbhit(sb, x, y): 1 = point is in the bar */
extern int	hr_sbpress();	/* hr_sbpress(sb, y) -> 1 if sb_pos changed;
				 * a press on the thumb starts a drag instead */
extern int	hr_sbmotion();	/* hr_sbmotion(sb, y) -> 1 if sb_pos changed */
extern int	hr_sbrelease();	/* end the drag (call on every release)      */

#endif /* HRSBAR_H */
