/*
 * clgfx.c - hrgui client-side direct-render draw library (GUI.md Model A).
 *
 * The client blits its own content straight to VRAM (segments 0x3A/0x3B) through
 * the salvaged engine bitblt -- the SAME asm blitter the server uses, so a whole
 * glyph row is shifted into place in one masked word op (never per-pixel).  It
 * clips to the visible-region list the server publishes in the shared VRAM tail
 * (shmem.h), read under a seqlock.  Fonts live in the tail too, so nothing is
 * relinked and no pixel or glyph ever crosses IPC.
 *
 * Only bitblt + its asm inner loops + rmath (R_point_in) + masks (texture[]) +
 * globals (BLT_* state) are pulled from libhrgfx here -- NOT the server-only
 * layer/clip/daemon code.  Blits go to a client-local `display' BITMAP based at
 * SEG0 (offset 0), the path proven safe in the server (a mid-VRAM destination
 * base faults in this toolchain; a SEG0-based dest spans the 512-line split
 * fine, since the emulator's VRAM is contiguous across it).
 */
#include "smgr.h"		/* BITMAP/BLTSTRUCT/RECT/POINT, texture[], L_*, CIOMSE* */
#include "shmem.h"

extern int	bitblt();
extern int	*texture[];

#define XMAXP	1024
#define YMAXP	800

static BITMAP	cldisp;			/* the whole framebuffer, base SEG0     */
static HRSURF	S;			/* cached clip descriptor for my window */
static int	mywid;
static int	hrfd = -1;		/* /dev/hr fd for cursor on/off         */
static int	curhid;			/* 1 while we have the cursor hidden    */
static int	lastseq = -1;		/* seqlock value of the last sync'd S   */
static int	indlg;			/* 1 = primitives target the DIALOG     */
					/* surface (hr_dlgsurf), not the window */

/* Cursor sprite box (framebuffer coords), captured once per batch in cl_begin;
 * a blit hides the driver's XOR cursor only when it overlaps this. */
static int	curbx0, curby0, curbx1, curby1;
static int	cureligible;		/* 1 = we may hide the driver cursor    */

/* VRAM word address of pixel (x,y), spanning the 512-line SEG0/SEG1 split --
 * a local copy of the engine's screen_addr so clgfx needs no layer.o. */
static int *
cl_scraddr(x, y)
{
	long addr;

	addr = ((long)y) << 7;			/* y * 128 bytes */
	addr += ((long)(x >> 4)) << 1;		/* + (x/16) words */
	if ( addr & 0xffff0000L )
	{
		addr &= 0x0000ffffL;
		addr |= (long)0x3b000000L;	/* SEG1 (lines 512..) */
	}
	else
		addr |= (long)0x3a000000L;	/* SEG0 (lines 0..511) */
	return (int *)addr;
}

static int
cl_words(l, r)
{
	if ( r <= l )
		return 0;
	l &= 0xfff0;
	if ( r & 0x000f )
		r = (r & 0xfff0) + 0x0010;
	return (r - l) >> 4;
}

cl_init(wid)
{
	mywid = wid;
	hr_setdraw(wid, 0);			/* clear any stale fast-path drain flag */
	cldisp.base = (int *)0x3a000000L;	/* SEG0, offset 0 */
	cldisp.rect.origin.x = 0;  cldisp.rect.origin.y = 0;
	cldisp.rect.corner.x = XMAXP;  cldisp.rect.corner.y = YMAXP;
	cldisp.width = XMAXP;
	hrfd = open("/dev/dmgr", 2);		/* for cursor on/off (any node) */
}

/* Refresh the cached clip descriptor with a seqlock read: retry while the
 * server is mid-write (seq odd) or seq changed under us.  Cheap fast-path: if
 * seq is even and unchanged since our last sync, S already holds that snapshot,
 * so callers can re-sync before EVERY primitive (not just once per batch) to
 * pick up a z-order/geometry change the instant the server publishes it -- which
 * is what stops a busy client painting into a window newly stacked on top. */
static
cl_sync()
{
	HRSURF *sp;
	int s1, s2, i;

	sp = indlg ? hr_dlgsurf() : hr_surf(mywid);
	s1 = sp->seq;
	if ( !(s1 & 1) && s1 == lastseq )
		return;
	do {
		s1 = sp->seq;
		S.mapped = sp->mapped;
		S.ox = sp->ox;  S.oy = sp->oy;  S.cw = sp->cw;  S.ch = sp->ch;
		S.nvis = sp->nvis;
		if ( S.nvis > SHM_MAXVIS )
			S.nvis = SHM_MAXVIS;
		for ( i = 0; i < S.nvis; i++ )
			S.vis[i] = sp->vis[i];
		s2 = sp->seq;
	} while ( (s1 & 1) || s1 != s2 );
	lastseq = s1;
}

cl_mapped()	{ return S.mapped; }
cl_cw()		{ return S.cw; }
cl_ch()		{ return S.ch; }

/* Re-read the clip descriptor (a public wrapper over the seqlock read) and report
 * its generation, so a client can cheaply tell that the server has hidden / shown
 * / raised / resized its window SINCE the client's last full repaint -- and force
 * one full repaint instead of patching incrementally over a stale or blank surface
 * (the "flood draws scattered characters onto a just-unhidden window" bug).  Cheap
 * when nothing changed (cl_sync fast-paths on an unchanged, even seqlock). */
cl_refresh()	{ cl_sync(); }
cl_gen()	{ return lastseq; }

/* 1 while a transient overlay (pop-up menu / dialog) is on screen: clients must
 * skip drawing so they do not paint over it (it is not a layer, so the clip
 * descriptor cannot exclude it).  The ONE exception: a client whose own dialog
 * is up (overlay == OV_DLG|wid) may draw while targeting the dialog surface --
 * its main window stays frozen like everyone else's (the box may cover it). */
cl_frozen()
{
	register int ov;

	if ( (ov = hr_glob()->overlay) == 0 )
		return 0;
	if ( indlg && ov == (OV_DLG | mywid) )
		return 0;
	return 1;
}

/* Switch the primitives between the window surface and the dialog surface
 * (shmem.h SHM_DLGSURF).  Called by the widget library (hrdlg.c) around the
 * dialog's lifetime; invalidating lastseq forces a full seqlock re-read of
 * whichever descriptor is now current. */
cl_dopen()	{ indlg = 1;  lastseq = -1;  cl_sync(); }
cl_dclose()	{ indlg = 0;  lastseq = -1;  cl_sync(); }

cl_fullyvis()
{
	return S.mapped && S.nvis == 1 &&
	       S.vis[0].x0 == S.ox && S.vis[0].y0 == S.oy &&
	       S.vis[0].x1 == S.ox + S.cw && S.vis[0].y1 == S.oy + S.ch;
}

/* cl_begin/cl_end used to hide the cursor and sync the clip once for a whole
 * repaint batch.  That is now done PER PRIMITIVE (cl_pbegin/cl_pend below) under
 * the global drawing lock, so these are just batch markers kept for the client
 * API (zterm/zclock bracket their repaints with them). */
cl_begin()	{ }
cl_end()	{ }

/* Enter a drawing primitive whose bounding box is the content-relative rect
 * (cx0,cy0)-(cx1,cy1).  Returns 1 if it took the global lock (SLOW path) or 0 if
 * it is drawing lock-free (FAST path); pass that value to cl_pend.
 *
 * FAST path (no lock, no cursor ioctls): the window is fully visible -- so its
 * pixels are disjoint from every other window and the desktop, and a lock-free
 * blit can neither corrupt nor be corrupted by a concurrent draw -- AND no server
 * overlay (menu/ghost) or layer op is in flight (hr_glob overlay/stacking) AND
 * this primitive does not touch the driver's XOR cursor sprite (the one shared
 * thing that roams over a topmost window).  This is the steady state of the
 * focused, fully-visible terminal, and it skips the lock entirely -- so a flood
 * of text no longer contends with the clock's per-line locking, the driver
 * cursor, or the (idle) server.
 *
 * SLOW path (take the lock + re-sync under it, exactly as the original): anything
 * else -- partly covered, a server op in flight, or the primitive overlaps the
 * cursor box -- serialises under the global lock and coordinates the cursor.
 *
 * The one race the fast path cannot exclude -- a raise/cover that de-topmosts us
 * partway through a single primitive -- is bounded to that one primitive (the
 * server sets hr_glob()->stacking around every layer op, so the NEXT primitive
 * already falls back to the lock), and it self-heals: any such change also fires
 * an E_EXPOSE, so we repaint the region clean immediately after. */
static int
cl_pbegin(cx0, cy0, cx1, cy1)
{
	HRGLOB *g;
	int bx0, by0, bx1, by1;

	cl_sync();			/* seqlock read -- valid without the lock */
	g = hr_glob();
	/* Dead session (the server's watchdog cleared the magic): the screen
	 * belongs to the restored text console now.  Painting on it is the one
	 * thing we must not do, and there is nobody left to draw FOR -- exit.
	 * Catches the clients whose idle point is not hr_evwait (zterm's main
	 * draws off its pty mux); hrlock.c hr_evwait catches the rest. */
	if ( g->magic != HR_MAGIC )
		exit(1);
	/* 16x16 sprite from the hotspot, padded 1px so a blit that just grazes it
	 * still hides it (a shown cursor XOR-clipped by a blit leaves a stray arrow). */
	curbx0 = g->curx - 1;   curby0 = g->cury - 1;
	curbx1 = g->curx + 17;  curby1 = g->cury + 17;
	bx0 = S.ox + cx0;  by0 = S.oy + cy0;
	bx1 = S.ox + cx1;  by1 = S.oy + cy1;
	/* Candidate for the lock-free fast path: fully visible, no menu overlay, and
	 * clear of the cursor sprite. */
	if ( S.mapped && !g->overlay && cl_fullyvis() &&
	     !(bx0 < curbx1 && bx1 > curbx0 && by0 < curby1 && by1 > curby0) )
	{
		/* Dekker handshake with the server's srvlock (which sets `stacking'
		 * then drains SHM_INDRAW): announce we are drawing lock-free BEFORE we
		 * test `stacking'.  If a restack is starting, either we see stacking and
		 * step aside, or the server sees our flag and waits -- never both draw.
		 * With the flag held the server cannot restack (it drains on us), so the
		 * clip we re-sync here is pinned for the whole primitive; it clears in
		 * cl_pend.  (hr_setdraw is an extern call: it orders the store before the
		 * stacking read.) */
		hr_setdraw(mywid, 1);
		if ( !g->stacking )
		{
			cl_sync();		/* clip now pinned -- server will drain on us */
			if ( S.mapped && cl_fullyvis() )
			{
				curhid = 0;
				cureligible = 0;	/* fast path never touches the cursor */
				return 0;		/* flag stays set until cl_pend */
			}
		}
		hr_setdraw(mywid, 0);		/* did not qualify / a restack is in flight */
	}
	hr_lock(hr_lockw());
	cl_sync();			/* clip now guaranteed stable for this primitive */
	curhid = 0;
	cureligible = ( hrfd >= 0 );
	return 1;
}

/* Hide the driver's XOR cursor if this framebuffer-coord blit rect overlaps the
 * cursor sprite -- lazily and at most once per primitive; cl_pend restores it. */
static
cl_hidecur(x0, y0, x1, y1)
{
	if ( !cureligible || curhid )
		return;
	if ( x0 < curbx1 && x1 > curbx0 && y0 < curby1 && y1 > curby0 )
	{
		ioctl(hrfd, CIOMSEOFF, (char *)0);
		curhid = 1;
	}
}

/* Leave a drawing primitive: restore the cursor if we hid it, then drop the lock
 * if this primitive took it (`locked' is cl_pbegin's return). */
static
cl_pend(locked)
{
	if ( curhid )
	{
		ioctl(hrfd, CIOMSEON, (char *)0);
		curhid = 0;
	}
	if ( locked )
		hr_unlock(hr_lockw());
	else
		hr_setdraw(mywid, 0);	/* fast path: release the drain flag */
}

/* Blit one glyph of font `fslot' with cell top-left at framebuffer (gx,gy),
 * painting only the part inside the already-clipped rect (x0,y0)-(x1,y1).  One
 * bitblt shifts each glyph row; op L_NSRC paints black ink on a white cell
 * (the .hf fonts store ink=1), L_NAND paints the ink only and leaves the rest
 * of the cell alone (transparent -- what cl_ptextt uses to double-strike). */
static
clglyph1(fslot, gx, gy, c, x0, y0, x1, y1, op)
{
	HRFONT *f;
	BLTSTRUCT blt;
	BITMAP src;
	int gi;

	if ( x1 <= x0 || y1 <= y0 )
		return;
	f = hr_font(fslot);
	if ( c < f->first || c >= f->first + f->nch )
		return;
	gi = c - f->first;
	src.base = (int *)&f->bits[gi * f->cellh];
	src.width = 16;				/* one word per glyph row */
	src.rect.origin.x = 0;   src.rect.origin.y = 0;
	src.rect.corner.x = f->cellw;  src.rect.corner.y = f->cellh;
	blt.src = &src;
	blt.dst = &cldisp;
	blt.op = op;
	blt.pat = texture[0];
	blt.dr.origin.x = x0;  blt.dr.origin.y = y0;
	blt.dr.corner.x = x1;  blt.dr.corner.y = y1;
	blt.sp.x = x0 - gx;  blt.sp.y = y0 - gy;
	cl_hidecur(x0, y0, x1, y1);
	bitblt(&blt, 1, 0);
}

/* Draw string s at content cell (col,row) using font `fslot'; the cell advance
 * is cellw/cellh (== the caller's grid metrics), clipped to each visible rect. */
cl_text(fslot, col, row, s, cellw, cellh)
char *s;
{
	HRFONT *f;
	int px, py, fw, fh, c, i, cx0, cy0, cx1, cy1, slen, locked;

	f = hr_font(fslot);
	fw = f->cellw;  fh = f->cellh;
	for ( slen = 0; s[slen]; slen++ )	/* string span -> fast-path bbox */
		;
	/* lock only if not a fully-visible, cursor-clear repaint (see cl_pbegin) */
	locked = cl_pbegin(col * cellw, row * cellh,
			   (col + slen) * cellw, row * cellh + fh);
	if ( !S.mapped || cl_frozen() )	/* window unmapped / a server overlay is up */
	{
		cl_pend(locked);
		return;
	}
	px = S.ox + col * cellw;
	py = S.oy + row * cellh;
	for ( ; (c = *s & 0xff) != 0; s++, px += cellw )
	{
		if ( c < 0x20 || c > 0x7e )
			continue;
		for ( i = 0; i < S.nvis; i++ )
		{
			cx0 = px;       cy0 = py;
			cx1 = px + fw;  cy1 = py + fh;
			if ( cx0 < S.vis[i].x0 ) cx0 = S.vis[i].x0;
			if ( cy0 < S.vis[i].y0 ) cy0 = S.vis[i].y0;
			if ( cx1 > S.vis[i].x1 ) cx1 = S.vis[i].x1;
			if ( cy1 > S.vis[i].y1 ) cy1 = S.vis[i].y1;
			clglyph1(fslot, px, py, c, cx0, cy0, cx1, cy1, L_NSRC);
		}
	}
	cl_pend(locked);
}

/* The shared body of cl_ptext/cl_ptextt: string s with its cell top-left at
 * content PIXEL (cx,cy), each glyph blitted with logical op `op'. */
static
clptext1(fslot, cx, cy, s, op)
char *s;
{
	HRFONT *f;
	int px, py, fw, fh, c, i, cx0, cy0, cx1, cy1, slen, locked;

	f = hr_font(fslot);
	fw = f->cellw;  fh = f->cellh;
	for ( slen = 0; s[slen]; slen++ )
		;
	locked = cl_pbegin(cx, cy, cx + slen * fw, cy + fh);
	if ( !S.mapped || cl_frozen() )
	{
		cl_pend(locked);
		return;
	}
	px = S.ox + cx;
	py = S.oy + cy;
	for ( ; (c = *s & 0xff) != 0; s++, px += fw )
	{
		if ( c < 0x20 || c > 0x7e )
			continue;
		for ( i = 0; i < S.nvis; i++ )
		{
			cx0 = px;       cy0 = py;
			cx1 = px + fw;  cy1 = py + fh;
			if ( cx0 < S.vis[i].x0 ) cx0 = S.vis[i].x0;
			if ( cy0 < S.vis[i].y0 ) cy0 = S.vis[i].y0;
			if ( cx1 > S.vis[i].x1 ) cx1 = S.vis[i].x1;
			if ( cy1 > S.vis[i].y1 ) cy1 = S.vis[i].y1;
			clglyph1(fslot, px, py, c, cx0, cy0, cx1, cy1, op);
		}
	}
	cl_pend(locked);
}

/* Draw string s with its cell top-left at content PIXEL (cx,cy) -- the widget
 * variant of cl_text, which is cell-grid-locked (a button label sits at an
 * arbitrary y no grid passes through).  Advance is the font's own cellw. */
cl_ptext(fslot, cx, cy, s)
char *s;
{
	clptext1(fslot, cx, cy, s, L_NSRC);
}

/* Like cl_ptext but TRANSPARENT: only the ink is painted, the rest of each
 * cell is left alone.  What a caller lays over already-drawn text -- zman
 * double-strikes a run one pixel over for the lineprinter's own bold. */
cl_ptextt(fslot, cx, cy, s)
char *s;
{
	clptext1(fslot, cx, cy, s, L_NAND);
}

/* Fill framebuffer rect (already in fb coords) clipped to rect r with val
 * (1=white, 0=black), via a pattern bitblt (op L_TRUE/L_FALSE reads no source).
 * val 3 = 50% gray: L_TRUE through the HALF_TONE stipple, whose dest is
 * result & pattern, so one blit lays the checker.  The pattern is indexed by
 * DESTINATION x word / y line (bitblt BLT_pat_index), so the dither is anchored
 * to the screen: adjacent fills and partial repaints always mesh. */
static
clfill_fb(x0, y0, x1, y1, r, val)
HRRECT r;
{
	BLTSTRUCT blt;
	BITMAP src;

	if ( x0 < r.x0 ) x0 = r.x0;
	if ( y0 < r.y0 ) y0 = r.y0;
	if ( x1 > r.x1 ) x1 = r.x1;
	if ( y1 > r.y1 ) y1 = r.y1;
	if ( x1 <= x0 || y1 <= y0 )
		return;
	cl_hidecur(x0, y0, x1, y1);
	src.rect.origin.x = x0;  src.rect.origin.y = y0;
	src.rect.corner.x = x1;  src.rect.corner.y = y1;
	src.width = 16 * cl_words(x0, x1);
	src.base = cl_scraddr(x0, y0);
	blt.src = &src;
	blt.sp.x = x0;  blt.sp.y = y0;
	blt.dst = &cldisp;
	blt.dr = src.rect;
	blt.op = (val == 2) ? L_NDST : (val == 0 ? L_FALSE : L_TRUE);
	blt.pat = (val == 3) ? texture[4] : texture[0];	/* 4 = HALF_TONE */
	bitblt(&blt, 1, 0);
}

/* Fill a content-relative pixel rect with val (0=black, 1=white, 2=invert,
 * 3=50% gray), clipped to the visible regions. */
cl_fillrect(cx0, cy0, cx1, cy1, val)
{
	int i, locked;

	locked = cl_pbegin(cx0, cy0, cx1, cy1);
	if ( !S.mapped || cl_frozen() )
	{
		cl_pend(locked);
		return;
	}
	for ( i = 0; i < S.nvis; i++ )
		clfill_fb(S.ox + cx0, S.oy + cy0, S.ox + cx1, S.oy + cy1,
			  S.vis[i], val);
	cl_pend(locked);
}

/* Erase a block of content cells to white. */
cl_erase(col, row, ncol, nrow, cellw, cellh)
{
	cl_fillrect(col * cellw, row * cellh,
		    (col + ncol) * cellw, (row + nrow) * cellh, 1);
}

/* NB: there is deliberately no VRAM block-copy scroll here.  The terminal
 * scrolls by shifting its character grid and redrawing the changed cells from
 * the font (GUI.md sec 4) -- that stays correct across the 512-line SEG0/SEG1
 * split and always clips to the visible regions, which a block copy did not. */

/* Plot a content-relative pixel if it falls inside a visible region.
 * mode: 0 = black (clear bit), 1 = white (set bit), 2 = invert (XOR). */
cl_point(cx, cy, mode)
{
	int fx, fy, i, *p, m;

	if ( !S.mapped )
		return;
	fx = S.ox + cx;  fy = S.oy + cy;
	for ( i = 0; i < S.nvis; i++ )
		if ( fx >= S.vis[i].x0 && fx < S.vis[i].x1 &&
		     fy >= S.vis[i].y0 && fy < S.vis[i].y1 )
		{
			cl_hidecur(fx, fy, fx + 1, fy + 1);
			p = cl_scraddr(fx, fy);
			m = 0x8000 >> (fx & 15);
			if ( mode == 2 )      *p ^= m;	/* invert (XOR hands) */
			else if ( mode )      *p |= m;	/* white */
			else                  *p &= ~m;	/* black */
			return;
		}
}

/* Bresenham line in content coords (used for graphics clients like the clock;
 * low-rate, so per-pixel plotting is fine here -- unlike text).  mode as cl_point. */
cl_line(x0, y0, x1, y1, mode)
{
	int dx, dy, sx, sy, err, e2, locked, bx0, by0, bx1, by1;

	bx0 = x0 < x1 ? x0 : x1;   bx1 = (x0 > x1 ? x0 : x1) + 1;
	by0 = y0 < y1 ? y0 : y1;   by1 = (y0 > y1 ? y0 : y1) + 1;
	locked = cl_pbegin(bx0, by0, bx1, by1);	/* cl_point runs inside this */
	if ( cl_frozen() )		/* a server menu/overlay is up */
	{
		cl_pend(locked);
		return;
	}
	dx = x1 - x0;  if ( dx < 0 ) dx = -dx;
	dy = y1 - y0;  if ( dy < 0 ) dy = -dy;
	sx = x0 < x1 ? 1 : -1;
	sy = y0 < y1 ? 1 : -1;
	err = dx - dy;
	for (;;)
	{
		cl_point(x0, y0, mode);
		if ( x0 == x1 && y0 == y1 )
			break;
		e2 = err + err;
		if ( e2 > -dy ) { err -= dy;  x0 += sx; }
		if ( e2 <  dx ) { err += dx;  y0 += sy; }
	}
	cl_pend(locked);
}
