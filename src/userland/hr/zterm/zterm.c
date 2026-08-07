/*
 * zterm.c - a ZView terminal-emulator client (GUI.md Phase 2).
 *
 * A plain process (no jlib, no coroutines) that gives a window a real shell:
 *   - declares the window it wants (title, 80x25 cells' worth of pixels, icon)
 *     to the server with hr_open() and is told its window id and granted
 *     content size back -- nothing comes in on the command line;
 *   - allocates a pseudo-terminal pair (/dev/ptyp<n> master + /dev/ttyp<n>
 *     slave, the new kernel driver), forks a shell on the slave so it gets a
 *     controlling terminal (job control, cooked mode, ^C -> SIGINT), and keeps
 *     the master;
 *   - keeps an in-memory character grid, runs a small VT/ANSI parser over the
 *     shell's output, and repaints changed lines by blitting glyphs DIRECTLY to
 *     the framebuffer via clgfx (GUI.md Model A direct-render): no pixels and no
 *     per-glyph traffic cross IPC, and a full-line scroll is one VRAM block copy;
 *   - repaints on E_EXPOSE (redraw-on-expose), which is what keeps the terminal
 *     correct when uncovered after being partially covered -- only the cells
 *     the event's damage rect touches, so uncovering a strip costs the strip;
 *   - forwards E_KEY events from the server to the shell (master write).
 *
 * There is no select(2), so the two input sources - the master's output and the
 * server's event pipe - are multiplexed the V7 way (GUI.md sec 4.5): two dumb
 * pump children each blindly copy one source into a single "mux" pipe, and this
 * (single) main process blocks on one read() of the mux, owning all state.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"

/* The window we ask the server for.  The size is filled in at run time from the
 * terminal font in the shared VRAM tail -- 80x25 character cells, the classic
 * console -- so the geometry follows the font instead of being duplicated as a
 * pixel count in the launcher catalog.  No HRF_STRETCH: the grid is sized from
 * the content, and a half-cell window would just waste pixels. */
#define	DEFCOLS	80
#define	DEFROWS	25

/* Grid size ceiling.  The window is NOT resizable, so the grid never has to hold
 * more than the 80x25 we ask for: the ceiling IS the default (~4 KB of
 * grid+disp).  It used to be 128x52 -- the biggest full-screen window at the 8x15
 * cell, with a power-of-two row stride -- but that is ~13 KB per terminal for
 * cells no zterm can ever address.  The grant is still clamped to this on connect
 * and on E_RESIZE, so a server that hands out a bigger content rect only ever
 * loses the surplus cells. */
#define	MAXROWS	DEFROWS
#define	MAXCOLS	DEFCOLS
/* Copy and Paste in the window menu (hrapp.h ha_menu): the CLIPBOARD half of the
 * text plumbing, as against select-and-middle-click, which needs no menu and is
 * unaffected.  Both are answered by the event pump, not here -- see hrpump.c. */
HRAPP	me = { "Shell", "term.icn", 0, 0, 0, 0, 0, HRM_COPY | HRM_PASTE };

int	mywid;
int	cols, rows;		/* grid size in cells */
int	cellw, cellh;		/* pixel cell metrics (for resize conversion) */

char	grid[MAXROWS][MAXCOLS];	/* logical content              */
char	disp[MAXROWS][MAXCOLS];	/* what is currently on screen  */

/* wrapd[r] = 1 when row r ran off the right edge and CONTINUES into row r+1,
 * rather than having been ended by a newline.  The grid cannot tell those apart
 * after the fact -- both just leave text on two rows -- so it is recorded as it
 * happens, in putch().  Copying uses it to rebuild logical lines: a long command
 * that wrapped across three rows must come back as ONE line, or pasting it into
 * another shell would run it as three. */
char	wrapd[MAXROWS];
int	cx, cy;			/* cursor cell */
int	curon;			/* 1 = block cursor currently painted (inverted) */
int	curc, curr;		/* cell where the cursor was painted             */

/* Text selection (GUI.md clipboard phase 1).  Cells are numbered r*cols + c, so
 * a selection is just a pair of numbers and the span between them is in reading
 * order -- a linear selection like a terminal's, not a rectangular block.
 *
 * The highlight is drawn by INVERTING the selected cells, and is managed exactly
 * like the block cursor above: lifted at the top of flush() and repainted at the
 * bottom, so it never confuses the grid/disp diff and needs no attribute plane.
 * selshown/showa/showb remember what is actually on the screen, because sela and
 * selb move while the user drags. */
int	selon;			/* 1 = there is a selection to show/copy         */
int	seldrag;		/* 1 = button down, extending the selection      */
int	sela, selb;		/* anchor and current end, as cell numbers       */
int	selshown;		/* 1 = a highlight is currently painted          */
int	showa, showb;		/* the span that is painted                      */

int	mfd = -1;		/* pty master fd */
char	sname[16];		/* slave device path */

/* ---- mux record: tag + length + payload (one atomic pipe write) ---- */
#define	MX_DATA	0		/* d[0..n-1] = master output bytes */
#define	MX_EVT	1		/* d[0..11]  = a WMSG event        */
#define	MX_EOF	2		/* master hit EOF (shell exited)   */
struct mux {
	char		tag;
	unsigned char	n;
	char		d[16];
};
int	muxr, muxw;		/* mux pipe ends */

/* ------------------------------------------------------------------ */
/* grid + rendering                                                   */
/* ------------------------------------------------------------------ */
/* The terminal keeps a character grid and repaints by drawing glyphs from the
 * font (GUI.md sec 4): it NEVER block-copies VRAM.  flush() diffs the logical
 * grid[] against disp[] (what is currently on screen) and redraws only the cells
 * that changed -- so a scroll, or a screen that is mostly spaces (the common
 * shell case), costs only the few glyphs that actually differ.  Every draw goes
 * through clgfx, which clips to the window's visible regions, so a covered
 * terminal never paints over the window on top of it. */

static
clearrow(r)
{
	register int c;
	for ( c = 0; c < MAXCOLS; c++ )
		grid[r][c] = ' ';
	wrapd[r] = 0;			/* a blank row continues nothing */
}

static
clearall()
{
	register int r;
	for ( r = 0; r < MAXROWS; r++ )
		clearrow(r);
	cx = cy = 0;
}

/* Mark the cells covering the damaged CONTENT-pixel rect (x,y,w,h) as needing
 * repaint: make disp[] impossible there so those cells differ from grid[] and
 * the next flush() redraws exactly them.  Cell bounds are rounded OUTWARD, so a
 * rect that clips a glyph in half still repaints the whole glyph.
 *
 * curon/selshown are deliberately left alone.  Their pixels inside the rect are
 * gone, but every cell in the rect is about to be redrawn from the grid, and
 * flush() re-applies the highlight over each redrawn run (selpatch) and the
 * cursor at the end -- so the XOR overlays come back correctly without this
 * having to know whether they were hit. */
static
invrect(x, y, w, h)
{
	register int r, c;
	int c0, c1, r0, r1;

	if ( w <= 0 || h <= 0 || cellw <= 0 || cellh <= 0 )
		return 0;
	c0 = x / cellw;
	r0 = y / cellh;
	c1 = (x + w + cellw - 1) / cellw;
	r1 = (y + h + cellh - 1) / cellh;
	if ( c0 < 0 ) c0 = 0;
	if ( r0 < 0 ) r0 = 0;
	if ( c1 > MAXCOLS ) c1 = MAXCOLS;
	if ( r1 > MAXROWS ) r1 = MAXROWS;
	for ( r = r0; r < r1; r++ )
		for ( c = c0; c < c1; c++ )
			disp[r][c] = 0;		/* 0 is never a stored glyph */
	return 0;
}

/* The whole window is gone (first draw, resize, or an expose that covers it). */
static
invalidate()
{
	register int r, c;
	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;
	curon = 0;
	selshown = 0;			/* the highlight pixels go with the repaint */
}

/* Block text cursor at the current cell, painted by inverting the cell (draw
 * twice to erase).  Kept as an overlay outside the grid/disp diff: erased at the
 * start of each flush and repainted at the end, so it never confuses the diff
 * and never leaves a trail. */
static
curdraw()
{
	int c, r;

	c = cx;  r = cy;
	if ( c >= cols ) c = cols - 1;		/* keep it on-screen at wrap */
	if ( r >= rows ) r = rows - 1;
	cl_fillrect(c * cellw, r * cellh, (c + 1) * cellw, (r + 1) * cellh, 2);
	curon = 1;  curc = c;  curr = r;
}

static
curerase()
{
	if ( curon )
	{
		cl_fillrect(curc * cellw, curr * cellh,
			    (curc + 1) * cellw, (curr + 1) * cellh, 2);
		curon = 0;
	}
}

/* Invert the cells of the span [a..b] (cell numbers, inclusive), one fill per
 * row.  Self-inverse, so this both paints and lifts the highlight -- the same
 * trick as the block cursor, and the reason a selection needs no attribute
 * plane and no change to the grid/disp diff. */
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
		cl_fillrect(c0 * cellw, r * cellh,
			    (c1 + 1) * cellw, (r + 1) * cellh, 2);
	}
}

/* Drop the selection.  Only the HIGHLIGHT goes: whatever was copied is already
 * in the shared store and stays pastable.  selshown is left alone: the next
 * flush()'s seldelta() sees a painted span and no wanted one, and lifts exactly
 * the pixels that are on the screen. */
static
selclear()
{
	selon = 0;
	seldrag = 0;
	return 0;
}

/* Bring the painted highlight into agreement with the wanted one, inverting
 * ONLY the cells whose state actually changes.
 *
 * This used to lift the whole highlight at the top of flush() and repaint the
 * whole highlight at the bottom, like the one-cell block cursor above.  That is
 * fine for one cell and badly wrong for a selection: a drag delivers a motion
 * event per clock tick, so extending a selection by ONE cell re-inverted every
 * cell already in it, twice -- O(selected) work per tick, growing as the user
 * drags.  Two spans in reading order differ only at their two ends, so the
 * symmetric difference is at most two runs and one dragged cell costs one cell.
 *
 * Painted spans are stored normalised (showa <= showb) so this can compare them
 * without re-sorting. */
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
		return 0;			/* the usual case mid-drag: no move */
	if ( ohi < nlo || nhi < olo )		/* disjoint: no cell is in both */
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

/* A run of cells [c0,c1) on row r has just been redrawn from the grid, which
 * wiped the highlight pixels over it.  Put back the part of the painted span
 * that the run covers, so the highlight survives shell output underneath it
 * without anyone having to lift and repaint the whole thing. */
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

/* Hand the selected text to the shared store so any window can paste it.
 *
 * The screen is a grid of cells, but what the user means to copy is LINES, so
 * the two differences between them are undone here (this is what a modern
 * terminal does, and why a naive cell dump pastes badly):
 *
 *   - Trailing blanks are padding, not text -- a row is blank-filled to the
 *     right edge whether or not anything was written there -- so they are
 *     trimmed.  Otherwise every pasted line arrives with a tail of spaces.
 *   - A row that WRAPPED is not a line, it is the first half of one.  It gets
 *     no newline, and no trim either: its last column is real content that
 *     runs straight into the next row.  So a command too long for the window
 *     comes back as the single line it was typed as, instead of being broken
 *     into pieces that a shell would run separately.
 *
 * HRSEL_MEM unconditionally: the grid is at most 80x25, so even a whole-screen
 * selection plus one newline per row is 2025 bytes, inside HRSEL_INL -- a
 * terminal copy never touches a disk.  Streamed a row at a time, so the only
 * buffer is one line long (zterm's data segment is already tight; see hrpump). */
static
copysel()
{
	char row[MAXCOLS + 1];
	int lo, hi, rl, rh, r, c0, c1, n, i;

	if ( !selon )
		return;
	lo = sela < selb ? sela : selb;
	hi = sela < selb ? selb : sela;
	if ( hr_selopen(mywid, HRSEL_MEM) < 0 )
		return;				/* someone else is publishing; drop it */
	rl = lo / cols;  rh = hi / cols;
	if ( rh >= rows ) rh = rows - 1;
	for ( r = rl; r <= rh; r++ )
	{
		c0 = (r == rl) ? lo % cols : 0;
		c1 = (r == rh) ? hi % cols : cols - 1;
		if ( c1 >= cols ) c1 = cols - 1;
		n = 0;
		for ( i = c0; i <= c1; i++ )
			row[n++] = grid[r][i];
		/* Trim a row that really ends here.  A wrapped row runs into the next
		 * one, so its trailing blanks are content -- unless the selection stops
		 * on it, where the user plainly does not want the padding either. */
		if ( !wrapd[r] || r == rh )
			while ( n > 0 && row[n-1] == ' ' )
				n--;
		if ( r < rh && !wrapd[r] )
			row[n++] = '\n';	/* a wrapped row is mid-line: no break */
		if ( n )
			hr_selwrite(row, n);
	}
	if ( hr_selclose() == 0 )
		hr_cmd(C_SELOWN);	/* so the previous owner drops its highlight */
}

/* Pointer position (content pixels) -> a cell number, clamped to the grid. */
static
selcell(px, py)
{
	int c, r;

	c = px / cellw;
	r = py / cellh;
	if ( c < 0 ) c = 0;
	if ( r < 0 ) r = 0;
	if ( c >= cols ) c = cols - 1;
	if ( r >= rows ) r = rows - 1;
	return r * cols + c;
}

/* Scroll the grid up one row (content only; the pixels are redrawn by the diff
 * in flush(), never block-copied). */
static
scrollup()
{
	register int r, c;
	for ( r = 1; r < rows; r++ )
	{
		for ( c = 0; c < cols; c++ )
			grid[r-1][c] = grid[r][c];
		wrapd[r-1] = wrapd[r];		/* continuations move with the text */
	}
	clearrow(rows-1);
}

/* Repaint one run of changed cells [c0,c1) on row r: erase it white, then blit
 * each maximal non-blank sub-span (blanks are already white after the erase). */
static
drawrun(r, c0, c1)
{
	char buf[MAXCOLS+1];
	int s, e, i, n;

	cl_erase(c0, r, c1 - c0, 1, cellw, cellh);
	for ( s = c0; s < c1; s = e )
	{
		while ( s < c1 && grid[r][s] == ' ' )
			s++;
		if ( s >= c1 )
			break;
		for ( e = s; e < c1 && grid[r][e] != ' '; e++ )
			;
		n = 0;
		for ( i = s; i < e; i++ )
			buf[n++] = grid[r][i];
		buf[n] = 0;
		cl_text(SHM_FTERM, s, r, buf, cellw, cellh);
	}
}

/* Repaint everything that changed since the last flush (clgfx hides the cursor
 * and syncs the clip descriptor once for the whole batch). */
static
flush()
{
	register int r, c;
	int c0;

	cl_begin();
	curerase();			/* lift the cursor before repainting content */
	for ( r = 0; r < rows; r++ )
	{
		c = 0;
		while ( c < cols )
		{
			if ( grid[r][c] == disp[r][c] )
			{
				c++;
				continue;
			}
			c0 = c;
			while ( c < cols && grid[r][c] != disp[r][c] )
			{
				disp[r][c] = grid[r][c];
				c++;
			}
			drawrun(r, c0, c);
			selpatch(r, c0, c);	/* the run wiped the highlight over it */
		}
	}
	seldelta();			/* only the cells whose highlight changed */
	curdraw();			/* repaint the cursor on top */
	cl_end();
}

/* ------------------------------------------------------------------ */
/* VT / ANSI parser (subset: enough for sh, ls, simple full-screen)   */
/* ------------------------------------------------------------------ */
int	state;			/* 0 normal, 1 ESC, 2 ESC[ */
int	args[4], narg;

static
newline()
{
	cy++;
	if ( cy >= rows )
	{
		scrollup();
		cy = rows - 1;
	}
}

static
putch(c)
{
	if ( cx >= cols )		/* wrap: this row CONTINUES into the next */
	{
		wrapd[cy] = 1;
		cx = 0;
		newline();
	}
	grid[cy][cx] = c;
	cx++;
}

static
eraseline(r, fromcol)
{
	register int c;
	for ( c = fromcol; c < cols; c++ )
		grid[r][c] = ' ';
	wrapd[r] = 0;			/* erased to the edge: nothing continues */
}

static
erasescreen()
{
	register int r;
	for ( r = 0; r < rows; r++ )
		clearrow(r);
}

static
docsi(c)
{
	int n;

	n = args[0];
	switch ( c )
	{
	case 'H':				/* cursor position row;col */
	case 'f':
		cy = args[0] ? args[0] - 1 : 0;
		cx = args[1] ? args[1] - 1 : 0;
		break;
	case 'A':				/* up    */
		cy -= n ? n : 1;
		break;
	case 'B':				/* down  */
		cy += n ? n : 1;
		break;
	case 'C':				/* right */
		cx += n ? n : 1;
		break;
	case 'D':				/* left  */
		cx -= n ? n : 1;
		break;
	case 'J':				/* erase display */
		erasescreen();
		if ( n != 2 )			/* 0J/2J: home for us */
			cy = cx = 0;
		else
			cy = cx = 0;
		break;
	case 'K':				/* erase to end of line */
		eraseline(cy, cx);
		break;
	}
	if ( cy < 0 ) cy = 0;
	if ( cy >= rows ) cy = rows - 1;
	if ( cx < 0 ) cx = 0;
	if ( cx >= cols ) cx = cols - 1;
}

static
vt(c)
register int c;
{
	c &= 0xff;
	switch ( state )
	{
	case 0:
		switch ( c )
		{
		case '\n':	wrapd[cy] = 0;	/* ended, not wrapped */
				newline();		break;
		case '\r':	cx = 0;			break;
		case '\b':	if ( cx ) cx--;		break;
		case '\t':	cx = (cx | 7) + 1;
				if ( cx >= cols ) cx = cols - 1;
				break;
		case '\007':				break;	/* bell */
		case '\033':	state = 1;		break;
		default:
			if ( c >= ' ' && c < 0x7f )
				putch(c);
		}
		break;
	case 1:					/* ESC */
		if ( c == '[' )
		{
			narg = 0;
			args[0] = args[1] = args[2] = args[3] = 0;
			state = 2;
		}
		else
			state = 0;		/* ignore other ESC x */
		break;
	case 2:					/* ESC [ */
		if ( c >= '0' && c <= '9' )
		{
			if ( narg < 4 )
				args[narg] = args[narg] * 10 + (c - '0');
		}
		else if ( c == ';' )
		{
			if ( narg < 3 )
				narg++;
		}
		else
		{
			docsi(c);
			state = 0;
		}
		break;
	}
}

/* ------------------------------------------------------------------ */
/* pty + shell                                                        */
/* ------------------------------------------------------------------ */

/* Master/slave pairs the kernel pty driver provides (NPTY in drv/pty.c) -- keep
 * in sync, and with the /dev/{ptyp,ttyp}<n> nodes in src/image/hdd_devices.txt. */
#define NPTY	8

/* Open a free master; derive the matching slave name.  Returns 0 or -1. */
static
openpty()
{
	int u;
	char mname[16];

	for ( u = 0; u < NPTY; u++ )
	{
		sprintf(mname, "/dev/ptyp%d", u);
		if ( (mfd = open(mname, 2)) >= 0 )
		{
			sprintf(sname, "/dev/ttyp%d", u);
			return 0;
		}
	}
	return -1;
}

/* Fork a shell whose stdin/stdout/stderr are the slave (its controlling tty).
 *
 * The shell keeps ONE inherited fd: HR_CMDFD, the shared command pipe.  That is
 * what lets a GUI app be typed at this shell's prompt and get a window -- it
 * needs the command pipe to announce itself, and it makes its own event pipe
 * (wire.h), exactly like an app in the desktop start-up script.  Our own EVENT
 * pipe is private and must NOT go down: our events would then be read by
 * whatever the user runs. */
static
spawnsh()
{
	int pid, s, f;

	pid = fork();
	if ( pid < 0 )
		return -1;
	if ( pid == 0 )
	{
		s = open(sname, 2);		/* claims the pty as ctty */
		if ( s < 0 )
			_exit(1);
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		for ( f = 3; f < 20; f++ )	/* drop master + our event pipe */
			if ( f != HR_CMDFD )
				close(f);
		execl("/bin/sh", "sh", "-i", (char *)0);
		_exit(127);
	}
	return pid;
}

/* The two mux pumps run as a separate tiny program (hrpump.c) exec'd from main,
 * not as forked copies of this process -- see the comment at the fork/execl in
 * main() for why (memory).  The mux record format (struct mux, MX_*) is shared
 * with hrpump by duplication; keep the two in sync. */

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
main(argc, argv)
char **argv;
{
	struct mux rbuf[40];		/* batch buffer: drain many records per draw */
	char *rb;
	struct mux *m;
	WMSG e;
	int mp[2], i, got, off, rlen, need, pending, fz, wasidle;

	/* Cell metrics come from the terminal font in the shared VRAM tail, which
	 * is readable before we have a window; ask for a window big enough for the
	 * classic 80x25, then derive the grid from the content size we are GRANTED
	 * (which the server may have clamped), exactly as on E_RESIZE. */
	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	me.ha_w = DEFCOLS * cellw;
	me.ha_h = DEFROWS * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* no window server (hr_open does cl_init) */
	cols = me.ha_w / cellw;
	rows = me.ha_h / cellh;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( rows > MAXROWS ) rows = MAXROWS;
	if ( cols < 1 ) cols = 1;
	if ( rows < 1 ) rows = 1;

	/* From here on we HAVE a window: on any set-up failure say goodbye so
	 * the server reaps it at once, rather than leaving an empty window
	 * standing until the crash watchdog notices (all-or-nothing). */
	if ( openpty() < 0 )
	{
		hr_bye();
		exit(1);
	}
	if ( spawnsh() < 0 )
	{
		hr_bye();
		exit(1);
	}

	clearall();
	flush();

	if ( pipe(mp) < 0 )
	{
		hr_bye();
		exit(1);
	}
	muxr = mp[0];
	muxw = mp[1];

	/* Spawn the two I/O pumps as a TINY separate program (hrpump) rather than
	 * forking copies of ourselves: a fork clones this process's whole data/BSS
	 * (the grid + engine globals, ~12 KB), so three-processes-per-terminal
	 * exhausted memory after a few terminals (dead 3rd/4th terminal; menus whose
	 * save-buffer malloc then failed).  hrpump links libc only (~2 KB).  It takes
	 * the fd NUMBERS on its command line and closes everything else. */
	{
		char ms[8], mw[8], ev[8];
		sprintf(ms, "%d", mfd);
		sprintf(mw, "%d", muxw);
		sprintf(ev, "%d", mywid);	/* the pump listens on our RING now */
		if ( fork() == 0 )		/* event pump (keys straight to master) */
		{
			execl("/usr/hr/bin/hrpump", "hrpump", "e", ms, mw, ev, (char *)0);
			_exit(1);
		}
		if ( fork() == 0 )		/* master-output pump */
		{
			execl("/usr/hr/bin/hrpump", "hrpump", "m", ms, mw, (char *)0);
			_exit(1);
		}
	}
	close(muxw);				/* main reads events via the mux */

	/* Decouple ingestion from drawing.  One read() drains every mux record that
	 * is currently available; we parse them ALL into the grid (cheap, and NOT
	 * gated by the draw lock) and then flush() ONCE.  Under a flood this turns a
	 * screenful of output into a single repaint instead of one per 16-byte chunk,
	 * so drawing can never throttle the loop that drains the pty.  Records are
	 * written atomically (<= PIPE_BUF), so a read returns whole records; any
	 * split tail is carried to the next read. */
	rb = (char *)rbuf;
	rlen = 0;
	pending = 0;
	wasidle = 1;			/* force a full repaint on the first pass */
	for (;;)
	{
		got = read(muxr, rb + rlen, sizeof(rbuf) - rlen);
		if ( got <= 0 )
			break;			/* pumps gone */
		rlen += got;
		need = 0;
		for ( off = 0; rlen - off >= sizeof(struct mux); off += sizeof(struct mux) )
		{
			m = (struct mux *)(rb + off);
			if ( m->tag == MX_DATA )
			{
				for ( i = 0; i < m->n; i++ )
					vt(m->d[i]);
				need = 1;		/* redraw once, after this batch */
				/* The shell just wrote to the grid -- it may have scrolled,
				 * overwritten or erased the very cells that are highlighted.
				 * The highlight would then point at text that is no longer
				 * the text it was taken from, so drop it.  Nothing is lost:
				 * a completed selection is already in the shared store and
				 * stays pastable.  A drag in progress is abandoned too --
				 * its anchor cell means nothing once the screen has moved. */
				if ( selon || seldrag )
					selclear();
			}
			else if ( m->tag == MX_EOF )
			{
				if ( need ) flush();
				hr_bye();	/* the shell exited: reap the window */
				exit(0);
			}
			else if ( m->tag == MX_EVT )
			{
				for ( i = 0; i < sizeof(e); i++ )
					((char *)&e)[i] = m->d[i];
				/* E_KEY never arrives here: evpump writes keystrokes straight
				 * to the master so a ^C is never starved behind shell output.
				 * Nor does E_PASTE -- the pump inserts the selection into the
				 * master itself, for the same reason.  What DOES arrive is the
				 * selection gesture, which needs the grid and so belongs here. */
				if ( e.wm_type == E_EXPOSE )
				{
					/* The event carries the damaged CONTENT rect
					 * (x,y,w,h), so repaint that and not the whole
					 * grid: uncovering a strip of a terminal costs
					 * the strip.  A rect that spans the window is
					 * the full case and takes the cheaper wholesale
					 * path. */
					if ( e.wm_arg[0] <= 0 && e.wm_arg[1] <= 0 &&
					     e.wm_arg[2] >= cols * cellw &&
					     e.wm_arg[3] >= rows * cellh )
						invalidate();
					else
						invrect(e.wm_arg[0], e.wm_arg[1],
							e.wm_arg[2], e.wm_arg[3]);
					need = 1;
				}
				else if ( e.wm_type == E_BUTTON )
				{
					if ( e.wm_arg[2] & EB_LEFT )	/* press: anchor here */
					{
						sela = selb = selcell(e.wm_arg[0], e.wm_arg[1]);
						seldrag = 1;
						selon = 0;	/* a bare click drops the old one */
						need = 1;
					}
					else				/* release: publish it */
					{
						seldrag = 0;
						copysel();
					}
				}
				else if ( e.wm_type == E_SELCLEAR )
				{
					/* another window took the selection */
					if ( selon || seldrag )
					{
						selclear();
						need = 1;
					}
				}
				else if ( e.wm_type == E_MOTION )
				{
					if ( seldrag )
					{
						int b;

						b = selcell(e.wm_arg[0], e.wm_arg[1]);
						if ( b != selb )
						{
							selb = b;
							/* one cell is a click, not a drag */
							selon = (sela != selb);
							need = 1;
						}
					}
				}
				else if ( e.wm_type == E_RESIZE )
				{
					int oldrows, oldcols, r, c, shift;

					oldrows = rows;  oldcols = cols;
					cols = e.wm_arg[0] / (cellw ? cellw : 8);
					rows = e.wm_arg[1] / (cellh ? cellh : 15);
					if ( cols > MAXCOLS ) cols = MAXCOLS;
					if ( rows > MAXROWS ) rows = MAXROWS;
					if ( cols < 1 ) cols = 1;
					if ( rows < 1 ) rows = 1;
					/* Shrinking below the cursor row: scroll the kept content up
					 * so the cursor line stays on-screen. */
					if ( cy >= rows )
					{
						shift = cy - (rows - 1);
						for ( r = 0; r < rows; r++ )
							for ( c = 0; c < MAXCOLS; c++ )
								grid[r][c] = grid[r + shift][c];
						cy = rows - 1;
					}
					/* Blank cells newly revealed by a grow. */
					for ( r = 0; r < rows; r++ )
						for ( c = (r < oldrows) ? oldcols : 0; c < cols; c++ )
							grid[r][c] = ' ';
					/* Wrap points were recorded against the OLD column count, so
					 * they no longer mark where lines actually broke.  The
					 * original line structure is not recoverable from the grid,
					 * so forget it rather than copy text back with breaks in
					 * places that were never breaks. */
					for ( r = 0; r < MAXROWS; r++ )
						wrapd[r] = 0;
					if ( cx >= cols ) cx = cols - 1;
					invalidate();		/* full repaint at the new size */
					need = 1;
				}
				else if ( e.wm_type == E_QUIT )
				{
					if ( need ) flush();
					close(mfd);		/* SIGHUP the shell */
					exit(0);
				}
			}
		}
		/* carry any partial trailing record to the front of the buffer */
		rlen -= off;
		for ( i = 0; i < rlen; i++ )
			rb[i] = rb[off + i];
		if ( need )
			pending = 1;		/* content changed; needs a repaint */

		/* Decide draw vs. defer.  DO NOT draw while (a) a server overlay (pop-up
		 * menu / ghost drag) is up -- we would paint over it, it is not a layer we
		 * can clip to, or (b) the window is unmapped (minimised).  We keep
		 * ingesting into grid[] regardless, so nothing is lost; the moment we are
		 * drawable again we force ONE full repaint (invalidate) and resume -- our
		 * surface was blanked or is stale wholesale, and while unmapped we got no
		 * damage we could have trusted.
		 *
		 * A mere clip-descriptor CHANGE no longer forces that full repaint.  It
		 * used to (paintgen), because every restack sent a full-content E_EXPOSE
		 * anyway; now the server sends the damaged rect only, so a raise costs the
		 * strip that was covered instead of all 80x25 cells, and honouring the rect
		 * is the whole point.  Every path that really does invalidate everything --
		 * move, resize, restore from an icon -- still sends a full-content expose
		 * of its own, and the expose is emitted AFTER the new clip is published, so
		 * we cannot patch incrementally over a surface the server has not finished
		 * with (the "flood scribbles random characters onto a just-unhidden window"
		 * bug). */
		cl_refresh();			/* re-read the clip (cheap when unchanged) */
		fz = cl_frozen();
		if ( fz || !cl_mapped() )
		{
			wasidle = 1;		/* deferring -> full repaint when we resume */
		}
		else
		{
			if ( wasidle )
			{
				invalidate();	/* first pass after being undrawable */
				pending = 1;
			}
			if ( pending )
			{
				flush();	/* ONE repaint for the whole drained batch */
				pending = 0;
			}
			wasidle = 0;
		}
	}
	hr_bye();		/* pumps died: reap our window on the way out */
	exit(0);
}
