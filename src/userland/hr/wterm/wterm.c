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

#define	MAXROWS	60
#define	MAXCOLS	132

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

/* ------------------------------------------------------------------ */
/* mux pumps                                                          */
/* ------------------------------------------------------------------ */

/* Copy server events (HR_EVFD) into the mux, one WMSG per record. */
static
evpump()
{
	struct mux mx;
	WMSG e;
	int n, i;

	close(muxr);
	close(mfd);
	for (;;)
	{
		n = read(HR_EVFD, &e, sizeof(e));
		if ( n <= 0 )
			_exit(0);
		if ( n != sizeof(e) )
			continue;
		mx.tag = MX_EVT;
		mx.n = sizeof(e);
		for ( i = 0; i < sizeof(e); i++ )
			mx.d[i] = ((char *)&e)[i];
		write(muxw, &mx, sizeof(mx));
	}
}

/* Copy master output into the mux, up to 16 bytes per record. */
static
mpump()
{
	struct mux mx;
	int n;

	close(muxr);
	close(HR_EVFD);
	for (;;)
	{
		n = read(mfd, mx.d, sizeof(mx.d));
		if ( n <= 0 )
			break;
		mx.tag = MX_DATA;
		mx.n = n;
		write(muxw, &mx, sizeof(mx));
	}
	mx.tag = MX_EOF;
	mx.n = 0;
	write(muxw, &mx, sizeof(mx));
	_exit(0);
}

/* read exactly len bytes (mux records are written atomically) */
static
readn(fd, buf, len)
char *buf;
{
	int got, n;
	got = 0;
	while ( got < len )
	{
		n = read(fd, buf + got, len - got);
		if ( n <= 0 )
			return got ? got : n;
		got += n;
	}
	return got;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */
main(argc, argv)
char **argv;
{
	struct mux mx;
	WMSG e;
	int mp[2], i, n;
	char ch;

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

	if ( fork() == 0 ) { evpump(); _exit(0); }
	if ( fork() == 0 ) { mpump();  _exit(0); }
	close(muxw);
	close(HR_EVFD);				/* main reads events via the mux */

	for (;;)
	{
		n = readn(muxr, &mx, sizeof(mx));
		if ( n != sizeof(mx) )
			break;			/* pumps gone */
		if ( mx.tag == MX_DATA )
		{
			for ( i = 0; i < mx.n; i++ )
				vt(mx.d[i]);
			flush();
		}
		else if ( mx.tag == MX_EOF )
		{
			cmd6(C_BYE, 0, 0, 0, 0, 0, 0);
			break;
		}
		else if ( mx.tag == MX_EVT )
		{
			for ( i = 0; i < sizeof(e); i++ )
				((char *)&e)[i] = mx.d[i];
			if ( e.wm_type == E_KEY )
			{
				ch = e.wm_arg[0] & 0xff;
				write(mfd, &ch, 1);	/* keystroke to the shell */
			}
			else if ( e.wm_type == E_EXPOSE )
			{
				invalidate();		/* damaged -> full repaint */
				flush();
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
				/* Shrinking below the cursor row: scroll the kept content up so
				 * the cursor line stays on-screen -- drop the oldest top rows and
				 * keep the cursor row with the recent output above it, so the
				 * cursor never falls outside the new window. */
				if ( cy >= rows )
				{
					shift = cy - (rows - 1);
					for ( r = 0; r < rows; r++ )
						for ( c = 0; c < MAXCOLS; c++ )
							grid[r][c] = grid[r + shift][c];
					cy = rows - 1;
				}
				/* Blank cells newly revealed by a grow (whole new rows, and new
				 * columns of existing rows) so no stale content shows. */
				for ( r = 0; r < rows; r++ )
					for ( c = (r < oldrows) ? oldcols : 0; c < cols; c++ )
						grid[r][c] = ' ';
				if ( cx >= cols ) cx = cols - 1;
				invalidate();		/* full repaint at the new size */
				flush();
			}
			else if ( e.wm_type == E_QUIT )
			{
				close(mfd);		/* SIGHUP the shell */
				break;
			}
		}
	}
	exit(0);
}
