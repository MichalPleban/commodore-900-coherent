/*
 * hrdlg.c - hrgui client-side modal dialog: open/close + the widget kit.
 *
 * See inc/hrdlg.h for the contract and an example.  The dialog itself is a
 * server-side overlay (wire.h C_DLGOPEN): the server saves the pixels under a
 * centred box, draws the frame, publishes the interior as this client's
 * drawable surface (shmem.h SHM_DLGSURF) and routes ALL input here as E_D*
 * events until C_DLGCLOSE.  This file supplies the client half: the blocking
 * open/close calls, the widget renderer, and the modal event loop.
 *
 * Everything is drawn with the ordinary clgfx primitives -- after cl_dopen()
 * they target the dialog surface -- so an application can freely mix its own
 * cl_* drawing (a preview canvas, a file list) with the widgets here.
 *
 * MODAL means this loop owns the ring: every event that is not E_D* or E_QUIT
 * is discarded.  That is safe because the client could not have repainted anyway
 * (its window is frozen under the overlay), and the server sends a
 * full-content E_EXPOSE when the dialog closes, which subsumes everything
 * dropped; if anything WAS dropped on a path with no close-expose (a refused
 * open), the ring's overflow flag is set so the application's own
 * hr_evover() check triggers the same full repaint.
 *
 * NOT linked into every client (Makefile HRDLG, like HRSEL): only consumers
 * name it.
 */
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"

static int	dlgw, dlgh;	/* granted interior size                     */
static int	focusw = -1;	/* index of the focused DW_TEXT, or -1       */

/* Copy at most n-1 bytes of s ("" for NULL) and NUL-fill the rest -- the
 * record goes on the wire, so no stack garbage (hrapp.c setstr's twin). */
static
dsetstr(d, s, n)
register char *d, *s;
register int n;
{
	if ( s == (char *)0 )
		s = "";
	while ( n > 1 && *s )
	{
		*d++ = *s++;
		n--;
	}
	while ( n-- > 0 )
		*d++ = 0;
}

/* ------------------------------------------------------------------ */
/* open / close                                                       */
/* ------------------------------------------------------------------ */

/* Ask the server for a modal dialog overlay of interior *pw x *ph and BLOCK
 * until it answers; the request is queued behind any dialog already up, so
 * this can wait.  On success the granted size is written back, the
 * primitives are switched to the dialog surface, and the caller draws.
 * Returns 0, or -1 (not under zview / write failed / refused: queue
 * overflow), or -2 (E_QUIT arrived -- the window is gone; clean up and
 * exit). */
hr_dlgopen(pw, ph)
int *pw, *ph;
{
	HRDLGO d;
	WMSG e;
	int dropped, ret;

	if ( hr_wid() < 0 )
		return -1;
	d.hd_type = C_DLGOPEN;
	d.hd_wid = hr_wid();
	d.hd_w = *pw;
	d.hd_h = *ph;
	d.hd_res = 0;
	dsetstr(d.hd_pad, (char *)0, sizeof(d.hd_pad));
	if ( write(HR_CMDFD, (char *)&d, sizeof(d)) != sizeof(d) )
		return -1;
	dropped = 0;
	for (;;)
	{
		hr_evwait(hr_wid());	/* signal-safe: an early return re-waits */
		while ( hr_evget(hr_wid(), (short *)&e) )
		{
			if ( e.wm_type == E_DLGOPEN )
			{
				if ( e.wm_arg[0] )
				{
					*pw = dlgw = e.wm_arg[1];
					*ph = dlgh = e.wm_arg[2];
					focusw = -1;
					cl_dopen();
					return 0;	/* close-expose covers drops */
				}
				ret = -1;	/* refused: no close-expose comes */
				goto out;
			}
			if ( e.wm_type == E_QUIT )
			{
				ret = -2;
				goto out;
			}
			dropped = 1;
		}
	}
out:
	if ( dropped )
		hr_evq(hr_wid())->eq_over = 1;	/* app repaints via hr_evover() */
	return ret;
}

/* Close: tell the server, wait for the restore (the overlay word IS the
 * "you may draw again" gate, so no reply event is needed -- when it clears,
 * the screen is back), then retarget the primitives at the window.  The
 * app's repaint is driven by the server's full-content E_EXPOSE. */
hr_dlgclose()
{
	long spin;

	hr_cmd(C_DLGCLOSE);
	for ( spin = 0; spin < 2000000L; spin++ )
		if ( hr_glob()->overlay == 0 )
			break;
	cl_dclose();
	return 0;
}

/* ------------------------------------------------------------------ */
/* widget rendering                                                   */
/* ------------------------------------------------------------------ */

/* 1-px black border around (x,y) w x h, dialog-interior px. */
static
dborder(x, y, w, h)
{
	cl_fillrect(x, y, x + w, y + 1, 0);
	cl_fillrect(x, y + h - 1, x + w, y + h, 0);
	cl_fillrect(x, y + 1, x + 1, y + h - 1, 0);
	cl_fillrect(x + w - 1, y + 1, x + w, y + h - 1, 0);
}

/* Stepped drop shadow off the right + bottom of (x,y) w x h, d px deep: the
 * mini version of the window/dialog shadow (zview outline/dlg_shadow), so a
 * button sits raised off the card exactly as the card sits off the desktop.
 * The margin is cleared white first, so redrawing a widget in place is
 * idempotent. */
static
dshadow(x, y, w, h, d)
{
	int k;

	cl_fillrect(x + w, y, x + w + d, y + h + d, 1);
	cl_fillrect(x, y + h, x + w + d, y + h + d, 1);
	for ( k = 0; k < d; k++ )
	{
		cl_fillrect(x + w + k, y + k,		/* right band */
			    x + w + k + 1, y + h + 1 + k, 0);
		cl_fillrect(x + k, y + h + k,		/* bottom band */
			    x + w + 1 + k, y + h + k + 1, 0);
	}
}

/* Effective on-screen extent of widget wp -> x0/y0/x1/y1.  DW_CHECK/DW_RADIO
 * and DW_LABEL may leave dw_w/dw_h 0: their extent follows the glyph box and
 * the label text (and for the toggles it INCLUDES the label -- clicking the
 * word must work, nobody aims at a 13-px square). */
static
dextent(wp, px0, py0, px1, py1)
HRWIDGET *wp;
int *px0, *py0, *px1, *py1;
{
	int fw, fh, w, h;

	fw = hr_font(SHM_FUI)->cellw;
	fh = hr_font(SHM_FUI)->cellh;
	w = wp->dw_w;
	h = wp->dw_h;
	if ( wp->dw_type == DW_CHECK || wp->dw_type == DW_RADIO )
	{
		if ( w <= 0 && wp->dw_label )
			w = DLG_CHK + 8 + strlen(wp->dw_label) * fw;
		if ( w <= 0 )
			w = DLG_CHK;
		if ( h <= 0 )
			h = fh > DLG_CHK ? fh : DLG_CHK;
	}
	else
	{
		if ( w <= 0 && wp->dw_label )
			w = strlen(wp->dw_label) * fw;
		if ( h <= 0 )
			h = fh;
	}
	*px0 = wp->dw_x;
	*py0 = wp->dw_y;
	*px1 = wp->dw_x + w;
	*py1 = wp->dw_y + h;
}

/* XOR-invert a button's interior: the self-erasing pressed-state highlight
 * (arm and disarm are the same call). */
static
dinvert(wp)
HRWIDGET *wp;
{
	cl_fillrect(wp->dw_x + 1, wp->dw_y + 1,
		    wp->dw_x + wp->dw_w - 1, wp->dw_y + wp->dw_h - 1, 2);
}

/* Draw one widget (fully: body, border, label, state), so a state change
 * just redraws the widget.  `focus' matters only to DW_TEXT. */
static
dwdraw(wp, focus)
HRWIDGET *wp;
{
	int fw, fh, x, y, w, h, lw, cx, ly;

	fw = hr_font(SHM_FUI)->cellw;
	fh = hr_font(SHM_FUI)->cellh;
	x = wp->dw_x;  y = wp->dw_y;
	w = wp->dw_w;  h = wp->dw_h;
	switch ( wp->dw_type )
	{
	case DW_LABEL:
		cl_ptext(SHM_FUI, x, y, wp->dw_label);
		break;

	/* Label placement below carries a +1,+1 nudge on top of the plain
	 * integer centring: the FUI glyphs sit high-left in their 9x16 cells,
	 * so exact arithmetic centring reads a pixel off (recurring gotcha). */
	case DW_BUTTON:
		cl_fillrect(x + 1, y + 1, x + w - 1, y + h - 1, 1);
		dborder(x, y, w, h);
		lw = strlen(wp->dw_label) * fw;
		cl_ptext(SHM_FUI, x + (w - lw) / 2 + 1, y + (h - fh) / 2 + 1,
			 wp->dw_label);
		if ( wp->dw_flags & DWF_DEF )
		{	/* default: a second ring -- and the shadow falls from
			 * the RING, since ring+button read as one raised object */
			dborder(x - 3, y - 3, w + 6, h + 6);
			dshadow(x - 3, y - 3, w + 6, h + 6, DLG_BSHAD);
		}
		else
			dshadow(x, y, w, h, DLG_BSHAD);
		break;

	case DW_TEXT:
		cl_fillrect(x + 1, y + 1, x + w - 1, y + h - 1, 1);
		dborder(x, y, w, h);
		ly = y + (h - fh) / 2 + 1;
		cl_ptext(SHM_FUI, x + DLG_THPAD + 2, ly, wp->dw_buf);
		if ( focus )
		{			/* caret: a bar after the last char */
			cx = x + DLG_THPAD + 2 + strlen(wp->dw_buf) * fw;
			if ( cx < x + w - 2 )
				cl_fillrect(cx, ly, cx + 2, ly + fh, 0);
		}
		break;

	case DW_CHECK:
		cl_fillrect(x + 1, y + 1, x + DLG_CHK - 1, y + DLG_CHK - 1, 1);
		dborder(x, y, DLG_CHK, DLG_CHK);
		if ( wp->dw_val )
		{			/* X mark: the two diagonals */
			cl_line(x + 3, y + 3, x + DLG_CHK - 4, y + DLG_CHK - 4, 0);
			cl_line(x + DLG_CHK - 4, y + 3, x + 3, y + DLG_CHK - 4, 0);
		}
		if ( wp->dw_label )
			cl_ptext(SHM_FUI, x + DLG_CHK + 9,
				 y + (DLG_CHK - fh) / 2 + 1, wp->dw_label);
		break;

	case DW_RADIO:
		cl_fillrect(x + 1, y + 1, x + DLG_CHK - 1, y + DLG_CHK - 1, 1);
		dborder(x, y, DLG_CHK, DLG_CHK);
		if ( wp->dw_val )	/* the dot */
			cl_fillrect(x + 4, y + 4,
				    x + DLG_CHK - 4, y + DLG_CHK - 4, 0);
		if ( wp->dw_label )
			cl_ptext(SHM_FUI, x + DLG_CHK + 9,
				 y + (DLG_CHK - fh) / 2 + 1, wp->dw_label);
		break;
	}
}

/* (Re)draw every widget -- call once after hr_dlgopen (and any custom canvas
 * drawing underneath them).
 *
 * The FIRST text field is focused automatically: nearly every dialog with a
 * field exists to have that field typed into (file names, search patterns),
 * so it must not cost a mouse trip before typing works.  Only when nothing is
 * focused yet -- a click elsewhere, or a Tab, moved the focus on purpose and
 * a later redraw must not snap it back. */
hr_dlgdraw(wg, n)
HRWIDGET wg[];
{
	int i;

	if ( focusw < 0 )
		for ( i = 0; i < n; i++ )
			if ( wg[i].dw_type == DW_TEXT )
			{
				focusw = i;
				break;
			}
	for ( i = 0; i < n; i++ )
		dwdraw(&wg[i], i == focusw);
	return 0;
}

/* ------------------------------------------------------------------ */
/* the modal loop                                                     */
/* ------------------------------------------------------------------ */

/* Which interactive widget (not a label) contains interior point (x,y). */
static
dhit(wg, n, x, y)
HRWIDGET wg[];
{
	int i, x0, y0, x1, y1;

	for ( i = 0; i < n; i++ )
	{
		if ( wg[i].dw_type == DW_LABEL )
			continue;
		dextent(&wg[i], &x0, &y0, &x1, &y1);
		if ( x >= x0 && x < x1 && y >= y0 && y < y1 )
			return i;
	}
	return -1;
}

/* The index of the widget carrying flag f (with DWF_END), or -1. */
static
dflagged(wg, n, f)
HRWIDGET wg[];
{
	int i;

	for ( i = 0; i < n; i++ )
		if ( wg[i].dw_type == DW_BUTTON &&
		     (wg[i].dw_flags & f) && (wg[i].dw_flags & DWF_END) )
			return i;
	return -1;
}

/* Run the dialog: track the widgets until a DWF_END button is activated and
 * return its index, or -1 on E_QUIT (the window is gone: close up and exit).
 * Buttons arm on press, disarm as the pointer leaves, commit on release
 * inside; toggles flip on press; a click in a text field focuses it (the
 * first field is focused from the start, hr_dlgdraw) and Tab moves the focus
 * to the next field; keys go to the focused field ('\b' rubs out), Return
 * fires the DWF_DEF button and Esc the DWF_CANCEL one.  The caller still
 * owns the widget array: dw_val / dw_buf hold the outcome. */
hr_dlgrun(wg, n)
HRWIDGET wg[];
{
	WMSG e;
	int arm, in, hit, i, c, len, fw, ret, dropped;
	int x0, y0, x1, y1;
	HRWIDGET *wp;

	fw = hr_font(SHM_FUI)->cellw;
	arm = -1;  in = 0;  ret = -3;  dropped = 0;
	while ( ret == -3 )
	{
		hr_evwait(hr_wid());
		while ( ret == -3 && hr_evget(hr_wid(), (short *)&e) )
		{
			if ( e.wm_type == E_DBUTTON &&
			     (e.wm_arg[2] & EB_LEFT) )
			{			/* left press */
				hit = dhit(wg, n, e.wm_arg[0], e.wm_arg[1]);
				if ( hit < 0 )
					continue;
				wp = &wg[hit];
				switch ( wp->dw_type )
				{
				case DW_BUTTON:
					arm = hit;
					in = 1;
					dinvert(wp);
					break;
				case DW_CHECK:
					wp->dw_val = !wp->dw_val;
					dwdraw(wp, 0);
					break;
				case DW_RADIO:
					for ( i = 0; i < n; i++ )
						if ( i != hit &&
						     wg[i].dw_type == DW_RADIO &&
						     wg[i].dw_grp == wp->dw_grp &&
						     wg[i].dw_val )
						{
							wg[i].dw_val = 0;
							dwdraw(&wg[i], 0);
						}
					if ( !wp->dw_val )
					{
						wp->dw_val = 1;
						dwdraw(wp, 0);
					}
					break;
				case DW_TEXT:
					if ( focusw != hit )
					{
						i = focusw;
						focusw = hit;
						if ( i >= 0 )
							dwdraw(&wg[i], 0);
						dwdraw(wp, 1);
					}
					break;
				}
			}
			else if ( e.wm_type == E_DBUTTON )
			{			/* left release */
				if ( arm >= 0 )
				{
					if ( in )
						dinvert(&wg[arm]);
					if ( in && (wg[arm].dw_flags & DWF_END) )
						ret = arm;
					arm = -1;  in = 0;
				}
			}
			else if ( e.wm_type == E_DMOTION )
			{
				if ( arm >= 0 )
				{
					dextent(&wg[arm], &x0, &y0, &x1, &y1);
					i = e.wm_arg[0] >= x0 && e.wm_arg[0] < x1 &&
					    e.wm_arg[1] >= y0 && e.wm_arg[1] < y1;
					if ( i != in )
					{
						dinvert(&wg[arm]);
						in = i;
					}
				}
			}
			else if ( e.wm_type == E_DKEY )
			{
				c = e.wm_arg[0] & 0xff;
				if ( c == '\r' || c == '\n' )
				{
					if ( (i = dflagged(wg, n, DWF_DEF)) >= 0 )
						ret = i;
				}
				else if ( c == 033 )
				{
					if ( (i = dflagged(wg, n, DWF_CANCEL)) >= 0 )
						ret = i;
				}
				else if ( c == '\t' )
				{	/* Tab: focus the next text field, wrapping */
					hit = -1;
					for ( i = 1; i <= n; i++ )
						if ( wg[(focusw + i) % n].dw_type
						     == DW_TEXT )
						{
							hit = (focusw + i) % n;
							break;
						}
					if ( hit >= 0 && hit != focusw )
					{
						i = focusw;
						focusw = hit;
						if ( i >= 0 )
							dwdraw(&wg[i], 0);
						dwdraw(&wg[hit], 1);
					}
				}
				else if ( focusw >= 0 )
				{
					wp = &wg[focusw];
					len = strlen(wp->dw_buf);
					if ( c == '\b' || c == 0x7f )
					{
						if ( len > 0 )
						{
							wp->dw_buf[len - 1] = 0;
							dwdraw(wp, 1);
						}
					}
					else if ( c >= 0x20 && c <= 0x7e &&
						  len < wp->dw_len - 1 &&
						  DLG_THPAD + 1 + (len + 1) * fw
						      < wp->dw_w - 2 - DLG_THPAD )
					{
						wp->dw_buf[len] = c;
						wp->dw_buf[len + 1] = 0;
						dwdraw(wp, 1);
					}
				}
			}
			else if ( e.wm_type == E_QUIT )
				ret = -1;
			else
				dropped = 1;	/* stale pre-dialog event */
		}
	}
	if ( dropped )
		hr_evq(hr_wid())->eq_over = 1;	/* belt and braces: the close- */
						/* expose normally covers this */
	return ret;
}
