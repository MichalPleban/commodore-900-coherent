/*
 * zwclock.c - dock widget: the time of day.
 *
 * Started by zdock from an "@" catalog line (see zdock.c), never from a
 * shell: hr_wopen() parses the -W cell contract and enters clgfx sub-surface
 * mode, after which the cl_* primitives draw inside the dock bar's cell,
 * clipped by the dock window's published visible rects.
 *
 * The loop is a bare 1-second SIGALRM heartbeat: every wake repaints the
 * cell outright -- a 60px cell is a handful of primitives, so no change
 * detection is worth having.  That one rule also heals any damage that
 * reaches the cell (an uncover, a failed save-under) within a second: the
 * dock paints AROUND live widget cells and never signals us -- a second,
 * asynchronous SIGALRM source could land in the V7 one-shot re-install
 * window and kill us (see zdock.c) -- so our own serialized alarm() is the
 * only SIGALRM in this process.  hr_wlive() is the exit gate: dock gone
 * (or its wid reused), widget gone.
 *
 * The widget itself takes no input: clicks land in the dock, whose catalog
 * wires this cell to the full clock app (the "Clock=/usr/hr/bin/zclock"
 * click verb), so the Clock icon can be taken off the bar.
 */
#include <time.h>
#include <signal.h>
#include "shmem.h"
#include "clgfx.h"
#include "hrwidg.h"

/* The value line is the 9x16 System/UI font, the label the sail 6x8 close
 * beneath it; the pair is centred as a block in the 69px cell.  FUI glyphs
 * sit 1px high-left in their cell, so centred FUI text gets +1,+1. */
#define VALY	20		/* value line top (system 9x16)           */
#define LBLY	40		/* label line top (sail 6x8)              */

extern int	strlen();

static char *dayname[7] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

int	tickflag;
int	strikes;	/* consecutive hr_wlive failures (see the loop) */

static
tick()
{
	signal(SIGALRM, tick);	/* FIRST: this kernel's signal() is V7
				 * one-shot (delivery resets to SIG_DFL,
				 * whose action KILLS) -- re-install before
				 * anything else narrows the unhandled
				 * window as far as user code can */
	tickflag = 1;
}

static
paint()
{
	char tbuf[8], dbuf[10];
	long t;
	struct tm *tp;
	register int w;

	time(&t);
	tp = localtime(&t);
	sprintf(tbuf, "%02d:%02d", tp->tm_hour, tp->tm_min);
	sprintf(dbuf, "%s %d", dayname[tp->tm_wday], tp->tm_mday);
	w = cl_cw();
	cl_begin();
	cl_fillrect(0, 0, w, cl_ch(), 1);
	cl_ptext(SHM_FUI, (w - 5 * 9) / 2 + 1, VALY + 1, tbuf);
	cl_ptext(SHM_FICON, (w - strlen(dbuf) * 6) / 2, LBLY, dbuf);
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
		alarm(1);		/* re-armed before the work: a signal
					 * landing mid-paint costs at most one
					 * 1-second delay, never a stall */
		/* Two strikes to exit: hr_winlist hands back a possibly-torn
		 * copy when its bounded seqlock retry runs out, and one bad
		 * read must not kill a healthy widget.  A really-dead dock
		 * fails every tick, so the exit is only one second later. */
		if ( !hr_wlive() )
		{
			if ( ++strikes >= 2 )
				exit(0);
			continue;
		}
		strikes = 0;
		cl_refresh();
		if ( cl_mapped() && !cl_frozen() )
			paint();
	}
}
