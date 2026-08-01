/*
 * wterm.c - an hrgui terminal-emulator client (GUI.md Phase 2).
 *
 * A plain process (no jlib, no coroutines) that gives a window a real shell:
 *   - allocates a pseudo-terminal pair (/dev/ptyp<n> master + /dev/ttyp<n>
 *     slave, the new kernel driver), forks a shell on the slave so it gets a
 *     controlling terminal (job control, cooked mode, ^C -> SIGINT), and keeps
 *     the master;
 *   - keeps an in-memory character grid, runs a small VT/ANSI parser over the
 *     shell's output, and repaints changed lines by blitting glyphs DIRECTLY to
 *     the framebuffer via clgfx (GUI.md Model A direct-render): no pixels and no
 *     per-glyph traffic cross IPC, and a full-line scroll is one VRAM block copy;
 *   - repaints the whole grid on E_EXPOSE (redraw-on-expose), which is what
 *     keeps the terminal correct when uncovered after being partially covered;
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

/* Grid size ceiling.  The framebuffer is 1024x800 and the terminal cell is the
 * gallant 12x25 font, so a full-screen terminal is at most 85x32 cells; 88x34
 * covers that with margin.  (It was 132x60 -- ~16 KB of grid+disp per process,
 * and wterm forks two I/O pumps that each inherit a full copy, so the oversize
 * was ~20 KB of dead memory per terminal.  On the 914 KB machine that was enough
 * to make the SECOND terminal's pump fork fail -> a shell whose output is never
 * read, i.e. the "second terminal can't type / no prompt" bug.) */
#define	MAXROWS	34
#define	MAXCOLS	88

int	mywid;
int	cols, rows;		/* grid size in cells */
int	cellw, cellh;		/* pixel cell metrics (for resize conversion) */

char	grid[MAXROWS][MAXCOLS];	/* logical content              */
char	disp[MAXROWS][MAXCOLS];	/* what is currently on screen  */
int	cx, cy;			/* cursor cell */
int	curon;			/* 1 = block cursor currently painted (inverted) */
int	curc, curr;		/* cell where the cursor was painted             */

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
/* server command helpers                                             */
/* ------------------------------------------------------------------ */
static
cmd6(type, a0, a1, a2, a3, a4, a5)
{
	WMSG c;
	c.wm_type = type;
	c.wm_wid = mywid;
	c.wm_arg[0] = a0; c.wm_arg[1] = a1; c.wm_arg[2] = a2;
	c.wm_arg[3] = a3; c.wm_arg[4] = a4; c.wm_arg[5] = a5;
	write(HR_CMDFD, &c, sizeof(c));
}

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
}

static
clearall()
{
	register int r;
	for ( r = 0; r < MAXROWS; r++ )
		clearrow(r);
	cx = cy = 0;
}

/* Force a full repaint on the next flush (screen damaged, resized, or first
 * drawn): make disp[] impossible so every cell differs from grid[].  The old
 * cursor pixels go away with the full repaint, so forget it (do not XOR-erase
 * a stale position). */
static
invalidate()
{
	register int r, c;
	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;		/* 0 is never a stored glyph */
	curon = 0;
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

/* Scroll the grid up one row (content only; the pixels are redrawn by the diff
 * in flush(), never block-copied). */
static
scrollup()
{
	register int r, c;
	for ( r = 1; r < rows; r++ )
		for ( c = 0; c < cols; c++ )
			grid[r-1][c] = grid[r][c];
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
		}
	}
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
	if ( cx >= cols )		/* wrap */
	{
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
		case '\n':	newline();		break;
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

/* Open a free master; derive the matching slave name.  Returns 0 or -1. */
static
openpty()
{
	int u;
	char mname[16];

	for ( u = 0; u < 4; u++ )
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

/* Fork a shell whose stdin/stdout/stderr are the slave (its controlling tty). */
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
		for ( f = 3; f < 20; f++ )	/* drop master + server pipes */
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
	int mp[2], i, got, off, rlen, need, pending, fz, wasidle, paintgen;

	if ( argc < 6 )
		exit(1);
	/* Standardized argv (GUI.md): wid contentW contentH cellW cellH -- the grid
	 * is derived from the content pixel size, the same as on E_RESIZE. */
	mywid = atoi(argv[1]);
	cellw = atoi(argv[4]);
	cellh = atoi(argv[5]);
	cols  = cellw ? atoi(argv[2]) / cellw : atoi(argv[2]);
	rows  = cellh ? atoi(argv[3]) / cellh : atoi(argv[3]);
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( rows > MAXROWS ) rows = MAXROWS;

	cl_init(mywid);			/* direct-render: map VRAM + clip descriptor */

	if ( openpty() < 0 )
		exit(1);
	if ( spawnsh() < 0 )
		exit(1);

	clearall();
	flush();

	if ( pipe(mp) < 0 )
		exit(1);
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
		sprintf(ev, "%d", HR_EVFD);
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
	close(muxw);
	close(HR_EVFD);				/* main reads events via the mux */

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
	wasidle = 0;
	paintgen = -2;			/* force a full repaint on the first pass */
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
			}
			else if ( m->tag == MX_EOF )
			{
				if ( need ) flush();
				cmd6(C_BYE, 0, 0, 0, 0, 0, 0);
				exit(0);
			}
			else if ( m->tag == MX_EVT )
			{
				for ( i = 0; i < sizeof(e); i++ )
					((char *)&e)[i] = m->d[i];
				/* E_KEY never arrives here: evpump writes keystrokes straight
				 * to the master so a ^C is never starved behind shell output. */
				if ( e.wm_type == E_EXPOSE )
				{
					invalidate();		/* damaged -> full repaint */
					need = 1;
				}
				else if ( e.wm_type == E_RESIZE )
				{
					int oldrows, oldcols, r, c, shift;

					oldrows = rows;  oldcols = cols;
					cols = e.wm_arg[0] / (cellw ? cellw : 8);
					rows = e.wm_arg[1] / (cellh ? cellh : 12);
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
		 * can clip to; (b) the window is unmapped (minimised); or (c) our clip
		 * descriptor changed since our last full repaint -- the server just hid /
		 * showed / raised / resized us and an E_EXPOSE is on its way, so the whole
		 * screen must be repainted from grid[], NOT patched incrementally over a
		 * stale-or-blank surface (that is the "flood scribbles random characters
		 * onto a just-unhidden window before the repaint" bug: the diff would see
		 * disp[] still matching and touch only the few just-changed cells).  We
		 * keep ingesting into grid[] regardless, so nothing is lost; the moment we
		 * are drawable again we force ONE full repaint (invalidate) and resume. */
		cl_refresh();			/* re-read the clip (cheap when unchanged) */
		fz = cl_frozen();
		if ( fz || !cl_mapped() )
		{
			wasidle = 1;		/* deferring -> full repaint when we resume */
		}
		else
		{
			if ( wasidle || cl_gen() != paintgen )
			{
				invalidate();	/* damaged / first pass after a change */
				pending = 1;
			}
			if ( pending )
			{
				flush();	/* ONE repaint for the whole drained batch */
				pending = 0;
			}
			paintgen = cl_gen();	/* record the generation we painted against */
			wasidle = 0;
		}
	}
	exit(0);
}
