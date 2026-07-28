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
	cldisp.base = (int *)0x3a000000L;	/* SEG0, offset 0 */
	cldisp.rect.origin.x = 0;  cldisp.rect.origin.y = 0;
	cldisp.rect.corner.x = XMAXP;  cldisp.rect.corner.y = YMAXP;
	cldisp.width = XMAXP;
	hrfd = open("/dev/dmgr", 2);		/* for cursor on/off (any node) */
}

/* Refresh the cached clip descriptor with a seqlock read: retry while the
 * server is mid-write (seq odd) or seq changed under us. */
static
cl_sync()
{
	HRSURF *sp;
	int s1, s2, i;

	sp = hr_surf(mywid);
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
}

cl_mapped()	{ return S.mapped; }
cl_cw()		{ return S.cw; }
cl_ch()		{ return S.ch; }

cl_fullyvis()
{
	return S.mapped && S.nvis == 1 &&
	       S.vis[0].x0 == S.ox && S.vis[0].y0 == S.oy &&
	       S.vis[0].x1 == S.ox + S.cw && S.vis[0].y1 == S.oy + S.ch;
}

/* Start a repaint batch: refresh the descriptor, and if the driver's XOR cursor
 * is at/near our window, hide it for the whole batch (cooperative: the driver
 * now lets any client toggle it).  The margin (24px) generously covers the ~16px
 * sprite reaching just outside the content rect, so a blit never clips a shown
 * cursor and burns a stray arrow; a cursor far away is left alone (no flicker). */
cl_begin()
{
	HRGLOB *g;

	cl_sync();
	curhid = 0;
	if ( hrfd < 0 || !S.mapped )
		return;
	g = hr_glob();
	if ( S.ox - 24 < g->curx && g->curx < S.ox + S.cw + 24 &&
	     S.oy - 24 < g->cury && g->cury < S.oy + S.ch + 24 )
	{
		ioctl(hrfd, CIOMSEOFF, (char *)0);
		curhid = 1;
	}
}

cl_end()
{
	if ( curhid )
	{
		ioctl(hrfd, CIOMSEON, (char *)0);
		curhid = 0;
	}
}

/* Blit one glyph of font `fslot' with cell top-left at framebuffer (gx,gy),
 * painting only the part inside the already-clipped rect (x0,y0)-(x1,y1).  One
 * bitblt shifts each glyph row; L_NSRC paints black ink on a white cell (the
 * .hf fonts store ink=1). */
static
clglyph1(fslot, gx, gy, c, x0, y0, x1, y1)
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
	blt.op = L_NSRC;
	blt.pat = texture[0];
	blt.dr.origin.x = x0;  blt.dr.origin.y = y0;
	blt.dr.corner.x = x1;  blt.dr.corner.y = y1;
	blt.sp.x = x0 - gx;  blt.sp.y = y0 - gy;
	bitblt(&blt, 1, 0);
}

/* Draw string s at content cell (col,row) using font `fslot'; the cell advance
 * is cellw/cellh (== the caller's grid metrics), clipped to each visible rect. */
cl_text(fslot, col, row, s, cellw, cellh)
char *s;
{
	HRFONT *f;
	int px, py, fw, fh, c, i, cx0, cy0, cx1, cy1;

	if ( !S.mapped )
		return;
	f = hr_font(fslot);
	fw = f->cellw;  fh = f->cellh;
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
			clglyph1(fslot, px, py, c, cx0, cy0, cx1, cy1);
		}
	}
}

/* Fill framebuffer rect (already in fb coords) clipped to rect r with val
 * (1=white, 0=black), via a pattern bitblt (op L_TRUE/L_FALSE reads no source). */
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
	src.rect.origin.x = x0;  src.rect.origin.y = y0;
	src.rect.corner.x = x1;  src.rect.corner.y = y1;
	src.width = 16 * cl_words(x0, x1);
	src.base = cl_scraddr(x0, y0);
	blt.src = &src;
	blt.sp.x = x0;  blt.sp.y = y0;
	blt.dst = &cldisp;
	blt.dr = src.rect;
	blt.op = (val == 2) ? L_NDST : (val ? L_TRUE : L_FALSE);
	blt.pat = texture[0];
	bitblt(&blt, 1, 0);
}

/* Fill a content-relative pixel rect with val (0=black, 1=white, 2=invert),
 * clipped to the visible regions. */
cl_fillrect(cx0, cy0, cx1, cy1, val)
{
	int i;

	if ( !S.mapped )
		return;
	for ( i = 0; i < S.nvis; i++ )
		clfill_fb(S.ox + cx0, S.oy + cy0, S.ox + cx1, S.oy + cy1,
			  S.vis[i], val);
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
	int dx, dy, sx, sy, err, e2;

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
}
