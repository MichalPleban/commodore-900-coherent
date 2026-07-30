/*
 * locktest2.c - cross-process contention test for the hrgui drawing lock.
 *
 * Runs on the hi-res JS emulator (the VRAM tail is mapped user-accessible into
 * every process, so it holds the shared lock word AND this test's shared state
 * -- no GUI, no driver needed for the baseline spin lock).  Forks NPROC
 * contenders; each, NITER times, acquires the lock, marks itself the sole
 * owner, holds across a window wide enough to be scheduler-preempted, then
 * checks nobody else got in, and releases.
 *
 * Correct mutual exclusion  => viol == 0  AND  count == NPROC*NITER.
 * A spin lock whose budget expires while the (preempted) holder cannot run will
 * "proceed best-effort" -- two processes in the section at once -> viol > 0 and
 * lost counter updates.
 *
 * Result is written to /lockout (the hi-res console is the framebuffer, so we
 * report via a file and extract it with disk.py).
 */
#include <stdio.h>

#define TAIL	0x3b009000L
#define LOCKW	((short *)(TAIL + 0x3800))	/* SHM_LOCK: the drawing lock */
#define COUNTP	((long  *)(TAIL + 0x3820))	/* shared counter             */
#define OWNERP	((short *)(TAIL + 0x3824))	/* current sole owner (pid)   */
#define VIOLP	((short *)(TAIL + 0x3826))	/* mutual-exclusion violations */
#define DONEP	((short *)(TAIL + 0x3828))	/* children finished           */

extern int	hr_lock();
extern int	hr_unlock();

#define NPROC	3
#define NITER	20
#define WORK	60000		/* hold-window width (far reads; preempt-wide) */

long	sink;			/* keeps the hold-window loop from being optimised away */

main()
{
	int	i, isparent, me, fd, bad;
	long	w;
	char	buf[128];

	*LOCKW = 0; *COUNTP = 0; *OWNERP = 0; *VIOLP = 0; *DONEP = 0;

	isparent = 1;
	for ( i = 0; i < NPROC - 1; i++ )
		if ( fork() == 0 ) { isparent = 0; break; }
	me = getpid() & 0x7fff;
	if ( me == 0 ) me = 1;

	for ( i = 0; i < NITER; i++ )
	{
		hr_lock(LOCKW);
		*OWNERP = me;
		bad = 0;
		for ( w = 0; w < WORK; w++ )
			if ( *OWNERP != me )	/* someone else intruded mid-section */
				bad = 1;
		if ( bad )
			*VIOLP = (*VIOLP + 1);
		*COUNTP = (*COUNTP + 1);
		hr_unlock(LOCKW);
	}

	if ( !isparent )
	{
		hr_lock(LOCKW); *DONEP = (*DONEP + 1); hr_unlock(LOCKW);
		_exit(0);
	}

	while ( *DONEP < NPROC - 1 )		/* parent: await children */
		sink++;
	sprintf(buf, "count=%ld expect=%d viol=%d\n",
		*COUNTP, NPROC * NITER, *VIOLP & 0xffff);
	fd = creat("/lockout", 0644);
	if ( fd >= 0 ) { write(fd, buf, strlen(buf)); close(fd); }
	sync();
	exit(0);
}
