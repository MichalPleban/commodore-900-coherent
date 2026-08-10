/*
 * zpuzzle.c - the 15 puzzle, as a ZView window.
 *
 * The classic sliding-tile puzzle the early X demos shipped as `puzzle':
 * a fixed 4x4 board of numbered tiles with one gap.  Clicking a tile in the
 * same ROW or COLUMN as the gap slides the whole run of tiles between it
 * and the gap one place (the X behaviour -- one click moves several tiles);
 * the vi keys do the same one tile at a time (h/j/k/l = the direction a
 * tile MOVES into the gap).  A move counter runs under the board, and
 * announces when the board is solved.
 *
 * "New" in the window menu (or the n key) deals a fresh board.  The deal is
 * a few hundred random legal moves backwards from the solved position, so
 * every deal is solvable -- half of all random tile arrangements are not,
 * which is the one piece of maths this program has to know.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"

extern long	time();

#define	N	4		/* the board is N x N                     */
#define	NCELL	(N * N)
#define	TS	48		/* tile cell, px                          */
#define	MARG	8		/* board inset from the window edge       */
#define	STATH	22		/* status line under the board            */

HRAPP	me = { "Puzzle", "puzzle.icn", 0, 0, 0, 0, 0, HRM_NEW };

int	mywid;
int	fcw, fch;		/* UI-font cell                           */
int	contw, conth;		/* granted content size, px               */

int	board[NCELL];		/* tile numbers, 0 = the gap              */
int	gr, gc;			/* where the gap is                       */
int	moves;
int	won;			/* 1 = solved: board frozen until New     */

/* ------------------------------------------------------------------ */
/* the board                                                          */
/* ------------------------------------------------------------------ */

static
issolved()
{
	register int i;

	for ( i = 0; i < NCELL - 1; i++ )
		if ( board[i] != i + 1 )
			return 0;
	return 1;
}

/* A fresh deal: walk the gap a few hundred random legal steps back from
 * the solved position, so the deal is always solvable. */
static
newgame()
{
	static int dr[] = { 0, 0, 1, -1 };
	static int dc[] = { 1, -1, 0, 0 };
	register int i, d;
	int n, tr, tc;

	do {
		for ( i = 0; i < NCELL - 1; i++ )
			board[i] = i + 1;
		board[NCELL - 1] = 0;
		gr = gc = N - 1;
		for ( n = 0; n < 400; n++ )
		{
			d = rand() & 3;
			tr = gr + dr[d];
			tc = gc + dc[d];
			if ( tr < 0 || tr >= N || tc < 0 || tc >= N )
				continue;
			board[gr * N + gc] = board[tr * N + tc];
			board[tr * N + tc] = 0;
			gr = tr;
			gc = tc;
		}
	} while ( issolved() );
	moves = 0;
	won = 0;
	return 0;
}

/* Slide toward the gap: cell (r,c) must share its row or column, and the
 * whole run of tiles between it and the gap moves one place.  Returns 1
 * when anything moved. */
static
moveto(r, c)
{
	register int d;

	if ( won || r < 0 || r >= N || c < 0 || c >= N )
		return 0;
	if ( r == gr && c == gc )
		return 0;
	if ( r == gr )
	{
		d = (c > gc) ? 1 : -1;
		while ( gc != c )
		{
			board[gr * N + gc] = board[gr * N + gc + d];
			board[gr * N + gc + d] = 0;
			gc += d;
			moves++;
		}
	}
	else if ( c == gc )
	{
		d = (r > gr) ? 1 : -1;
		while ( gr != r )
		{
			board[gr * N + gc] = board[(gr + d) * N + gc];
			board[(gr + d) * N + gc] = 0;
			gr += d;
			moves++;
		}
	}
	else
		return 0;
	if ( issolved() )
		won = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* drawing                                                            */
/* ------------------------------------------------------------------ */

/* One cell: a bordered tile with its number (the 9x16 UI glyphs sit 1px
 * high-left in the cell, so centre +1,+1), or plain white for the gap. */
static
drawcell(r, c)
{
	char t[4];
	register int v;
	int x, y, tx, ty;

	x = MARG + c * TS;
	y = MARG + r * TS;
	v = board[r * N + c];
	cl_fillrect(x, y, x + TS, y + TS, 1);
	if ( v == 0 )
		return 0;
	cl_fillrect(x + 1, y + 1, x + TS - 1, y + 2, 0);
	cl_fillrect(x + 1, y + TS - 2, x + TS - 1, y + TS - 1, 0);
	cl_fillrect(x + 1, y + 1, x + 2, y + TS - 1, 0);
	cl_fillrect(x + TS - 2, y + 1, x + TS - 1, y + TS - 1, 0);
	sprintf(t, "%d", v);
	tx = x + (TS - strlen(t) * fcw) / 2 + 1;
	ty = y + (TS - fch) / 2 + 1;
	cl_ptext(SHM_FUI, tx, ty, t);
	return 0;
}

static
drawstat()
{
	char t[40];
	int y;

	y = MARG + N * TS + 3;
	cl_fillrect(0, y, contw, conth, 1);
	if ( won )
		sprintf(t, "Solved in %d moves!", moves);
	else
		sprintf(t, "%d moves", moves);
	cl_ptext(SHM_FUI, MARG + 1, y + 2, t);
	return 0;
}

static
drawall()
{
	register int r, c;

	cl_fillrect(0, 0, contw, conth, 1);
	for ( r = 0; r < N; r++ )
		for ( c = 0; c < N; c++ )
			drawcell(r, c);
	drawstat();
	return 0;
}

/* ------------------------------------------------------------------ */
/* input                                                              */
/* ------------------------------------------------------------------ */

/* h/j/k/l name the direction a TILE moves, so the tile that moves is the
 * gap's neighbour on the opposite side. */
static
dokey(c)
{
	c &= 0xff;
	switch ( c )
	{
	case 'h':	return moveto(gr, gc + 1);
	case 'l':	return moveto(gr, gc - 1);
	case 'k':	return moveto(gr + 1, gc);
	case 'j':	return moveto(gr - 1, gc);
	case 'n':
	case 'N':
		newgame();
		return 1;
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
	int need, wasidle, r, c;

	fcw = hr_font(SHM_FUI)->cellw;
	fch = hr_font(SHM_FUI)->cellh;
	if ( fcw <= 0 ) fcw = 9;
	if ( fch <= 0 ) fch = 16;
	me.ha_w = 2 * MARG + N * TS;
	me.ha_h = MARG + N * TS + STATH;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;

	srand((int)time((long *)0) ^ getpid());
	newgame();

	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		cl_begin();
		drawall();
		cl_end();
	}

	need = 0;
	wasidle = 0;
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
			case E_RESIZE:
				need = 1;
				break;

			case E_KEY:
				if ( dokey(e.wm_arg[0]) )
					need = 1;
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					r = (e.wm_arg[1] - MARG) / TS;
					c = (e.wm_arg[0] - MARG) / TS;
					if ( e.wm_arg[0] >= MARG &&
					     e.wm_arg[1] >= MARG &&
					     moveto(r, c) )
						need = 1;
				}
				break;

			case E_MENU:
				if ( e.wm_arg[0] == HRM_NEW )
				{
					newgame();
					need = 1;
				}
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )
			need = 1;
		cl_refresh();
		if ( cl_frozen() || !cl_mapped() )
			wasidle = 1;
		else
		{
			if ( wasidle )
			{
				need = 1;
				wasidle = 0;
			}
			if ( need )
			{
				cl_begin();
				drawall();
				cl_end();
				need = 0;
			}
		}
	}
}
