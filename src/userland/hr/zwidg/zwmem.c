/*
 * zwmem.c - dock widget: free user memory.
 *
 * The minimal cut of zmon's memory pane (which is itself /bin/mem's walk):
 * chase the kernel's in-core segment queue (segmq) through /dev/kmem summing
 * every segment's size, and free = the user span (coretop - corebot) minus
 * that sum.  One click is 1 Kb, so the figures are Kb directly.  Needs the
 * kernel headers (-I flags) and a symboled /coherent, exactly like zmon and
 * ps; with no /dev/kmem (or a stripped kernel) the cell shows "--" and keeps
 * ticking.
 *
 * Unlike zmon this walk keeps NO snapshot of the kalloc arena.  zmon has to
 * buffer the whole arena because it chases pointers all over it (procq, then
 * every process's p_segp[] and u-area) dozens of times a tick; this widget
 * touches exactly two fields of each SEG -- s_forw to advance and s_size to
 * accumulate -- so it reads each 24-byte SEG where it lies.  The malloc()ed
 * copy was `casize' (asize_, ~33 Kb) of heap, and this machine charges a
 * process its data up front: it made a 1.6 Kb widget cost 47 Kb of core,
 * more than zterm.  In place it is ~14 Kb, the same as its sibling widgets.
 *
 * The price is one lseek+read pair per segment instead of one for the lot:
 * measured at ~440 us per /dev/kmem read (and that figure is flat from 1
 * byte to 32000 -- the c900 emulator charges the syscall but not the block
 * copy, so it cannot price the snapshot's 33 Kb move at all).  With ~16
 * segments live that is ~7 ms per walk, and the walk runs every SAMPLE
 * ticks and only when the cell is actually on screen (see main).
 *
 * Loop discipline: the zwclock 1-second self-armed SIGALRM heartbeat --
 * every wake repaints (the dock paints around live widget cells and never
 * signals us; see zwclock.c and zdock.c on the V7 one-shot signal race
 * that forbids it) -- but the KERNEL WALK runs only every 3rd tick: a
 * repaint is a handful of primitives, a queue walk is a read per segment.
 *
 * The widget itself takes no input: clicks land in the dock, whose catalog
 * wires this cell to the Monitor (the "Monitor=/usr/hr/bin/zmon" click
 * verb), so the Monitor icon can be taken off the bar.
 */
#include <stdio.h>
#include <const.h>
#include <param.h>
#include <l.out.h>
#include <seg.h>
#include <signal.h>
#include "shmem.h"
#include "clgfx.h"
#include "hrwidg.h"

/* The value line is the 9x16 System/UI font, the label the sail 6x8 close
 * beneath it (block-centred in the cell; centred FUI text gets +1,+1). */
#define VALY	20		/* value line top (system 9x16)           */
#define LBLY	40		/* label line top (sail 6x8)              */

#define SAMPLE	3		/* kernel walk every SAMPLE ticks         */

extern int	strlen();

/* Is a kernel pointer inside the kalloc arena?  (ps.c's range(), without
 * its map() twin: nothing is copied into a local buffer here.) */
#define	range(p)	((char *)(p) >= (char *)aend && \
			 (char *)(p) < (char *)aend + casize)

#define	aasize		nl[0].n_value
#define	aend		nl[1].n_value
#define	asegmq		nl[2].n_value
#define	acorebot	nl[3].n_value
#define	acoretop	nl[4].n_value

struct nlist nl[] = {
	"asize_",	0,	0,
	"end_",		0,	0,
	"segmq_",	0,	0,
	"corebot_",	0,	0,
	"coretop_",	0,	0,
	/* The terminator must be a COMPLETE initializer group: the z8001 PCC
	 * drops a trailing partial group, so a bare "" would leave the array
	 * one element short and nlist() would scan past its end. */
	"",		0,	0
};

int	kfd = -1;		/* /dev/kmem; -1 = no data                */
unsigned casize;		/* arena size (the range() bound)         */
saddr_t	corebot, coretop;	/* user memory bounds, clicks (= Kb)      */
unsigned mfree;			/* the figure on display, Kb              */

int	tickflag;
int	stale;		/* a walk is due: taken when we can paint */
int	strikes;	/* consecutive hr_wlive failures (see the loop) */

static
tick()
{
	signal(SIGALRM, tick);	/* FIRST: V7 one-shot -- see zwclock.c */
	alarm(1);		/* re-armed by the HANDLER, so a delivery
				 * landing outside pause() (mid-sample, or
				 * before a descheduled process reaches the
				 * pause) cannot strand the loop in pause()
				 * with no alarm pending -- see zwclock.c */
	tickflag = 1;
}

/* Read from /dev/kmem: kernel data addresses are 16-bit offsets. */
static
kread(s, bp, n)
long s;
char *bp;
{
	unsigned x;

	x = s;
	s = x;
	if ( kfd < 0 )
		return -1;
	lseek(kfd, s, 0);
	if ( read(kfd, bp, n) != n )
	{
		kfd = -1;
		return -1;
	}
	return 0;
}

/* One-time set-up: namelist, /dev/kmem and the boot-fixed bounds.  On
 * failure kfd stays -1 and the cell shows "--" forever. */
static
initdata()
{
	nlist("/coherent", nl);
	if ( nl[0].n_type == 0 )
		return 0;
	if ( (kfd = open("/dev/kmem", 0)) < 0 )
		return 0;
	if ( kread((long)aasize, (char *)&casize, sizeof(casize)) < 0 )
		return 0;
	kread((long)acorebot, (char *)&corebot, sizeof(corebot));
	kread((long)acoretop, (char *)&coretop, sizeof(coretop));
	return 0;
}

/* Resample: the segment walk (mem's figures), one SEG read at a time.
 * `seg' holds the queue head first and, from then on, the segment we are
 * standing on -- its s_forw carries us to the next.  The trip count is
 * capped at what the arena could physically hold, so a torn or corrupt
 * queue cannot spin us on the kernel forever. */
static void
sample()
{
	register SEG *sp;
	register unsigned n;
	SEG seg;
	unsigned used;

	if ( kfd < 0 )
		return;
	if ( kread((long)asegmq, (char *)&seg, sizeof(seg)) < 0 )
		return;
	used = 0;
	n = casize / sizeof(SEG);
	for ( sp = seg.s_forw; sp != (SEG *)asegmq; sp = seg.s_forw )
	{
		if ( range((char *)sp) == 0 || n-- == 0 )
			break;		/* mid-flight change: show what we have */
		if ( kread((long)sp, (char *)&seg, sizeof(seg)) < 0 )
			return;
		used += seg.s_size;
	}
	mfree = (coretop - corebot) - used;
}

static
paint()
{
	char vbuf[10];
	register int w, n;

	if ( kfd < 0 )
		strcpy(vbuf, "--");
	else
		sprintf(vbuf, "%uK", mfree);
	n = strlen(vbuf);
	w = cl_cw();
	cl_begin();
	cl_fillrect(0, 0, w, cl_ch(), 1);
	cl_ptext(SHM_FUI, (w - n * 9) / 2 + 1, VALY + 1, vbuf);
	cl_ptext(SHM_FICON, (w - 4 * 6) / 2, LBLY, "free");
	cl_end();
}

main(argc, argv)
char **argv;
{
	int tickn;

	signal(SIGALRM, tick);		/* FIRST: the dock may poke at once */
	if ( hr_wopen(&argc, argv) < 0 )
		exit(1);		/* not started by a dock */

	initdata();
	sample();
	if ( cl_mapped() && !cl_frozen() )
		paint();
	alarm(1);
	tickn = 0;
	for (;;)
	{
		pause();
		if ( !tickflag )
			continue;
		tickflag = 0;
		/* (the handler re-armed the alarm -- see tick()) */
		if ( !hr_wlive() )
		{
			if ( ++strikes >= 2 )	/* two strikes: one torn list
						 * read must not kill us */
				exit(0);
			continue;
		}
		strikes = 0;
		if ( ++tickn >= SAMPLE )
			tickn = 0, stale = 1;
		cl_refresh();
		if ( cl_mapped() && !cl_frozen() )
		{
			/* the walk is taken here, not above: only when it can
			 * be seen, and on the map state cl_refresh() has just
			 * brought back rather than the previous pass's */
			if ( stale )
			{
				stale = 0;
				sample();
			}
			paint();
		}
	}
}
