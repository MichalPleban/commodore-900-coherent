/*
 * zedit.c - a ZView plain-text editor client.
 *
 * A direct-render client (GUI.md Model A, like zterm): it keeps the document
 * as malloc'd lines, renders through a character-cell diff, and blits glyphs
 * straight to the framebuffer via clgfx.  The window is RESIZABLE
 * (HRF_STRETCH) and opens at the terminal's content size (80x25 cells of the
 * 8 px terminal font plus the scrollbar column), so an editor and a shell
 * tile identically.
 *
 * Layout, top to bottom:
 *   - ONE status line (terminal font): file name, a '*' when the buffer is
 *     modified, and the cursor's row,column -- with a 1 px rule under it;
 *   - below it the scrollable text area, with the common vertical scrollbar
 *     (clgfx/hrsbar.c) on the LEFT edge, exactly as in zterm: the bar is 16 px
 *     = one VRAM word, so the text grid keeps its byte alignment.
 *
 * Commands live in the window menu (wire.h HRM_*): New, Open, Save, Cut,
 * Copy, Paste, Help.  Open and Save put up a modal file-name dialog (hrdlg); Save
 * comes prefilled with the current name.  Cut/Copy write the mouse selection
 * to the CLIPBOARD store; Paste inserts the clipboard at the cursor.  The
 * select-drag also publishes the PRIMARY selection on release, and a
 * middle-click E_PASTE inserts PRIMARY at the click -- both system gestures
 * work here exactly as in a terminal.
 *
 * The hi-res keyboard map (zvpump.c) delivers ASCII only, so the key set is
 * the MicroEMACS one (Coherent's own editor) -- and zvpump maps the nav
 * keypad (arrows, Home, End, PgUp, PgDn, keypad Del) onto these same codes,
 * so the dedicated keys just work:
 *   ^B/^F left/right    ^P/^N up/down    ^A/^E line start/end
 *   ^Z/^V page up/down  ^D/DEL del char
 *   ^K kill to end of line into the KILL BUFFER (consecutive ^Ks append,
 *      so ^K^K takes text + newline -- the classic line-moving idiom)
 *   ^W kill the selection into the kill buffer    ^Y yank it back
 *   ^O open a line below the cursor   ^T transpose   ^L recentre + redraw
 *   ^S find next (a Find dialog asks for the pattern the first time)
 *   ^G abort (drop the selection)
 *   ^X^C quit -- the MicroEMACS exit chord; asks only when the buffer is
 *      modified (the window-menu Quit asks the server's generic question)
 *   ESC is Meta:  M-< / M-> buffer start/end   M-v page up
 *                 M-f / M-b word forward/back  M-s new search pattern
 * and the function keys (wire.h HRK_*):
 *   Help (F11) = this list as a dialog     F2 = save (dialog only if unnamed)
 *   F3 = Open   F4 = New   F5/F6/F7 = Cut/Copy/Paste   F8 = find next
 *   F10 = quit: zvpump delivers it AS the ^X^C chord, not as a code of its own
 *   Clear/Home = top of file   Stop/Continue = abort (drop the selection)
 * plus mouse: click places the cursor, drag selects.
 *
 * Tabs are stored literally and expanded to 8-column stops for display; the
 * cursor column shown in the status line is the DISPLAY column.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrsbar.h"
#include "hrdlg.h"

extern char	*malloc();

/* The window we ask for: the terminal's geometry (80x25 cells + the bar). */
#define	DEFCOLS	80
#define	DEFROWS	25

/* View grid ceilings: the biggest full-screen window at the 8x15 cell. */
#define	MAXROWS	52
#define	MAXCOLS	126

/* Document ceilings.  MAXLN pointers is the only per-slot cost (4 KB); each
 * line is malloc'd to its real length.  MAXLL is the stored-line cap -- a
 * longer input line is SPLIT on load (and the buffer marked modified, so a
 * save that would write the split back is not silent). */
#define	MAXLN	1024
#define	MAXLL	240
#define	TABW	8

#define	FNLEN	40		/* file-name buffer (path) */

HRAPP	me = { "Edit", "edit.icn", 0, 0, HRF_STRETCH | HRF_CONFIRM, 0, 0,
	       HRM_NEW | HRM_OPEN | HRM_SAVE | HRM_CUT | HRM_COPY | HRM_PASTE |
	       HRM_HELP };

int	mywid;
int	cellw, cellh;		/* cell metrics (terminal font)               */
int	xcols, xpix;		/* text-grid offset right of the scrollbar    */
int	ty0;			/* text area top: below status line + rule    */
int	contw, conth;		/* granted content size, px                   */
int	rows, cols;		/* visible text cells                         */

/* ---- the document ---- */
char	*ln[MAXLN];		/* malloc'd NUL-terminated lines              */
int	nln;			/* line count (always >= 1)                   */
int	dotl, dotc;		/* cursor: line index, BYTE offset in it      */
int	top, offx;		/* view: first line, first display column     */
char	fname[FNLEN];		/* current file name ("" = untitled)          */
int	modified;

/* ---- rendering (zterm's diff scheme) ---- */
char	disp[MAXROWS][MAXCOLS];	/* what is on screen; 0 = unknown/needs paint */
int	curon;			/* 1 = block cursor painted (inverted)        */
int	curc, curr;		/* cell where the cursor was painted          */
int	statdirty = 1;		/* 1 = repaint the status line wholesale      */

/* ---- selection: DOCUMENT positions, so it survives scrolling ---- */
int	selon;			/* 1 = a selection exists                     */
int	seldrag;		/* 1 = button down, extending it              */
int	sal, sac;		/* anchor line, byte offset                   */
int	sll, slc;		/* moving end (follows the drag / cursor)     */
int	selshown;		/* a highlight is painted on screen...        */
int	showa, showb;		/* ...over this VIEW-cell span (normalised)   */

HRSBAR	sbar;			/* the common scrollbar (hrsbar.h)            */
int	sbforce	= 1;		/* 1 = repaint the whole bar                  */

char	wk[MAXLL + 2];		/* line-edit / load work buffer               */
static char vbuf[MAXCOLS];	/* view-row expansion buffer                  */

/* ---- the kill buffer (MicroEMACS ^K / ^W / ^Y) ---- */
#define	MAXKILL	2048
char	kbuf[MAXKILL];		/* killed text, yanked back by ^Y             */
int	kn;			/* bytes in it                                */
int	lastkill;		/* last command was a kill: the next appends  */
int	metap;			/* 1 = ESC seen: next key is a Meta command   */
int	ctlxp;			/* 1 = ^X seen: next key completes the chord  */

char	srchbuf[32];		/* the search pattern (^S / M-s)              */

/* ------------------------------------------------------------------ */
/* line storage                                                       */
/* ------------------------------------------------------------------ */

static char *
lndup(s, n)
char *s;
{
	register char *p;
	register int i;

	if ( (p = malloc(n + 1)) == 0 )
		return 0;
	for ( i = 0; i < n; i++ )
		p[i] = s[i];
	p[n] = 0;
	return p;
}

/* Replace line i with a copy of s[0..n).  0 ok, -1 no memory (unchanged). */
static
setline(i, s, n)
char *s;
{
	char *p;

	if ( (p = lndup(s, n)) == 0 )
		return -1;
	free(ln[i]);
	ln[i] = p;
	return 0;
}

static
insline(i, s, n)
char *s;
{
	char *p;
	register int j;

	if ( nln >= MAXLN || (p = lndup(s, n)) == 0 )
		return -1;
	for ( j = nln; j > i; j-- )
		ln[j] = ln[j-1];
	ln[i] = p;
	nln++;
	return 0;
}

static
delline(i)
{
	register int j;

	free(ln[i]);
	for ( j = i; j < nln - 1; j++ )
		ln[j] = ln[j+1];
	nln--;
	return 0;
}

static
freebuf()
{
	register int i;

	for ( i = 0; i < nln; i++ )
		free(ln[i]);
	nln = 0;
	return 0;
}

static
newbuf()
{
	nln = 0;
	ln[0] = lndup("", 0);
	if ( ln[0] != 0 )
		nln = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* display-column mapping (tab expansion)                             */
/* ------------------------------------------------------------------ */

/* Display column of byte ci in line li. */
static
dispcol(li, ci)
{
	register char *p;
	register int dc, i;

	p = ln[li];
	dc = 0;
	for ( i = 0; i < ci && p[i]; i++ )
		dc = (p[i] == '\t') ? (dc / TABW + 1) * TABW : dc + 1;
	return dc;
}

/* Byte index of the character whose cell(s) cover display column dcw --
 * or the line length when dcw lies past the end. */
static
byteat(li, dcw)
{
	register char *p;
	register int dc, i, nd;

	p = ln[li];
	dc = 0;
	for ( i = 0; p[i]; i++ )
	{
		nd = (p[i] == '\t') ? (dc / TABW + 1) * TABW : dc + 1;
		if ( dcw < nd )
			return i;
		dc = nd;
	}
	return i;
}

/* ------------------------------------------------------------------ */
/* the view: document lines -> a character grid                       */
/* ------------------------------------------------------------------ */

/* Text of display row r (cols cells, blank-padded): line top+r expanded from
 * display column offx.  Static buffer, one row at a time -- exactly vrow()
 * in zterm.  A line past the end of the document is all blanks. */
static char *
vrow(r)
{
	register char *p;
	register int i, dc, c;
	int li;

	for ( i = 0; i < cols; i++ )
		vbuf[i] = ' ';
	li = top + r;
	if ( li < 0 || li >= nln )
		return vbuf;
	p = ln[li];
	dc = 0;
	for ( i = 0; p[i]; i++ )
	{
		c = p[i] & 0xff;
		if ( c == '\t' )
			dc = (dc / TABW + 1) * TABW;	/* blanks already there */
		else
		{
			if ( dc >= offx && dc - offx < cols )
				vbuf[dc - offx] =
				    (c < 0x20 || c > 0x7e) ? '?' : c;
			dc++;
		}
		if ( dc - offx >= cols )
			break;
	}
	return vbuf;
}

/* Mark the cells under the damaged CONTENT rect as needing repaint (make
 * disp[] impossible there), and flag the chrome the rect touches. */
static
invrect(x, y, w, h)
{
	register int r, c;
	int c0, c1, r0, r1;

	if ( w <= 0 || h <= 0 )
		return 0;
	if ( y < ty0 )
		statdirty = 1;
	if ( x < xpix )
		sbforce = 1;
	c0 = (x - xpix) / cellw;
	r0 = (y - ty0) / cellh;
	c1 = (x - xpix + w + cellw - 1) / cellw;
	r1 = (y - ty0 + h + cellh - 1) / cellh;
	if ( c0 < 0 ) c0 = 0;
	if ( r0 < 0 ) r0 = 0;
	if ( c1 > MAXCOLS ) c1 = MAXCOLS;
	if ( r1 > MAXROWS ) r1 = MAXROWS;
	for ( r = r0; r < r1; r++ )
		for ( c = c0; c < c1; c++ )
			disp[r][c] = 0;
	return 0;
}

/* Everything is stale: first draw, resize, restore, full expose. */
static
invalidate()
{
	register int r, c;

	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;
	curon = 0;
	selshown = 0;
	sbforce = 1;
	statdirty = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* cursor + selection overlays (zterm's XOR scheme)                   */
/* ------------------------------------------------------------------ */

static
curdraw()
{
	int r, dc;

	r = dotl - top;
	if ( r < 0 || r >= rows )
		return 0;
	dc = dispcol(dotl, dotc) - offx;
	if ( dc < 0 || dc >= cols )
		return 0;
	cl_fillrect(xpix + dc * cellw, ty0 + r * cellh,
		    xpix + (dc + 1) * cellw, ty0 + (r + 1) * cellh, 2);
	curon = 1;  curc = dc;  curr = r;
	return 0;
}

static
curerase()
{
	if ( curon )
	{
		cl_fillrect(xpix + curc * cellw, ty0 + curr * cellh,
			    xpix + (curc + 1) * cellw, ty0 + (curr + 1) * cellh, 2);
		curon = 0;
	}
	return 0;
}

/* Invert view cells [a..b] inclusive (cell = r*cols + c), one fill per row. */
static
selpaint(a, b)
{
	int lo, hi, rl, rh, r, c0, c1;

	lo = a < b ? a : b;
	hi = a < b ? b : a;
	rl = lo / cols;  rh = hi / cols;
	if ( rh >= rows ) rh = rows - 1;
	for ( r = rl; r <= rh; r++ )
	{
		c0 = (r == rl) ? lo % cols : 0;
		c1 = (r == hi / cols) ? hi % cols : cols - 1;
		if ( c1 >= cols ) c1 = cols - 1;
		if ( c0 > c1 ) continue;
		cl_fillrect(xpix + c0 * cellw, ty0 + r * cellh,
			    xpix + (c1 + 1) * cellw, ty0 + (r + 1) * cellh, 2);
	}
	return 0;
}

/* Selection endpoints in document order -> *pl0/*pc0 <= *pl1/*pc1, byte
 * offsets clamped to their lines. */
static
selnorm(pl0, pc0, pl1, pc1)
int *pl0, *pc0, *pl1, *pc1;
{
	int l0, c0, l1, c1, t;

	l0 = sal;  c0 = sac;  l1 = sll;  c1 = slc;
	if ( l1 < l0 || (l1 == l0 && c1 < c0) )
	{
		t = l0; l0 = l1; l1 = t;
		t = c0; c0 = c1; c1 = t;
	}
	if ( l0 < 0 ) { l0 = 0; c0 = 0; }
	if ( l1 > nln - 1 ) l1 = nln - 1;
	t = strlen(ln[l0]);
	if ( c0 > t ) c0 = t;
	t = strlen(ln[l1]);
	if ( c1 > t ) c1 = t;
	*pl0 = l0;  *pc0 = c0;  *pl1 = l1;  *pc1 = c1;
	return 0;
}

/* The selection's visible span as view cells [*plo..*phi], or 0 if none of it
 * is in view.  Endpoints are POSITIONS (between characters): the span covers
 * the cells from the start position through the one before the end position,
 * which in linear cell numbers is simply end-cell minus one -- that carries a
 * line-start end position back to the tail of the row above by itself. */
static
selspan(plo, phi)
int *plo, *phi;
{
	int l0, c0, l1, c1, dc, lo, hi;

	if ( !selon )
		return 0;
	selnorm(&l0, &c0, &l1, &c1);
	if ( l1 < top || l0 >= top + rows )
		return 0;
	if ( l0 < top )
		lo = 0;
	else
	{
		dc = dispcol(l0, c0) - offx;
		if ( dc < 0 ) dc = 0;
		if ( dc > cols - 1 ) dc = cols - 1;
		lo = (l0 - top) * cols + dc;
	}
	if ( l1 >= top + rows )
		hi = rows * cols - 1;
	else
	{
		dc = dispcol(l1, c1) - offx;
		if ( dc < 0 ) dc = 0;
		if ( dc > cols ) dc = cols;
		hi = (l1 - top) * cols + dc - 1;
	}
	if ( hi > rows * cols - 1 )
		hi = rows * cols - 1;
	if ( hi < lo )
		return 0;
	*plo = lo;  *phi = hi;
	return 1;
}

static
selclear()
{
	selon = 0;
	seldrag = 0;
	return 0;
}

/* Bring the painted highlight into agreement with the wanted one, inverting
 * only the cells whose state changes (zterm's seldelta; the wanted span is
 * re-derived from the document each flush, so it tracks scrolling). */
static
seldelta()
{
	int nlo, nhi, won, olo, ohi;

	nlo = nhi = 0;
	won = selspan(&nlo, &nhi);
	if ( !selshown )
	{
		if ( won )
		{
			selpaint(nlo, nhi);
			selshown = 1;  showa = nlo;  showb = nhi;
		}
		return 0;
	}
	olo = showa;  ohi = showb;
	if ( !won )
	{
		selpaint(olo, ohi);
		selshown = 0;
		return 0;
	}
	if ( olo == nlo && ohi == nhi )
		return 0;
	if ( ohi < nlo || nhi < olo )
	{
		selpaint(olo, ohi);
		selpaint(nlo, nhi);
	}
	else
	{
		if ( olo != nlo )
			selpaint(olo < nlo ? olo : nlo, (olo > nlo ? olo : nlo) - 1);
		if ( ohi != nhi )
			selpaint((ohi < nhi ? ohi : nhi) + 1, ohi > nhi ? ohi : nhi);
	}
	showa = nlo;  showb = nhi;
	return 0;
}

/* A redrawn run wiped the highlight pixels over it; put the painted part back. */
static
selpatch(r, c0, c1)
{
	int lo, hi;

	if ( !selshown )
		return 0;
	lo = r * cols + c0;
	hi = r * cols + c1 - 1;
	if ( lo < showa ) lo = showa;
	if ( hi > showb ) hi = showb;
	if ( lo <= hi )
		selpaint(lo, hi);
	return 0;
}

/* ------------------------------------------------------------------ */
/* painting                                                           */
/* ------------------------------------------------------------------ */

/* Repaint one run of changed cells [c0,c1) on view row r from vp: erase
 * white, then blit each maximal non-blank sub-span (cl_ptext, because the
 * text area starts at pixel ty0, off the cl_text row grid). */
static
drawrun(r, c0, c1, vp)
char *vp;
{
	char buf[MAXCOLS + 1];
	int s, e, i, n;

	cl_fillrect(xpix + c0 * cellw, ty0 + r * cellh,
		    xpix + c1 * cellw, ty0 + (r + 1) * cellh, 1);
	for ( s = c0; s < c1; s = e )
	{
		while ( s < c1 && vp[s] == ' ' )
			s++;
		if ( s >= c1 )
			break;
		for ( e = s; e < c1 && vp[e] != ' '; e++ )
			;
		n = 0;
		for ( i = s; i < e; i++ )
			buf[n++] = vp[i];
		buf[n] = 0;
		cl_ptext(SHM_FTERM, xpix + s * cellw, ty0 + r * cellh, buf);
	}
	return 0;
}

/* The status line: file name, modified star, cursor row,column; 1 px rule
 * under it.  Redrawn only when its text changes (or statdirty says the
 * pixels are gone). */
static char	oldst[64];

static
drawstat()
{
	char left[FNLEN + 4], right[16], now[64];

	sprintf(left, "%s%s", fname[0] ? fname : "(untitled)",
		modified ? " *" : "");
	sprintf(right, "%d,%d", dotl + 1, dispcol(dotl, dotc) + 1);
	sprintf(now, "%s|%s", left, right);
	if ( !statdirty && strcmp(now, oldst) == 0 )
		return 0;
	strcpy(oldst, now);
	statdirty = 0;
	cl_fillrect(0, 0, contw, cellh, 1);
	cl_ptext(SHM_FTERM, 4, 0, left);
	cl_ptext(SHM_FTERM, contw - 4 - strlen(right) * cellw, 0, right);
	cl_fillrect(0, cellh + 1, contw, cellh + 2, 0);
	return 0;
}

/* Keep the view top on a line that can legally head the window: at most
 * nln - rows (so a document shorter than the window never scrolls), at
 * least 0.  Deletions, resizes and page motion all funnel through here. */
static
clamptop()
{
	if ( top > nln - rows )
		top = nln - rows;
	if ( top < 0 )
		top = 0;
	return 0;
}

/* Repaint everything that changed: status, the view diff, the selection
 * delta, the cursor, the scrollbar -- zterm's flush() shape. */
static
flush()
{
	register int r, c;
	register char *vp;
	int c0;

	clamptop();		/* enforce the invariant whatever moved it */
	cl_begin();
	curerase();
	drawstat();
	for ( r = 0; r < rows; r++ )
	{
		vp = vrow(r);
		c = 0;
		while ( c < cols )
		{
			if ( vp[c] == disp[r][c] )
			{
				c++;
				continue;
			}
			c0 = c;
			while ( c < cols && vp[c] != disp[r][c] )
			{
				disp[r][c] = vp[c];
				c++;
			}
			drawrun(r, c0, c, vp);
			selpatch(r, c0, c);
		}
	}
	seldelta();
	curdraw();
	sbar.sb_x = 0;
	sbar.sb_y = ty0;
	sbar.sb_h = conth - ty0;
	sbar.sb_total = nln;
	sbar.sb_page = rows;
	sbar.sb_pos = top;
	hr_sbdraw(&sbar, sbforce);
	sbforce = 0;
	cl_end();
	return 0;
}

/* Derive the cell layout from the granted content size. */
static
layout()
{
	ty0 = cellh + 3;
	rows = (conth - ty0) / cellh;
	cols = (contw - xpix) / cellw;
	if ( rows > MAXROWS ) rows = MAXROWS;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( rows < 1 ) rows = 1;
	if ( cols < 1 ) cols = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* cursor motion + view                                               */
/* ------------------------------------------------------------------ */

/* Scroll the view the minimum needed to bring the cursor cell into it. */
static
fixview()
{
	int dc;

	if ( dotl < top )
		top = dotl;
	if ( dotl >= top + rows )
		top = dotl - rows + 1;
	if ( top < 0 )
		top = 0;
	dc = dispcol(dotl, dotc);
	if ( dc < offx )
		offx = dc;
	if ( dc >= offx + cols )
		offx = dc - cols + 1;
	if ( offx < 0 )
		offx = 0;
	return 0;
}

static
mvleft()
{
	if ( dotc > 0 )
		dotc--;
	else if ( dotl > 0 )
	{
		dotl--;
		dotc = strlen(ln[dotl]);
	}
	return 0;
}

static
mvright()
{
	if ( dotc < strlen(ln[dotl]) )
		dotc++;
	else if ( dotl < nln - 1 )
	{
		dotl++;
		dotc = 0;
	}
	return 0;
}

/* Up/down keep the DISPLAY column, so the cursor tracks straight through
 * lines of mixed tabs and text. */
static
mvup()
{
	int dc;

	if ( dotl == 0 )
	{
		dotc = 0;
		return 0;
	}
	dc = dispcol(dotl, dotc);
	dotl--;
	dotc = byteat(dotl, dc);
	return 0;
}

static
mvdown()
{
	int dc;

	if ( dotl >= nln - 1 )
	{
		dotc = strlen(ln[dotl]);
		return 0;
	}
	dc = dispcol(dotl, dotc);
	dotl++;
	dotc = byteat(dotl, dc);
	return 0;
}

/* Page by a screenful less one line of context; the view moves with the
 * cursor so a page-through reads continuously. */
static
pgmove(d)
{
	int dc, step;

	step = (rows > 1 ? rows - 1 : 1) * d;
	dc = dispcol(dotl, dotc);
	dotl += step;
	if ( dotl < 0 ) dotl = 0;
	if ( dotl > nln - 1 ) dotl = nln - 1;
	dotc = byteat(dotl, dc);
	top += step;
	clamptop();
	return 0;
}

/* Put the cursor at the cell under content pixel (px,py). */
static
setcurxy(px, py)
{
	int r, dcw;

	r = (py - ty0) / cellh;
	if ( r < 0 )
		r = 0;
	dotl = top + r;
	if ( dotl > nln - 1 ) dotl = nln - 1;
	if ( dotl < 0 ) dotl = 0;
	dcw = offx + (px - xpix) / cellw;
	if ( dcw < 0 )
		dcw = 0;
	dotc = byteat(dotl, dcw);
	return 0;
}

/* ------------------------------------------------------------------ */
/* editing primitives                                                 */
/* ------------------------------------------------------------------ */

static
inschar(c)
{
	register char *p;
	register int i;
	int len;

	p = ln[dotl];
	len = strlen(p);
	if ( len >= MAXLL )
		return 0;
	for ( i = 0; i < dotc; i++ )
		wk[i] = p[i];
	wk[dotc] = c;
	for ( i = dotc; i < len; i++ )
		wk[i+1] = p[i];
	if ( setline(dotl, wk, len + 1) < 0 )
		return 0;
	dotc++;
	modified = 1;
	return 0;
}

static
donl()
{
	register char *p;
	register int i;
	int len;

	p = ln[dotl];
	len = strlen(p);
	if ( insline(dotl + 1, p + dotc, len - dotc) < 0 )
		return 0;
	p = ln[dotl];			/* insline moved pointers, not bytes */
	for ( i = 0; i < dotc; i++ )
		wk[i] = p[i];
	setline(dotl, wk, dotc);
	dotl++;
	dotc = 0;
	modified = 1;
	return 0;
}

/* Remove bytes [i0,i1) from line l. */
static
delrange(l, i0, i1)
{
	register char *p;
	register int i;
	int len, n;

	if ( i1 <= i0 )
		return 0;
	p = ln[l];
	len = strlen(p);
	n = 0;
	for ( i = 0; i < i0; i++ )
		wk[n++] = p[i];
	for ( i = i1; i < len; i++ )
		wk[n++] = p[i];
	if ( setline(l, wk, n) == 0 )
		modified = 1;
	return 0;
}

/* Append line l+1 onto line l.  Refused (no-op) when the join would not fit
 * a stored line -- nothing is ever silently truncated. */
static
joinln(l)
{
	register char *a, *b;
	register int i;
	int la, lb, n;

	if ( l >= nln - 1 )
		return 0;
	a = ln[l];  b = ln[l+1];
	la = strlen(a);  lb = strlen(b);
	if ( la + lb > MAXLL )
		return 0;
	n = 0;
	for ( i = 0; i < la; i++ )
		wk[n++] = a[i];
	for ( i = 0; i < lb; i++ )
		wk[n++] = b[i];
	if ( setline(l, wk, n) < 0 )
		return 0;
	delline(l + 1);
	modified = 1;
	return 1;
}

static
backspc()
{
	int nc;

	if ( dotc > 0 )
	{
		delrange(dotl, dotc - 1, dotc);
		dotc--;
	}
	else if ( dotl > 0 )
	{
		nc = strlen(ln[dotl - 1]);
		if ( joinln(dotl - 1) )
		{
			dotl--;
			dotc = nc;
		}
	}
	return 0;
}

static
delfwd()
{
	if ( dotc < strlen(ln[dotl]) )
		delrange(dotl, dotc, dotc + 1);
	else
		joinln(dotl);
	return 0;
}

static
kappend(s, n)
char *s;
{
	register int i;

	for ( i = 0; i < n && kn < MAXKILL; i++ )
		kbuf[kn++] = s[i];
	return 0;
}

/* ^K: kill to end of line into the kill buffer; at the end of a line kill
 * the newline (join).  A consecutive ^K APPENDS, so ^K^K takes a whole line
 * text + newline -- kill lines, move, ^Y them back. */
static
dokill()
{
	int len;

	if ( !lastkill )
		kn = 0;
	len = strlen(ln[dotl]);
	if ( dotc < len )
	{
		kappend(ln[dotl] + dotc, len - dotc);
		delrange(dotl, dotc, len);
	}
	else if ( joinln(dotl) )
		kappend("\n", 1);
	return 0;
}

/* ^Y: insert the kill buffer at the cursor. */
static
yank()
{
	register int i, c;

	selclear();
	for ( i = 0; i < kn; i++ )
	{
		c = kbuf[i] & 0xff;
		if ( c == '\n' )
			donl();
		else
			inschar(c);
	}
	return 0;
}

/* ^O: open a line below the cursor without moving off it. */
static
openline()
{
	int l, c;

	l = dotl;  c = dotc;
	donl();
	dotl = l;  dotc = c;
	return 0;
}

/* ^T: transpose the characters around the cursor and advance. */
static
transpose()
{
	register char *p;
	register int i;
	int len, t;

	p = ln[dotl];
	len = strlen(p);
	i = dotc;
	if ( i >= len )
		i = len - 1;
	if ( i <= 0 )
		return 0;
	for ( t = 0; t < len; t++ )
		wk[t] = p[t];
	t = wk[i-1];  wk[i-1] = wk[i];  wk[i] = t;
	if ( setline(dotl, wk, len) == 0 )
		modified = 1;
	dotc = i + 1;
	if ( dotc > len )
		dotc = len;
	return 0;
}

/* ^L: put the cursor line in the middle of the window and repaint it all. */
static
recenter()
{
	top = dotl - rows / 2;
	clamptop();
	invalidate();
	return 0;
}

/* M-f / M-b: word motion (letters, digits, underscore). */
static
wordch(c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
	       (c >= '0' && c <= '9') || c == '_';
}

static
wordfwd()
{
	register char *p;

	for (;;)
	{
		p = ln[dotl];
		if ( dotc >= strlen(p) )
		{
			if ( dotl >= nln - 1 )
				return 0;
			dotl++;
			dotc = 0;
			continue;
		}
		if ( wordch(p[dotc] & 0xff) )
			break;
		dotc++;
	}
	p = ln[dotl];
	while ( dotc < strlen(p) && wordch(p[dotc] & 0xff) )
		dotc++;
	return 0;
}

static
wordback()
{
	register char *p;

	for (;;)
	{
		if ( dotc == 0 )
		{
			if ( dotl == 0 )
				return 0;
			dotl--;
			dotc = strlen(ln[dotl]);
			continue;
		}
		p = ln[dotl];
		if ( wordch(p[dotc-1] & 0xff) )
			break;
		dotc--;
	}
	p = ln[dotl];
	while ( dotc > 0 && wordch(p[dotc-1] & 0xff) )
		dotc--;
	return 0;
}

/* Delete the selected range; cursor lands at its start. */
static
delsel()
{
	int l0, c0, l1, c1;

	if ( !selon )
		return 0;
	selnorm(&l0, &c0, &l1, &c1);
	if ( l0 == l1 )
		delrange(l0, c0, c1);
	else
	{
		delrange(l0, c0, strlen(ln[l0]));
		delrange(l1, 0, c1);
		while ( l1 > l0 + 1 )
		{
			delline(l0 + 1);
			l1--;
		}
		joinln(l0);	/* may refuse when too long: two lines stay */
	}
	dotl = l0;
	dotc = c0;
	selclear();
	modified = 1;
	fixview();
	return 0;
}

/* ^W: kill the selection into the kill buffer (replacing it), then delete
 * it -- the keyboard twin of menu Cut, but through the kill buffer so a ^Y
 * puts it back, and a following ^K appends to it.  Returns 1 if it killed
 * anything (so a bare ^W does not arm the append). */
static
killregion()
{
	int l0, c0, l1, c1, l, i0, i1, len;
	register char *p;

	if ( !selon )
		return 0;
	kn = 0;
	selnorm(&l0, &c0, &l1, &c1);
	for ( l = l0; l <= l1; l++ )
	{
		p = ln[l];
		len = strlen(p);
		i0 = (l == l0) ? c0 : 0;
		i1 = (l == l1) ? c1 : len;
		if ( i1 > i0 )
			kappend(p + i0, i1 - i0);
		if ( l < l1 )
			kappend("\n", 1);
	}
	delsel();
	return 1;
}

/* ------------------------------------------------------------------ */
/* selection <-> the shared stores                                    */
/* ------------------------------------------------------------------ */

static long
selbytes()
{
	int l0, c0, l1, c1, l;
	long t;

	selnorm(&l0, &c0, &l1, &c1);
	if ( l0 == l1 )
		return (long)(c1 - c0);
	t = strlen(ln[l0]) - c0 + 1;
	for ( l = l0 + 1; l < l1; l++ )
		t += strlen(ln[l]) + 1;
	return t + c1;
}

/* Write the selection to a shared store: clip = 1 the CLIPBOARD (menu
 * Cut/Copy), clip = 0 the PRIMARY selection (published when a drag ends).
 * PRIMARY picks its store by size, as shmem.h prescribes: the tail when it
 * fits, else a file -- an editor cannot bound its content. */
static
putsel(clip)
{
	int l0, c0, l1, c1, l, i0, i1, len;
	register char *p;

	if ( !selon )
		return 0;
	if ( clip )
	{
		if ( hr_clipopen() < 0 )
			return 0;
	}
	else if ( hr_selopen(mywid,
		    selbytes() <= (long)HRSEL_INL ? HRSEL_MEM : HRSEL_FILE) < 0 )
		return 0;
	selnorm(&l0, &c0, &l1, &c1);
	for ( l = l0; l <= l1; l++ )
	{
		p = ln[l];
		len = strlen(p);
		i0 = (l == l0) ? c0 : 0;
		i1 = (l == l1) ? c1 : len;
		if ( i1 > i0 )
		{
			if ( clip )
				hr_clipwrite(p + i0, i1 - i0);
			else
				hr_selwrite(p + i0, i1 - i0);
		}
		if ( l < l1 )
		{
			if ( clip )
				hr_clipwrite("\n", 1);
			else
				hr_selwrite("\n", 1);
		}
	}
	if ( clip )
		hr_clipclose();
	else if ( hr_selclose() == 0 )
		hr_cmd(C_SELOWN);	/* previous owner drops its highlight */
	return 0;
}

/* Insert a shared store at the cursor: clip = 1 the clipboard (menu Paste),
 * clip = 0 PRIMARY (middle-click E_PASTE).  Streamed, chunk at a time, so a
 * file-backed selection larger than the tail pastes through the same loop. */
static
insstream(clip)
{
	char b[64];
	long off, len;
	int n, i, c;

	len = clip ? hr_cliplen() : hr_sellen();
	if ( len <= 0 )
		return 0;
	selclear();
	for ( off = 0; off < len; off += n )
	{
		n = clip ? hr_clipread(off, b, sizeof(b))
			 : hr_selread(off, b, sizeof(b));
		if ( n <= 0 )
			break;
		for ( i = 0; i < n; i++ )
		{
			c = b[i] & 0xff;
			if ( c == '\n' )
				donl();
			else if ( c == '\t' || (c >= ' ' && c < 0x7f) )
				inschar(c);
		}
	}
	fixview();
	return 0;
}

/* ------------------------------------------------------------------ */
/* file I/O                                                           */
/* ------------------------------------------------------------------ */

static
addline(s, n)
char *s;
{
	char *p;

	if ( nln >= MAXLN || (p = lndup(s, n)) == 0 )
		return -1;
	ln[nln++] = p;
	return 0;
}

/* Read a file into the buffer.  The old document is freed only once the file
 * is OPEN, so a failed open leaves the buffer untouched (and the dialog says
 * why).  An input line longer than MAXLL is split, and hitting MAXLN drops
 * the remainder; both leave the buffer marked modified -- the in-memory text
 * no longer matches the file, and a save must not pretend otherwise. */
static
loadfile(name)
char *name;
{
	char b[512];
	register int i, c;
	int fd, n, wl, dirty;

	if ( (fd = open(name, 0)) < 0 )
		return -1;
	freebuf();
	wl = 0;
	dirty = 0;
	while ( (n = read(fd, b, sizeof(b))) > 0 )
	{
		for ( i = 0; i < n; i++ )
		{
			c = b[i] & 0xff;
			if ( c == '\n' )
			{
				if ( addline(wk, wl) < 0 )
				{
					dirty = 1;
					goto full;
				}
				wl = 0;
			}
			else if ( wl < MAXLL )
				wk[wl++] = c;
			else		/* split an over-long line */
			{
				dirty = 1;
				if ( addline(wk, wl) < 0 )
					goto full;
				wl = 0;
				wk[wl++] = c;
			}
		}
	}
	if ( wl > 0 && addline(wk, wl) < 0 )
		dirty = 1;
full:
	close(fd);
	if ( nln == 0 )
		newbuf();
	dotl = dotc = top = offx = 0;
	selclear();
	modified = dirty;
	invalidate();
	return 0;
}

static
savefile(name)
char *name;
{
	int fd, i, len;

	if ( (fd = creat(name, 0644)) < 0 )
		return -1;
	for ( i = 0; i < nln; i++ )
	{
		len = strlen(ln[i]);
		if ( len > 0 && write(fd, ln[i], len) != len )
		{
			close(fd);
			return -1;
		}
		if ( write(fd, "\n", 1) != 1 )
		{
			close(fd);
			return -1;
		}
	}
	close(fd);
	sync();		/* flush the buffer cache: a save must survive the
			 * session ending, and single-user runs no update daemon */
	modified = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* dialogs (hrdlg widget kit)                                         */
/* ------------------------------------------------------------------ */

/* The file-name dialog, shared by Open and Save: a text field, a message
 * line that says why an OK was refused (the dialog stays up with the name
 * as typed, zclock's pattern), OK / Cancel. */
char	fnbuf[FNLEN];
char	dmsg[36];

HRWIDGET fwg[] = {
    { DW_LABEL,   12,  12,   0,  0, "File name:" },
    { DW_TEXT,    12,  32, 256, 22, (char *)0, 0, 0, fnbuf, sizeof(fnbuf) },
    { DW_LABEL,   12,  62,   0,  0, dmsg },
    { DW_BUTTON,  60,  88,  70, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  88,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NFWG	(sizeof(fwg) / sizeof(fwg[0]))
#define	FW_MSG	2
#define	FW_OK	3
#define	FD_W	280
#define	FD_H	132

/* Run it: save = 1 writes the buffer (name prefilled with the current file),
 * save = 0 reads a file in.  Returns 1 when the action was done. */
static
filedlg(save)
{
	int w, h, r, i;

	dmsg[0] = 0;
	if ( save )
	{
		for ( i = 0; (fnbuf[i] = fname[i]) != 0; i++ )
			;
	}
	else
		fnbuf[0] = 0;
	w = FD_W;
	h = FD_H;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	for (;;)
	{
		/* a label draws no background, so wipe the message line first */
		cl_fillrect(fwg[FW_MSG].dw_x, fwg[FW_MSG].dw_y, w,
			    fwg[FW_MSG].dw_y + hr_font(SHM_FUI)->cellh, 1);
		hr_dlgdraw(fwg, NFWG);
		r = hr_dlgrun(fwg, NFWG);
		if ( r == -1 )
		{
			hr_dlgclose();
			exit(0);
		}
		if ( r != FW_OK )
		{
			r = 0;
			break;
		}
		if ( fnbuf[0] == 0 )
		{
			strcpy(dmsg, "Enter a file name");
			continue;
		}
		if ( (save ? savefile(fnbuf) : loadfile(fnbuf)) < 0 )
		{
			strcpy(dmsg, save ? "Cannot write that file"
					  : "Cannot open that file");
			continue;
		}
		for ( i = 0; (fname[i] = fnbuf[i]) != 0; i++ )
			;
		r = 1;
		break;
	}
	hr_dlgclose();
	statdirty = 1;
	return r;
}

/* "Discard unsaved changes?" -- guards New and Open when modified. */
char	cfmsg[40];

HRWIDGET cwg[] = {
    { DW_LABEL,   12,  16,   0,  0, cfmsg },
    { DW_BUTTON,  40,  56,  90, DLG_BTNH, "Discard", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  56,  80, DLG_BTNH, "Cancel",  0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NCWG	(sizeof(cwg) / sizeof(cwg[0]))

static
confirm(msg)
char *msg;
{
	int w, h, r;

	strcpy(cfmsg, msg);
	w = 300;
	h = 100;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(cwg, NCWG);
	r = hr_dlgrun(cwg, NCWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return r == 1;		/* the Discard button */
}

/* ---- the Find dialog (^S / M-s) ------------------------------------ */

char	smsg[24];

HRWIDGET sfw[] = {
    { DW_LABEL,   12,  16,   0,  0, "Find:" },
    { DW_TEXT,    70,  12, 200, 22, (char *)0, 0, 0, srchbuf, sizeof(srchbuf) },
    { DW_LABEL,   12,  46,   0,  0, smsg },
    { DW_BUTTON,  60,  72,  70, DLG_BTNH, "Find",   0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  72,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NSFW	(sizeof(sfw) / sizeof(sfw[0]))
#define	SF_MSG	2
#define	SF_OK	3

/* Move the cursor to the next occurrence of srchbuf after it, scanning
 * forward and wrapping past the end (matches never span a newline).
 * Returns 1 and moves the cursor, or 0. */
static
findnext()
{
	register char *p;
	register int c;
	int i, l, c0, len, plen;

	plen = strlen(srchbuf);
	if ( plen == 0 )
		return 0;
	l = dotl;
	c0 = dotc + 1;
	for ( i = 0; i <= nln; i++ )
	{
		p = ln[l];
		len = strlen(p);
		for ( c = c0; c + plen <= len; c++ )
			if ( strncmp(p + c, srchbuf, plen) == 0 )
			{
				dotl = l;
				dotc = c;
				return 1;
			}
		l++;
		if ( l >= nln )
			l = 0;
		c0 = 0;
	}
	return 0;
}

/* ^S: find the next match; the first ^S (or M-s, any time) asks for the
 * pattern with a dialog, which stays up saying "Not found" rather than
 * silently doing nothing.  A repeat ^S that finds nothing just leaves the
 * cursor where it is. */
static
dosearch(newpat)
{
	int w, h, r;

	if ( newpat || srchbuf[0] == 0 )
	{
		smsg[0] = 0;
		w = 284;
		h = 116;
		r = hr_dlgopen(&w, &h);
		if ( r == -2 )
			exit(0);
		if ( r < 0 )
			return 0;
		for (;;)
		{
			cl_fillrect(sfw[SF_MSG].dw_x, sfw[SF_MSG].dw_y, w,
				    sfw[SF_MSG].dw_y + hr_font(SHM_FUI)->cellh, 1);
			hr_dlgdraw(sfw, NSFW);
			r = hr_dlgrun(sfw, NSFW);
			if ( r == -1 )
			{
				hr_dlgclose();
				exit(0);
			}
			if ( r != SF_OK )
				break;
			if ( srchbuf[0] == 0 )
			{
				strcpy(smsg, "Enter text to find");
				continue;
			}
			if ( findnext() )
				break;
			strcpy(smsg, "Not found");
		}
		hr_dlgclose();
	}
	else
		findnext();
	fixview();
	return 0;
}

/* ---- the Help dialog (the C900 Help key, F11) ----------------------- *
 * One card listing the whole key set -- the C900 keyboard has a Help key,
 * so it should do something helpful. */

HRWIDGET hwg[] = {
    { DW_LABEL, 12,  12, 0, 0, "Move: arrows / keypad, or ^B ^F ^P ^N" },
    { DW_LABEL, 12,  32, 0, 0, "^A ^E line ends     ^Z ^V page up/down" },
    { DW_LABEL, 12,  52, 0, 0, "^D DEL delete       ^K kill line (^K^K +NL)" },
    { DW_LABEL, 12,  72, 0, 0, "^W kill selection   ^Y yank it back" },
    { DW_LABEL, 12,  92, 0, 0, "^O open line  ^T transpose  ^L recentre" },
    { DW_LABEL, 12, 112, 0, 0, "^S find next  ^G abort selection" },
    { DW_LABEL, 12, 132, 0, 0, "ESC then:  < > top/end   v page up" },
    { DW_LABEL, 12, 152, 0, 0, "           f b word   s new search" },
    { DW_LABEL, 12, 172, 0, 0, "F2 Save    F3 Open     F4 New" },
    { DW_LABEL, 12, 192, 0, 0, "F5 Cut     F6 Copy     F7 Paste   F8 Find" },
    { DW_LABEL, 12, 212, 0, 0, "Clear/Home top of file   Stop/Cont abort" },
    { DW_LABEL, 12, 232, 0, 0, "F10 or ^X^C quit (asks if unsaved)" },
    { DW_BUTTON, 171, 264, 70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NHWG	(sizeof(hwg) / sizeof(hwg[0]))

static
dohelp()
{
	int w, h, r;

	w = 412;
	h = 308;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(hwg, NHWG);
	r = hr_dlgrun(hwg, NHWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return 0;
}

/* ------------------------------------------------------------------ */
/* menu commands                                                      */
/* ------------------------------------------------------------------ */

/* F2: save under the current name without a dialog; the dialog appears
 * only when there is no name yet, or the write fails (so it can say why). */
static
quicksave()
{
	if ( fname[0] && savefile(fname) == 0 )
		return 0;
	filedlg(1);
	return 0;
}

static
donew()
{
	if ( modified && !confirm("Discard unsaved changes?") )
		return 0;
	freebuf();
	newbuf();
	fname[0] = 0;
	modified = 0;
	dotl = dotc = top = offx = 0;
	selclear();
	invalidate();
	return 0;
}

static
doopen()
{
	if ( modified && !confirm("Discard unsaved changes?") )
		return 0;
	filedlg(0);
	return 0;
}

/* ^X^C (MicroEMACS exit; F10 arrives as this chord, zvpump.c): quit, asking
 * only when there are unsaved changes.  The window-menu Quit instead asks the
 * server's generic HRF_CONFIRM question and lands here as E_QUIT.  hr_bye()
 * tells the server to reap the window (zterm's self-quit idiom). */
static
doquit()
{
	if ( modified && !confirm("Discard unsaved changes?") )
		return 0;
	hr_bye();
	exit(0);
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static
dokey(c)
{
	int waskill;

	c &= 0xff;
	if ( metap )				/* the key after an ESC */
	{
		metap = 0;
		lastkill = 0;
		switch ( c )
		{
		case '<':	dotl = 0;  dotc = 0;			break;
		case '>':	dotl = nln - 1;
				dotc = strlen(ln[dotl]);		break;
		case 'v': case 'V':	pgmove(-1);			break;
		case 'f': case 'F':	wordfwd();			break;
		case 'b': case 'B':	wordback();			break;
		case 's': case 'S':	dosearch(1);			break;
		}				/* unknown Meta: ignored */
		fixview();
		return 0;
	}
	if ( ctlxp )				/* the key after a ^X */
	{
		ctlxp = 0;
		lastkill = 0;
		if ( c == 'C'-0x40 )		/* ^X^C: quit (MicroEMACS) */
			doquit();
		return 0;			/* unknown ^X chord: ignored */
	}
	waskill = 0;
	switch ( c )
	{
	case '\r':
	case '\n':	selclear();  donl();		break;
	case '\b':	selclear();  backspc();		break;
	case 0x7f:					/* DEL */
	case 'D'-0x40:	selclear();  delfwd();		break;
	case 'K'-0x40:	selclear();  dokill();  waskill = 1;	break;
	case 'W'-0x40:	waskill = killregion();		break;
	case 'Y'-0x40:	yank();				break;
	case 'O'-0x40:	selclear();  openline();	break;
	case 'T'-0x40:	selclear();  transpose();	break;
	case 'L'-0x40:	recenter();			break;
	case 'S'-0x40:	dosearch(0);			break;
	case 'A'-0x40:	dotc = 0;			break;
	case 'E'-0x40:	dotc = strlen(ln[dotl]);	break;
	case 'B'-0x40:	mvleft();			break;
	case 'F'-0x40:	mvright();			break;
	case 'P'-0x40:	mvup();				break;
	case 'N'-0x40:	mvdown();			break;
	case 'Z'-0x40:	pgmove(-1);			break;
	case 'V'-0x40:	pgmove(1);			break;
	case 'G'-0x40:	selclear();			break;	/* abort */
	case 'X'-0x40:	ctlxp = 1;			break;	/* ^X prefix */
	case 033:	metap = 1;  selclear();		break;
	case HRK_HELP:	dohelp();			break;
	case HRK_F2:	quicksave();			break;
	case HRK_F3:	doopen();			break;
	case HRK_F4:	donew();			break;
	case HRK_F5:	putsel(1);  delsel();		break;	/* Cut   */
	case HRK_F6:	putsel(1);			break;	/* Copy  */
	case HRK_F7:	insstream(1);			break;	/* Paste */
	case HRK_F8:	dosearch(0);			break;	/* Find  */
	case HRK_CLRHOME: dotl = 0;  dotc = 0;		break;
	case HRK_STOP:	selclear();			break;
	default:
		if ( c == '\t' || (c >= ' ' && c < 0x7f) )
		{
			selclear();
			inschar(c);
		}
	}
	lastkill = waskill;
	fixview();
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

main(argc, argv)
char **argv;
{
	WMSG e;
	int need, i;

	/* Cell metrics from the terminal font in the shared tail (readable
	 * before we have a window); ask for exactly zterm's content -- the
	 * 80x25 grid plus the scrollbar column -- so an editor and a shell
	 * open the same size.  HRF_STRETCH: the user resizes us from there. */
	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	xcols = (HRSB_W + cellw - 1) / cellw;
	xpix = xcols * cellw;
	me.ha_w = xpix + DEFCOLS * cellw;
	me.ha_h = DEFROWS * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	layout();

	newbuf();
	if ( nln == 0 )
	{
		hr_bye();
		exit(1);		/* no memory for even one line */
	}
	/* An optional file argument (options were consumed by hr_open): open
	 * it, or start an empty buffer under that name if it does not exist
	 * yet -- the way an editor is asked for a new file. */
	if ( argc > 1 && argv[1][0] )
	{
		for ( i = 0; argv[1][i] && i < FNLEN - 1; i++ )
			fname[i] = argv[1][i];
		fname[i] = 0;
		loadfile(fname);
	}

	invalidate();
	need = 1;			/* flushed below, or by the first loop
					 * pass if a server overlay is up now */
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		flush();
		need = 0;
	}
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
				if ( e.wm_arg[0] <= 0 && e.wm_arg[1] <= 0 &&
				     e.wm_arg[2] >= contw && e.wm_arg[3] >= conth )
					invalidate();
				else
					invrect(e.wm_arg[0], e.wm_arg[1],
						e.wm_arg[2], e.wm_arg[3]);
				need = 1;
				break;

			case E_RESIZE:
				contw = e.wm_arg[0];
				conth = e.wm_arg[1];
				layout();
				clamptop();
				invalidate();
				need = 1;
				break;

			case E_KEY:
				dokey(e.wm_arg[0]);
				need = 1;
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					if ( e.wm_arg[0] < xpix &&
					     e.wm_arg[1] >= ty0 )
					{
						if ( hr_sbpress(&sbar, e.wm_arg[1]) )
						{
							top = sbar.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[1] >= ty0 )
					{
						setcurxy(e.wm_arg[0], e.wm_arg[1]);
						sal = sll = dotl;
						sac = slc = dotc;
						seldrag = 1;
						selon = 0;
						lastkill = 0;	/* a click breaks a ^K run */
						need = 1;
					}
				}
				else				/* release */
				{
					if ( sbar.sb_drag )
						hr_sbrelease(&sbar);
					else if ( seldrag )
					{
						seldrag = 0;
						if ( selon )
							putsel(0);	/* publish PRIMARY */
					}
				}
				break;

			case E_MOTION:
				if ( sbar.sb_drag )
				{
					if ( hr_sbmotion(&sbar, e.wm_arg[1]) )
					{
						top = sbar.sb_pos;
						need = 1;
					}
				}
				else if ( seldrag )
				{
					setcurxy(e.wm_arg[0], e.wm_arg[1]);
					sll = dotl;
					slc = dotc;
					selon = (sal != sll || sac != slc);
					need = 1;
				}
				break;

			case E_PASTE:		/* middle-click: insert PRIMARY */
				setcurxy(e.wm_arg[0], e.wm_arg[1]);
				insstream(0);
				need = 1;
				break;

			case E_SELCLEAR:	/* another window took PRIMARY */
				selclear();
				need = 1;
				break;

			case E_MENU:
				switch ( e.wm_arg[0] )
				{
				case HRM_NEW:	donew();		break;
				case HRM_OPEN:	doopen();		break;
				case HRM_SAVE:	filedlg(1);		break;
				case HRM_CUT:	putsel(1);  delsel();	break;
				case HRM_COPY:	putsel(1);		break;
				case HRM_PASTE:	insstream(1);		break;
				case HRM_HELP:	dohelp();		break;
				}
				need = 1;
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )		/* fell behind: assume the worst */
		{
			invalidate();
			need = 1;
		}
		/* Draw or defer, zterm's discipline: never paint under a server
		 * overlay or while unmapped.  Resuming alone owes nothing (the
		 * save-under restored our pixels); only a draw DROPPED against a
		 * freeze does, and cl_dropped() reports exactly that. */
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )
			{
				invalidate();
				need = 1;
			}
			if ( need )
			{
				flush();
				need = 0;
			}
		}
	}
}
