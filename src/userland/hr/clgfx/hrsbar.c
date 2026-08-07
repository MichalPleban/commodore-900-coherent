/*
 * hrsbar.c - hrgui common control: the vertical scrollbar (hrsbar.h).
 *
 * The library half of the contract described in hrsbar.h: it owns the LOOK
 * (arrow boxes, dithered trough, proportional bordered thumb -- the chrome
 * this machine's contemporaries drew) and the GESTURES (line, page, drag);
 * the client owns the HRSBAR and the meaning of sb_pos.  Everything is drawn
 * with cl_* primitives in content pixels, so the control needs no knowledge
 * of windows, dialogs or clipping -- clgfx already provides all three.
 *
 * Drawing is DELTA by default, like the widgets it will sit beside: moving
 * the thumb repaints the old thumb back to trough gray and paints the new
 * one, two small fills, so a drag never repaints the arrows or the whole
 * trough.  hr_sbdraw(sb, 1) paints everything -- first draw and expose.
 *
 * The trough is 50% gray via cl_fillrect val 3: the stipple is anchored to
 * SCREEN coordinates by the blitter, so the two delta fills always mesh with
 * the trough around them.
 */
#include "clgfx.h"
#include "hrsbar.h"

/* Thumb geometry from the document model, into *typ (top, content px) and
 * *thp (height).  Proportional -- the thumb is to the trough as the page is
 * to the document -- floored at HRSB_MINTH so a long document still leaves
 * something to grab.  A document that fits entirely fills the trough. */
static
sbgeom(sb, typ, thp)
HRSBAR *sb;
int *typ, *thp;
{
	int tr, h, scr;

	tr = sb->sb_h - 2 * HRSB_W;		/* trough height          */
	scr = sb->sb_total - sb->sb_page;	/* scrollable span, units */
	if ( scr <= 0 || sb->sb_total <= 0 )
	{
		*typ = sb->sb_y + HRSB_W;
		*thp = tr;
		return 0;
	}
	h = (int)((long)tr * sb->sb_page / sb->sb_total);
	if ( h < HRSB_MINTH ) h = HRSB_MINTH;
	if ( h > tr ) h = tr;
	/* pos maps over the leftover travel, so pos==scr puts it flush bottom */
	*typ = sb->sb_y + HRSB_W + (int)((long)sb->sb_pos * (tr - h) / scr);
	*thp = h;
	return 0;
}

static
sbclamp(sb)
HRSBAR *sb;
{
	int max;

	max = sb->sb_total - sb->sb_page;
	if ( max < 0 ) max = 0;
	if ( sb->sb_pos > max ) sb->sb_pos = max;
	if ( sb->sb_pos < 0 )   sb->sb_pos = 0;
	return 0;
}

/* One 16x16 arrow box with its top-left at (x,y0): white plate, 1 px outline,
 * solid black arrow (5-row triangular head + 4 px stem), pointing up or down. */
static
sbbox(x, y0, up)
{
	register int i;

	cl_fillrect(x, y0, x + HRSB_W, y0 + HRSB_W, 1);
	cl_fillrect(x, y0, x + HRSB_W, y0 + 1, 0);
	cl_fillrect(x, y0 + HRSB_W - 1, x + HRSB_W, y0 + HRSB_W, 0);
	cl_fillrect(x, y0, x + 1, y0 + HRSB_W, 0);
	cl_fillrect(x + HRSB_W - 1, y0, x + HRSB_W, y0 + HRSB_W, 0);
	for ( i = 0; i < 5; i++ )
		cl_fillrect(x + 7 - i, up ? y0 + 3 + i : y0 + 12 - i,
			    x + 9 + i, up ? y0 + 4 + i : y0 + 13 - i, 0);
	if ( up )
		cl_fillrect(x + 6, y0 + 8, x + 10, y0 + 13, 0);
	else
		cl_fillrect(x + 6, y0 + 3, x + 10, y0 + 8, 0);
	return 0;
}

/* Draw the bar.  force = everything (arrows, rails, trough, thumb); else only
 * the thumb, and only if it actually moved or resized since the last draw. */
hr_sbdraw(sb, force)
HRSBAR *sb;
{
	int x, y, h, ty, th;

	x = sb->sb_x;  y = sb->sb_y;  h = sb->sb_h;
	sbgeom(sb, &ty, &th);
	if ( !force && ty == sb->sb_ty && th == sb->sb_th )
		return 0;
	if ( force )
	{
		sbbox(x, y, 1);
		sbbox(x, y + h - HRSB_W, 0);
		/* side rails, then the gray bed between them */
		cl_fillrect(x, y + HRSB_W, x + 1, y + h - HRSB_W, 0);
		cl_fillrect(x + HRSB_W - 1, y + HRSB_W,
			    x + HRSB_W, y + h - HRSB_W, 0);
		cl_fillrect(x + 1, y + HRSB_W,
			    x + HRSB_W - 1, y + h - HRSB_W, 3);
	}
	else	/* return the old thumb's pixels to the trough */
		cl_fillrect(x + 1, sb->sb_ty,
			    x + HRSB_W - 1, sb->sb_ty + sb->sb_th, 3);
	/* the thumb: white plate with a 1 px outline, between the rails */
	cl_fillrect(x + 1, ty, x + HRSB_W - 1, ty + th, 1);
	cl_fillrect(x + 1, ty, x + HRSB_W - 1, ty + 1, 0);
	cl_fillrect(x + 1, ty + th - 1, x + HRSB_W - 1, ty + th, 0);
	cl_fillrect(x + 1, ty, x + 2, ty + th, 0);
	cl_fillrect(x + HRSB_W - 2, ty, x + HRSB_W - 1, ty + th, 0);
	sb->sb_ty = ty;  sb->sb_th = th;
	return 0;
}

hr_sbhit(sb, px, py)
HRSBAR *sb;
{
	return px >= sb->sb_x && px < sb->sb_x + HRSB_W &&
	       py >= sb->sb_y && py < sb->sb_y + sb->sb_h;
}

/* A left press at content y (already known to be in the bar): arrows move a
 * unit, the trough a page less one (the classic line of context), the thumb
 * starts a drag.  Returns 1 if sb_pos changed -- a drag start returns 0 and
 * the position then follows hr_sbmotion. */
hr_sbpress(sb, py)
HRSBAR *sb;
{
	int old, ty, th;

	old = sb->sb_pos;
	sbgeom(sb, &ty, &th);
	if ( py < sb->sb_y + HRSB_W )
		sb->sb_pos--;			/* up: toward the beginning */
	else if ( py >= sb->sb_y + sb->sb_h - HRSB_W )
		sb->sb_pos++;
	else if ( py < ty )
		sb->sb_pos -= sb->sb_page - 1;
	else if ( py >= ty + th )
		sb->sb_pos += sb->sb_page - 1;
	else
	{
		sb->sb_drag = 1;
		sb->sb_grab = py - ty;
	}
	sbclamp(sb);
	return sb->sb_pos != old;
}

/* Track a drag: put the thumb's grabbed pixel back under the pointer and read
 * the position off the travel, rounded to the nearest unit so the thumb does
 * not creep during a slow drag.  Returns 1 if sb_pos changed. */
hr_sbmotion(sb, py)
HRSBAR *sb;
{
	int old, tr, ty, th, scr, rng;
	long p;

	if ( !sb->sb_drag )
		return 0;
	old = sb->sb_pos;
	sbgeom(sb, &ty, &th);
	tr = sb->sb_h - 2 * HRSB_W;
	scr = sb->sb_total - sb->sb_page;
	rng = tr - th;
	if ( scr > 0 && rng > 0 )
	{
		p = (long)(py - sb->sb_grab - (sb->sb_y + HRSB_W));
		p = (p * scr + rng / 2) / rng;
		sb->sb_pos = (int)p;
	}
	sbclamp(sb);
	return sb->sb_pos != old;
}

hr_sbrelease(sb)
HRSBAR *sb;
{
	sb->sb_drag = 0;
	return 0;
}
