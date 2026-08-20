/*
 * zwwin.c - dock widget: how many programs are on the desktop.
 *
 * The count is of distinct CLIENTS (pids) in the server-published window
 * list in the shared tail (shmem.h SHM_WINLIST) -- no server round trip --
 * so an app with two windows counts once.  The dock's own bar is excluded
 * by wid (the list carries no nodecor flag; "not the host window" is the
 * precise filter, and the host wid is what hr_wopen parsed).  A click on
 * the cell asks the server for its "Switch to..." dialog (the dock sends
 * C_ACTIVATE with a negative wid -- the catalog's "*" click verb).
 *
 * Loop discipline: identical to zwclock -- a 1-second self-armed SIGALRM
 * heartbeat that repaints the cell on every wake (the dock paints around
 * live widget cells and never signals us; see zwclock.c and zdock.c on the
 * V7 one-shot signal race that forbids it).
 */
#include <signal.h>
#include "shmem.h"
#include "clgfx.h"
#include "hrwidg.h"

/* The value line is the 9x16 System/UI font, the label the sail 6x8 close
 * beneath it (block-centred in the cell; centred FUI text gets +1,+1). */
#define VALY	20		/* value line top (system 9x16)           */
#define LBLY	40		/* label line top (sail 6x8)              */

extern int	strlen();

int	tickflag;
int	strikes;	/* consecutive hr_wlive failures (see the loop) */

static
tick()
{
	signal(SIGALRM, tick);	/* FIRST: V7 one-shot -- see zwclock.c */
	tickflag = 1;
}

/* Count the distinct client pids in the window list, the dock excepted. */
static
countprogs()
{
	HRWIN wl[HRWL_N];
	int pids[HRWL_N];
	register int w, i;
	int n;

	if ( hr_winlist(wl) < 0 )
		return 0;
	n = 0;
	for ( w = 0; w < HRWL_N; w++ )
	{
		if ( !wl[w].ww_used || w == hr_wdockwid() )
			continue;
		for ( i = 0; i < n && pids[i] != wl[w].ww_pid; i++ )
			;
		if ( i == n )
			pids[n++] = wl[w].ww_pid;
	}
	return n;
}

static
paint()
{
	char vbuf[8];
	register int w, n;

	sprintf(vbuf, "%d", countprogs());
	n = strlen(vbuf);
	w = cl_cw();
	cl_begin();
	cl_fillrect(0, 0, w, cl_ch(), 1);
	cl_ptext(SHM_FUI, (w - n * 9) / 2 + 1, VALY + 1, vbuf);
	cl_ptext(SHM_FICON, (w - 5 * 6) / 2, LBLY, "progs");
	cl_end();
}

main(argc, argv)
char **argv;
{
	signal(SIGALRM, tick);		/* FIRST: the dock may poke at once */
	if ( hr_wopen(&argc, argv) < 0 )
		exit(1);		/* not started by a dock */

	if ( cl_mapped() && !cl_frozen() )
		paint();
	alarm(1);
	for (;;)
	{
		pause();
		if ( !tickflag )
			continue;
		tickflag = 0;
		alarm(1);
		if ( !hr_wlive() )
		{
			if ( ++strikes >= 2 )	/* two strikes: one torn list
						 * read must not kill us */
				exit(0);
			continue;
		}
		strikes = 0;
		cl_refresh();
		if ( cl_mapped() && !cl_frozen() )
			paint();
	}
}
