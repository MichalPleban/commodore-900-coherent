/*
 * zman.c - a ZView manual-page browser.
 *
 * The pages it shows are the pre-formatted catman pages this system installs
 * under /usr/man: one directory per section, named <N>...man (1cmdman,
 * 8cmdman), one file per page, already formatted by nroff -man.  That is
 * exactly what /bin/man cats to a terminal; zman is the same idea with a
 * window on it:
 *   - a top row with a FIND FIELD: type a page name (it always has the
 *     focus -- printable keys go here), press Enter or the Find button,
 *     and the list jumps to that page; the page count sits on the right,
 *     and doubles as the "no such page" notice;
 *   - a scrollable LIST of every page by name, laid out in COLUMNS to use
 *     the width (reading order, like ls), the selected one shown inverted;
 *   - a scrollable CONTENT pane with the selected page, rendered the way
 *     nroff meant it: the classic backspace overstrikes are parsed out,
 *     c\bc becomes BOLD (the glyph double-struck one pixel over, which is
 *     exactly what the lineprinter did) and _\bc becomes UNDERLINED (a rule
 *     under the run).  Text in the content pane is SELECTABLE: drag to
 *     highlight, and the span is published to the PRIMARY selection on
 *     release (middle-click pastes it into any window), exactly as a
 *     terminal's select-to-copy works.
 * Both panes have the common vertical scrollbar (hrsbar) on the LEFT edge.
 * The layout, diff renderer and event loop are zprint's; the diff grid
 * carries a per-cell attribute beside the character so a page repaints
 * incrementally like everything else.
 *
 * Keys: printable characters edit the find field (BS rubs out, ^U clears,
 * Enter finds); ^P/^N select the previous/next page; ^Z/^V (and ^B/^F)
 * page the content.
 */
#include <stdio.h>
#include <types.h>
#include <stat.h>
#include <dir.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrsbar.h"
#include "hrdlg.h"	/* the DLG_* chrome metrics only: no dialogs here */

extern char	*malloc();

#define	MANDIR	"/usr/man"

/* View grid ceilings: the biggest full-screen window at the 8x15 cell. */
#define	MAXROWS	52
#define	MAXCOLS	126

/* The page catalog. */
#define	MAXSECT	8
#define	MAXPG	250
#define	NNAME	15		/* a page file name (DIRSIZ + NUL)        */

/* The content pane (the formatted page). */
#define	MAXCL	600
#define	MAXLL	200

/* Cell attribute bits (the aout/dispa planes). */
#define	A_BOLD	1
#define	A_UL	2

/* Button-bar metrics (UI font, dialog-button chrome). */
#define	BARH	32		/* bar height incl. its bottom rule       */
#define	BTNY	3		/* button top                             */
#define	BTNPAD	10		/* label side padding inside a button     */
#define	BTNGAP	14		/* between buttons                        */
#define	MAXBTN	2

/* The find field: box left of the Find button, always focused. */
#define	FLDX	6
#define	FLDW	(4 + (NNAME - 1) * 9 + 4)	/* 14 chars + inner pad */

/* The page list is laid out in COLUMNS to use the width: each entry gets a
 * fixed field of LCOLCW text cells (" name(x)" is at most 1+14+3 = 18, plus a
 * one-cell gap).  lcol columns fit across the pane, filled top-to-bottom
 * within a row before moving right (reading order, like ls). */
#define	LCOLCW	19

HRAPP	me = { "Manual", "man.icn", 0, 0, HRF_STRETCH, 0, 0, 0 };

int	mywid;
int	cellw, cellh;		/* terminal-font cell (the two panes)     */
int	fcw, fch;		/* UI-font cell (buttons, bar text)       */
int	xpix;			/* text-grid offset right of the bars     */
int	contw, conth;		/* granted content size, px               */
int	ly0, lrows;		/* list pane: top px, visible rows        */
int	cy0, crows;		/* content pane: top px, visible rows     */
int	cols;			/* text columns in both panes             */
int	lcol;			/* list columns (page entries across)     */

/* ---- the page catalog, built once by scanpages() ---- */
char	sects[MAXSECT][NNAME];	/* section directory names under /usr/man */
int	nsect;
struct page {
	char	nm[NNAME];	/* the page (file) name                   */
	char	sx;		/* index into sects[]                     */
};
struct page pages[MAXPG];
int	npg;
int	selpg = -1;		/* selected page, -1 = none               */
int	ltop;			/* first visible list ROW (of lcol pages) */

/* ---- the content pane: the selected page, text + attribute planes ---- */
char	*ln[MAXCL];		/* malloc'd NUL-terminated text lines     */
char	*la[MAXCL];		/* malloc'd attribute bytes, same length  */
int	nln;			/* line count (always >= 1)               */
int	ctop;			/* first visible content line             */

/* ---- rendering (zprint's diff scheme, plus the attribute plane) ---- */
char	dispc[MAXROWS][MAXCOLS];	/* character on screen; 0 = repaint */
char	dispa[MAXROWS][MAXCOLS];	/* attribute on screen              */
int	hlon;			/* 1 = list highlight painted             */
int	hlx0, hly0, hlx1, hly1;	/* the highlighted cell's rect, px        */
int	chromedirty = 1;	/* 1 = repaint bar + rules wholesale      */

/* ---- content-pane text selection (select-to-copy, zterm's scheme) ----
 * Cells are numbered vr*cols + c over the VISIBLE content rows (vr 0..crows-1,
 * so document line ctop+vr); a selection is a pair of cell numbers and the
 * span between them is in reading order.  The highlight is drawn by INVERTING
 * the cells, self-inverse like the list highlight.  Any content scroll or a
 * new page drops it (the cell numbers are viewport-relative). */
int	selon;			/* 1 = there is a selection to show/copy  */
int	seldrag;		/* 1 = button down, extending it          */
int	sela, selb;		/* anchor and current end, cell numbers   */
int	selshown;		/* 1 = a highlight is currently painted   */
int	showa, showb;		/* the painted span, normalised           */

HRSBAR	sbl;			/* list scrollbar                         */
HRSBAR	sbc;			/* content scrollbar                      */
int	sblforce = 1, sbcforce = 1;

/* the button bar as laid out by drawbar(), for hit-testing */
int	nbtn;
int	bx[MAXBTN], bw[MAXBTN];
char	*blab[MAXBTN];
int	barmed = -1;		/* armed (pressed) button, -1 = none      */
int	barin;			/* 1 = pointer currently inside it        */

static char vbuf[MAXCOLS];	/* view-row expansion buffers             */
static char vabuf[MAXCOLS];

/* ------------------------------------------------------------------ */
/* line storage (text + attributes, appending only)                   */
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

static
freebuf()
{
	register int i;

	for ( i = 0; i < nln; i++ )
	{
		free(ln[i]);
		free(la[i]);
	}
	nln = 0;
	return 0;
}

static
addline(s, a, n)
char *s, *a;
{
	char *p, *q;

	if ( nln >= MAXCL || (p = lndup(s, n)) == 0 )
		return -1;
	if ( (q = lndup(a, n)) == 0 )
	{
		free(p);
		return -1;
	}
	ln[nln] = p;
	la[nln] = q;
	nln++;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the overstrike parser                                              */
/* ------------------------------------------------------------------ */

/* One raw catman line -> text + attribute columns: \b backs up a column,
 * X over X is bold, _ over X (either order) is underline, tabs land on
 * 8-stops.  Returns the trimmed length. */
static
parseline(s, out, aout)
char *s, *out, *aout;
{
	register char *p;
	register int c, o;
	int mx;

	o = 0;
	mx = 0;
	for ( p = s; (c = *p & 0xff) != 0 && c != '\n'; p++ )
	{
		if ( c == '\b' )
		{
			if ( o > 0 )
				o--;
			continue;
		}
		if ( c == '\t' )
		{
			do {
				if ( o < MAXLL )
				{
					if ( o >= mx )
					{
						out[o] = ' ';
						aout[o] = 0;
						mx = o + 1;
					}
					o++;
				}
			} while ( o < MAXLL && (o & 7) != 0 );
			continue;
		}
		if ( c < 0x20 || c > 0x7e )
			c = '?';
		if ( o >= MAXLL )
			continue;
		if ( o < mx )			/* overstriking column o */
		{
			if ( out[o] == c )
				aout[o] |= A_BOLD;
			else if ( out[o] == '_' )
			{
				out[o] = c;
				aout[o] |= A_UL;
			}
			else if ( c == '_' )
				aout[o] |= A_UL;
			else
				out[o] = c;
		}
		else
		{
			out[o] = c;
			aout[o] = 0;
			mx = o + 1;
		}
		o++;
	}
	while ( mx > 0 && out[mx - 1] == ' ' && aout[mx - 1] == 0 )
		mx--;
	out[mx] = 0;
	aout[mx] = 0;
	return mx;
}

/* ------------------------------------------------------------------ */
/* the page catalog                                                   */
/* ------------------------------------------------------------------ */

static
pgcmp(a, b)
struct page *a, *b;
{
	register int r;

	if ( (r = strcmp(a->nm, b->nm)) != 0 )
		return r;
	return sects[a->sx][0] - sects[b->sx][0];
}

/* Every directory under /usr/man whose name ends in "man" is a section
 * (1cmdman, 8cmdman, ...); every file inside is a page. */
static
scanpages()
{
	char nm[NNAME], path[40];
	struct direct dir;
	register FILE *dirfile;
	register int i, n;
	int s;

	nsect = 0;
	npg = 0;
	if ( (dirfile = fopen(MANDIR, "r")) != 0 )
	{
		while ( nsect < MAXSECT &&
			fread((char *)&dir, sizeof(dir), 1, dirfile) == 1 )
		{
			if ( dir.d_ino == 0 || dir.d_name[0] == '.' )
				continue;
			for ( i = 0; i < DIRSIZ; i++ )
				nm[i] = dir.d_name[i];
			nm[DIRSIZ] = 0;
			n = strlen(nm);
			if ( n < 4 || strcmp(nm + n - 3, "man") != 0 )
				continue;
			strcpy(sects[nsect], nm);
			nsect++;
		}
		fclose(dirfile);
	}
	for ( s = 0; s < nsect; s++ )
	{
		sprintf(path, "%s/%s", MANDIR, sects[s]);
		if ( (dirfile = fopen(path, "r")) == 0 )
			continue;
		while ( npg < MAXPG &&
			fread((char *)&dir, sizeof(dir), 1, dirfile) == 1 )
		{
			if ( dir.d_ino == 0 || dir.d_name[0] == '.' )
				continue;
			for ( i = 0; i < DIRSIZ; i++ )
				pages[npg].nm[i] = dir.d_name[i];
			pages[npg].nm[DIRSIZ] = 0;
			pages[npg].sx = s;
			npg++;
		}
		fclose(dirfile);
	}
	qsort((char *)pages, npg, sizeof(struct page), pgcmp);
	return 0;
}

/* Load the selected page into the content pane. */
static
loadpage(i)
{
	char b[300], o[MAXLL + 1], a[MAXLL + 1];
	char path[60];
	register FILE *fp;
	int n;

	freebuf();
	ctop = 0;
	if ( i < 0 || i >= npg )
	{
		addline("(no page selected)", "", 18);
		return 0;
	}
	sprintf(path, "%s/%s/%s", MANDIR, sects[pages[i].sx], pages[i].nm);
	if ( (fp = fopen(path, "r")) == 0 )
	{
		addline("(cannot read that page)", "", 23);
		return 0;
	}
	while ( nln < MAXCL && fgets(b, sizeof(b), fp) != 0 )
	{
		n = parseline(b, o, a);
		addline(o, a, n);
	}
	fclose(fp);
	if ( nln == 0 )
		addline("(empty page)", "", 12);
	return 0;
}

static	clearsel();

static
select(i)
{
	int prow;

	if ( i < 0 || i >= npg )
		return 0;
	selpg = i;
	prow = selpg / lcol;			/* the list row it sits in */
	if ( prow < ltop )
		ltop = prow;
	if ( prow >= ltop + lrows )
		ltop = prow - lrows + 1;
	clearsel();				/* the old page's selection is gone */
	loadpage(selpg);
	return 0;
}

/* ------------------------------------------------------------------ */
/* the view: list rows + content rows -> one attributed grid          */
/* ------------------------------------------------------------------ */

/* Pixel y of view row r: rows 0..lrows-1 are the list, the rest content. */
static
rowy(r)
{
	if ( r < lrows )
		return ly0 + r * cellh;
	return cy0 + (r - lrows) * cellh;
}

/* Text and attributes of view row r (cols cells, blank-padded). */
static char *
vrow(r)
{
	register char *p;
	register int i;
	int li;
	char lbuf[MAXCOLS + 16];

	for ( i = 0; i < cols; i++ )
	{
		vbuf[i] = ' ';
		vabuf[i] = 0;
	}
	if ( r < lrows )			/* a list line: lcol pages across */
	{
		register int j, base;

		if ( npg == 0 )
		{
			if ( r == 0 )
			{
				p = "(no manual pages)";
				for ( i = 0; p[i] && i < cols; i++ )
					vbuf[i] = p[i];
			}
			return vbuf;
		}
		for ( j = 0; j < lcol; j++ )
		{
			li = (ltop + r) * lcol + j;
			if ( li < 0 || li >= npg )
				continue;
			sprintf(lbuf, " %s", pages[li].nm);
			base = j * LCOLCW;
			for ( i = 0; lbuf[i] && base + i < cols
				  && i < LCOLCW - 1; i++ )
				vbuf[base + i] = lbuf[i];
		}
		return vbuf;
	}
	li = ctop + (r - lrows);		/* a content line */
	if ( li < 0 || li >= nln )
		return vbuf;
	p = ln[li];
	for ( i = 0; p[i] && i < cols; i++ )
	{
		vbuf[i] = p[i];
		vabuf[i] = la[li][i];
	}
	return vbuf;
}

static
invalidate()
{
	register int r, c;

	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
		{
			dispc[r][c] = 0;
			dispa[r][c] = 0;
		}
	hlon = 0;
	selon = 0;
	seldrag = 0;
	selshown = 0;
	sblforce = 1;
	sbcforce = 1;
	chromedirty = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the list highlight (XOR, zprint's scheme) -- one column CELL now   */
/* ------------------------------------------------------------------ */

static
hldraw()
{
	int r, cx0;

	if ( selpg < 0 )
		return 0;
	r = selpg / lcol - ltop;
	if ( r < 0 || r >= lrows )
		return 0;
	cx0 = xpix + (selpg % lcol) * LCOLCW * cellw;
	hlx0 = cx0;
	hlx1 = cx0 + LCOLCW * cellw;
	if ( hlx1 > contw )
		hlx1 = contw;
	hly0 = ly0 + r * cellh;
	hly1 = hly0 + cellh;
	cl_fillrect(hlx0, hly0, hlx1, hly1, 2);
	hlon = 1;
	return 0;
}

static
hlerase()
{
	if ( hlon )
	{
		cl_fillrect(hlx0, hly0, hlx1, hly1, 2);
		hlon = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* content-pane selection (zterm's incremental invert)                */
/* ------------------------------------------------------------------ */

/* Content pixel (x,y) -> a content cell number, clamped to the pane. */
static
contentcell(x, y)
{
	int vr, c;

	vr = (y - cy0) / cellh;
	if ( vr < 0 ) vr = 0;
	if ( vr >= crows ) vr = crows - 1;
	c = (x - xpix) / cellw;
	if ( c < 0 ) c = 0;
	if ( c >= cols ) c = cols - 1;
	return vr * cols + c;
}

/* Invert the content cells of the span [a..b] (inclusive), one fill per row. */
static
selpaint(a, b)
{
	int lo, hi, rl, rh, r, c0, c1;

	lo = a < b ? a : b;
	hi = a < b ? b : a;
	rl = lo / cols;  rh = hi / cols;
	if ( rh >= crows ) rh = crows - 1;
	for ( r = rl; r <= rh; r++ )
	{
		c0 = (r == rl) ? lo % cols : 0;
		c1 = (r == hi / cols) ? hi % cols : cols - 1;
		if ( c1 >= cols ) c1 = cols - 1;
		if ( c0 > c1 ) continue;
		cl_fillrect(xpix + c0 * cellw, cy0 + r * cellh,
			    xpix + (c1 + 1) * cellw, cy0 + (r + 1) * cellh, 2);
	}
	return 0;
}

/* Drop the selection; the highlight is lifted by the next flush's seldelta. */
static
clearsel()
{
	selon = 0;
	seldrag = 0;
	return 0;
}

/* Bring the painted highlight into agreement with the wanted one, inverting
 * ONLY the cells whose state changes -- zterm's seldelta, unchanged. */
static
seldelta()
{
	int nlo, nhi, olo, ohi;

	nlo = nhi = 0;
	if ( selon )
	{
		nlo = sela < selb ? sela : selb;
		nhi = sela < selb ? selb : sela;
	}
	if ( !selshown )
	{
		if ( selon )
		{
			selpaint(nlo, nhi);
			selshown = 1;  showa = nlo;  showb = nhi;
		}
		return 0;
	}
	olo = showa;  ohi = showb;
	if ( !selon )				/* the selection went away */
	{
		selpaint(olo, ohi);
		selshown = 0;
		return 0;
	}
	if ( olo == nlo && ohi == nhi )
		return 0;			/* mid-drag, no move */
	if ( ohi < nlo || nhi < olo )		/* disjoint */
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

/* A run of content row `cr' cells [c0,c1) was just redrawn (wiping the
 * highlight over it); put back the part of the painted span it covers. */
static
selpatch(cr, c0, c1)
{
	int lo, hi;

	if ( !selshown )
		return 0;
	lo = cr * cols + c0;
	hi = cr * cols + c1 - 1;
	if ( lo < showa ) lo = showa;
	if ( hi > showb ) hi = showb;
	if ( lo <= hi )
		selpaint(lo, hi);
	return 0;
}

/* Hand the selected text to the shared store so any window can paste it.
 * Each man-page line is a whole line (no wrap), so rows are trimmed of
 * trailing blanks and joined with newlines.  The selection is bounded by the
 * visible pane (crows*cols < HRSEL_INL), so HRSEL_MEM never touches a disk. */
static
copysel()
{
	char row[MAXCOLS + 1];
	int lo, hi, rl, rh, cr, c0, c1, n, i, li, len;
	register char *lp;

	if ( !selon )
		return 0;
	lo = sela < selb ? sela : selb;
	hi = sela < selb ? selb : sela;
	if ( hr_selopen(mywid, HRSEL_MEM) < 0 )
		return 0;			/* someone else is publishing */
	rl = lo / cols;  rh = hi / cols;
	if ( rh >= crows ) rh = crows - 1;
	for ( cr = rl; cr <= rh; cr++ )
	{
		li = ctop + cr;
		lp = (li >= 0 && li < nln) ? ln[li] : "";
		len = strlen(lp);
		c0 = (cr == rl) ? lo % cols : 0;
		c1 = (cr == rh) ? hi % cols : cols - 1;
		if ( c1 >= cols ) c1 = cols - 1;
		n = 0;
		for ( i = c0; i <= c1; i++ )
			row[n++] = (i < len) ? lp[i] : ' ';
		while ( n > 0 && row[n-1] == ' ' )
			n--;
		if ( cr < rh )
			row[n++] = '\n';
		if ( n )
			hr_selwrite(row, n);
	}
	if ( hr_selclose() == 0 )
		hr_cmd(C_SELOWN);	/* the previous owner drops its highlight */
	return 0;
}

/* ------------------------------------------------------------------ */
/* the button bar                                                     */
/* ------------------------------------------------------------------ */

/* Draw one button: white face, 1px border, 2px drop shadow, centred label
 * (the 9x16 UI glyphs sit 1px high-left in their cell, so centre +1,+1). */
static
drawbtn(i)
{
	int x, w, tx, ty;

	x = bx[i];  w = bw[i];
	cl_fillrect(x, BTNY, x + w, BTNY + DLG_BTNH, 1);
	cl_fillrect(x, BTNY, x + w, BTNY + 1, 0);
	cl_fillrect(x, BTNY + DLG_BTNH - 1, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x, BTNY, x + 1, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + w - 1, BTNY, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + 2, BTNY + DLG_BTNH, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	cl_fillrect(x + w, BTNY + 2, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	tx = x + (w - strlen(blab[i]) * fcw) / 2 + 1;
	ty = BTNY + (DLG_BTNH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, tx, ty, blab[i]);
	if ( i == barmed && barin )
		cl_fillrect(x + 1, BTNY + 1, x + w - 1, BTNY + DLG_BTNH - 1, 2);
	return 0;
}

char	findbuf[NNAME];		/* the find field's text (always focused) */
char	statusmsg[40];		/* replaces the page count until a key    */

/* The find field: 1px box, the text, a caret after it. */
static
drawfld()
{
	int y0, y1, cx, ty;

	y0 = BTNY;
	y1 = BTNY + DLG_BTNH;
	cl_fillrect(FLDX, y0, FLDX + FLDW, y1, 1);
	cl_fillrect(FLDX, y0, FLDX + FLDW, y0 + 1, 0);
	cl_fillrect(FLDX, y1 - 1, FLDX + FLDW, y1, 0);
	cl_fillrect(FLDX, y0, FLDX + 1, y1, 0);
	cl_fillrect(FLDX + FLDW - 1, y0, FLDX + FLDW, y1, 0);
	ty = y0 + (DLG_BTNH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, FLDX + 4, ty, findbuf);
	cx = FLDX + 4 + strlen(findbuf) * fcw + 1;
	cl_fillrect(cx, y0 + 4, cx + 1, y1 - 4, 0);
	return 0;
}

static
drawbar()
{
	static char *btns[] = { "Find" };
	char sbuf[48];
	register int i;
	int x;

	cl_fillrect(0, 0, contw, BARH - 1, 1);
	cl_fillrect(0, BARH - 1, contw, BARH, 0);
	drawfld();
	nbtn = 1;
	x = FLDX + FLDW + BTNGAP;
	for ( i = 0; i < nbtn; i++ )
	{
		blab[i] = btns[i];
		bx[i] = x;
		bw[i] = strlen(btns[i]) * fcw + 2 * BTNPAD;
		drawbtn(i);
		x += bw[i] + BTNGAP;
	}
	if ( statusmsg[0] )
		strcpy(sbuf, statusmsg);
	else
		sprintf(sbuf, "%d pages", npg);
	i = contw - 8 - strlen(sbuf) * fcw;
	if ( i > x )
		cl_ptext(SHM_FUI, i, BTNY + (DLG_BTNH - fch) / 2 + 1, sbuf);
	cl_fillrect(0, cy0 - 2, contw, cy0 - 1, 0);
	return 0;
}

/* Which button is under content pixel (x,y)?  -1 = none. */
static
btnhit(x, y)
{
	register int i;

	if ( y < BTNY || y >= BTNY + DLG_BTNH )
		return -1;
	for ( i = 0; i < nbtn; i++ )
		if ( x >= bx[i] && x < bx[i] + bw[i] )
			return i;
	return -1;
}

/* ------------------------------------------------------------------ */
/* painting                                                           */
/* ------------------------------------------------------------------ */

/* Repaint cells c0..c1 of view row r from the text/attribute planes.
 * Runs are split where the attribute changes: a bold run is double-struck
 * one pixel over (the lineprinter's own bold), an underlined run gets a
 * rule along the cell bottom. */
static
drawrun(r, c0, c1, vp, ap)
char *vp, *ap;
{
	char buf[MAXCOLS + 1];
	int s, e, i, n, y, at;

	y = rowy(r);
	cl_fillrect(xpix + c0 * cellw, y,
		    xpix + c1 * cellw, y + cellh, 1);
	for ( s = c0; s < c1; s = e )
	{
		while ( s < c1 && vp[s] == ' ' && ap[s] == 0 )
			s++;
		if ( s >= c1 )
			break;
		at = ap[s];
		for ( e = s; e < c1 && ap[e] == at
			  && !(vp[e] == ' ' && at == 0); e++ )
			;
		n = 0;
		for ( i = s; i < e; i++ )
			buf[n++] = vp[i];
		buf[n] = 0;
		cl_ptext(SHM_FTERM, xpix + s * cellw, y, buf);
		if ( at & A_BOLD )
			cl_ptextt(SHM_FTERM, xpix + s * cellw + 1, y, buf);
		if ( at & A_UL )
			cl_fillrect(xpix + s * cellw, y + cellh - 1,
				    xpix + e * cellw, y + cellh, 0);
	}
	return 0;
}

/* Number of list ROWS: ceil(npg / lcol), at least 1. */
static
listrows()
{
	if ( npg <= 0 )
		return 1;
	return (npg + lcol - 1) / lcol;
}

static
clamptops()
{
	int lr;

	lr = listrows();
	if ( ltop > lr - lrows )
		ltop = lr - lrows;
	if ( ltop < 0 )
		ltop = 0;
	if ( ctop > nln - crows )
		ctop = nln - crows;
	if ( ctop < 0 )
		ctop = 0;
	return 0;
}

static
flush()
{
	register int r, c;
	register char *vp;
	int c0, nrows;

	clamptops();
	cl_begin();
	hlerase();
	if ( chromedirty )
	{
		drawbar();
		chromedirty = 0;
	}
	nrows = lrows + crows;
	for ( r = 0; r < nrows; r++ )
	{
		vp = vrow(r);
		c = 0;
		while ( c < cols )
		{
			if ( vp[c] == dispc[r][c] && vabuf[c] == dispa[r][c] )
			{
				c++;
				continue;
			}
			c0 = c;
			while ( c < cols && (vp[c] != dispc[r][c]
					  || vabuf[c] != dispa[r][c]) )
			{
				dispc[r][c] = vp[c];
				dispa[r][c] = vabuf[c];
				c++;
			}
			drawrun(r, c0, c, vp, vabuf);
			if ( r >= lrows )	/* the run wiped any highlight */
				selpatch(r - lrows, c0, c);
		}
	}
	hldraw();
	seldelta();
	sbl.sb_x = 0;
	sbl.sb_y = ly0;
	sbl.sb_h = lrows * cellh;
	sbl.sb_total = listrows();
	sbl.sb_page = lrows;
	sbl.sb_pos = ltop;
	hr_sbdraw(&sbl, sblforce);
	sblforce = 0;
	sbc.sb_x = 0;
	sbc.sb_y = cy0;
	sbc.sb_h = crows * cellh;
	sbc.sb_total = nln;
	sbc.sb_page = crows;
	sbc.sb_pos = ctop;
	hr_sbdraw(&sbc, sbcforce);
	sbcforce = 0;
	cl_end();
	return 0;
}

/* Derive the pane layout from the granted content size: the bar, then up to
 * 10 list rows (fewer in a short window, but at least 2), then the content
 * pane in what is left. */
static
layout()
{
	ly0 = BARH + 2;
	lrows = 10;
	while ( lrows > 2 && ly0 + lrows * cellh + 3 + 3 * cellh > conth )
		lrows--;
	cy0 = ly0 + lrows * cellh + 3;
	crows = (conth - cy0) / cellh;
	cols = (contw - xpix) / cellw;
	if ( crows < 1 ) crows = 1;
	if ( lrows + crows > MAXROWS ) crows = MAXROWS - lrows;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( cols < 1 ) cols = 1;
	lcol = cols / LCOLCW;
	if ( lcol < 1 ) lcol = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* Find                                                               */
/* ------------------------------------------------------------------ */

/* Go to the page the field names: exact match first, then the first
 * prefix match; not found says so where the page count usually sits. */
static
dofind()
{
	register int i, n;

	statusmsg[0] = 0;
	chromedirty = 1;
	n = strlen(findbuf);
	if ( n == 0 )
		return 0;
	for ( i = 0; i < npg; i++ )
		if ( strcmp(pages[i].nm, findbuf) == 0 )
		{
			select(i);
			return 0;
		}
	for ( i = 0; i < npg; i++ )
		if ( strncmp(pages[i].nm, findbuf, n) == 0 )
		{
			select(i);
			return 0;
		}
	sprintf(statusmsg, "no page \"%.14s\"", findbuf);
	return 0;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static
pgmove(d)
{
	ctop += (crows > 1 ? crows - 1 : 1) * d;
	clearsel();			/* cells are viewport-relative */
	clamptops();
	return 0;
}

static
dokey(c)
{
	register int n;

	c &= 0xff;
	switch ( c )
	{
	case 'P'-0x40:	select(selpg - 1);	return 0;
	case 'N'-0x40:	select(selpg + 1);	return 0;
	case 'Z'-0x40:
	case 'B'-0x40:	pgmove(-1);		return 0;
	case 'V'-0x40:
	case 'F'-0x40:	pgmove(1);		return 0;

	case '\r':
	case '\n':
		dofind();
		return 0;

	case 0x08: case 0x7f:			/* rub out */
		n = strlen(findbuf);
		if ( n > 0 )
			findbuf[n - 1] = 0;
		statusmsg[0] = 0;
		chromedirty = 1;
		return 0;

	case 'U'-0x40:				/* kill the line */
		findbuf[0] = 0;
		statusmsg[0] = 0;
		chromedirty = 1;
		return 0;
	}
	if ( c >= 0x20 && c <= 0x7e )		/* the field has the focus */
	{
		n = strlen(findbuf);
		if ( n < sizeof(findbuf) - 1 )
		{
			findbuf[n] = c;
			findbuf[n + 1] = 0;
			statusmsg[0] = 0;
			chromedirty = 1;
		}
	}
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

	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	fcw = hr_font(SHM_FUI)->cellw;
	fch = hr_font(SHM_FUI)->cellh;
	if ( fcw <= 0 ) fcw = 9;
	if ( fch <= 0 ) fch = 16;
	xpix = ((HRSB_W + cellw - 1) / cellw) * cellw;
	me.ha_w = xpix + 80 * cellw;	/* a full man line, ~4 list columns */
	me.ha_h = BARH + 2 + 10 * cellh + 3 + 22 * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	layout();

	scanpages();
	for ( i = 0; i < npg; i++ )	/* open on the desktop's own page */
		if ( strcmp(pages[i].nm, "zview") == 0 )
			break;
	if ( i < npg )
		select(i);
	else
		loadpage(selpg);	/* -1: no page shown until one is picked */

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
				invalidate();
				need = 1;
				break;

			case E_RESIZE:
				contw = e.wm_arg[0];
				conth = e.wm_arg[1];
				layout();
				clamptops();
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
					if ( (i = btnhit(e.wm_arg[0],
							 e.wm_arg[1])) >= 0 )
					{
						barmed = i;
						barin = 1;
						cl_begin();
						cl_fillrect(bx[i] + 1, BTNY + 1,
						    bx[i] + bw[i] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= ly0 &&
						  e.wm_arg[1] < ly0 + lrows * cellh )
					{
						if ( hr_sbpress(&sbl, e.wm_arg[1]) )
						{
							ltop = sbl.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= cy0 )
					{
						if ( hr_sbpress(&sbc, e.wm_arg[1]) )
						{
							ctop = sbc.sb_pos;
							clearsel();
							need = 1;
						}
					}
					else if ( e.wm_arg[1] >= ly0 &&
						  e.wm_arg[1] < ly0 + lrows * cellh )
					{
						int gr, gc;

						gr = (e.wm_arg[1] - ly0) / cellh;
						gc = (e.wm_arg[0] - xpix)
						     / (LCOLCW * cellw);
						if ( gc < 0 ) gc = 0;
						if ( gc >= lcol ) gc = lcol - 1;
						i = (ltop + gr) * lcol + gc;
						if ( i >= 0 && i < npg )
						{
							select(i);
							need = 1;
						}
					}
					else if ( e.wm_arg[0] >= xpix &&
						  e.wm_arg[1] >= cy0 )
					{
						/* start a content selection */
						sela = selb =
						  contentcell(e.wm_arg[0],
							      e.wm_arg[1]);
						selon = 1;
						seldrag = 1;
						need = 1;
					}
				}
				else				/* release */
				{
					if ( barmed >= 0 )
					{
						i = barmed;
						barmed = -1;
						if ( barin )
						{
							cl_begin();
							cl_fillrect(bx[i] + 1,
							    BTNY + 1,
							    bx[i] + bw[i] - 1,
							    BTNY + DLG_BTNH - 1, 2);
							cl_end();
							if ( i == 0 )
								dofind();
							need = 1;
						}
					}
					else if ( sbl.sb_drag )
						hr_sbrelease(&sbl);
					else if ( sbc.sb_drag )
						hr_sbrelease(&sbc);
					else if ( seldrag )
					{
						seldrag = 0;
						copysel();	/* publish it */
					}
				}
				break;

			case E_MOTION:
				if ( barmed >= 0 )
				{
					i = (btnhit(e.wm_arg[0], e.wm_arg[1])
					     == barmed);
					if ( i != barin )
					{
						barin = i;
						cl_begin();
						cl_fillrect(bx[barmed] + 1,
						    BTNY + 1,
						    bx[barmed] + bw[barmed] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
				}
				else if ( sbl.sb_drag )
				{
					if ( hr_sbmotion(&sbl, e.wm_arg[1]) )
					{
						ltop = sbl.sb_pos;
						need = 1;
					}
				}
				else if ( sbc.sb_drag )
				{
					if ( hr_sbmotion(&sbc, e.wm_arg[1]) )
					{
						ctop = sbc.sb_pos;
						clearsel();
						need = 1;
					}
				}
				else if ( seldrag )
				{
					int nc;

					nc = contentcell(e.wm_arg[0],
							 e.wm_arg[1]);
					if ( nc != selb )
					{
						selb = nc;
						need = 1;
					}
				}
				break;

			case E_SELCLEAR:		/* another window took it */
				if ( selon )
				{
					clearsel();
					need = 1;
				}
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
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )	/* a draw was lost against a freeze */
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
