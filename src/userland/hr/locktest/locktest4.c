/*
 * locktest4.c - verify the drawing-lock kernel mutex WITH dead-owner recovery.
 *
 *   Phase 1 (recovery): a child takes the lock, then _exit()s WITHOUT unlocking
 *     (simulating a client killed while it holds the lock).  The parent then
 *     takes the lock; the driver's dead-owner watchdog must reclaim it from the
 *     dead holder, or the parent hangs here forever.  Prints "recover=1" iff it
 *     reacquired -- absence of that line in the output == recovery FAILED.
 *
 *   Phase 2 (regression): NPROC live processes contend on the lock, each checking
 *     that no one else is inside its critical section.  The watchdog is armed the
 *     whole time, so viol==0 proves it never steals a lock from a LIVE owner
 *     (which is exactly the mutual-exclusion breach the kernel-mutex rewrite
 *     fixed).  Correct result: "recover=1" then "count=60 expect=60 viol=0".
 *
 * Runs on the plain serial emulator: results go to stdout (and /lockout+sync).
 */
#include <stdio.h>

extern int	hr_lock();
extern int	hr_unlock();

#define TAIL	0x38000000L
#define COUNTP	((long  *)(TAIL + 0x3820))
#define OWNERP	((short *)(TAIL + 0x3824))
#define VIOLP	((short *)(TAIL + 0x3826))
#define DONEP	((short *)(TAIL + 0x3828))

#define NPROC	3
#define NITER	20
#define WORK	40000

long	sink;

static
loaddrv()
{
	int pid, st;
	pid = fork();
	if ( pid == 0 ) { execl("/etc/load", "load", "/drv/hr", (char *)0); _exit(1); }
	while ( wait(&st) != pid ) ;
}

main()
{
	int	i, isparent, me, bad, recover, pid, st, fd;
	long	w;
	char	buf[96];

	loaddrv();
	*COUNTP = 0; *OWNERP = 0; *VIOLP = 0; *DONEP = 0;

	/* Phase 1: a dead owner leaves the lock held; the watchdog must reclaim it. */
	pid = fork();
	if ( pid == 0 ) { hr_lock((short *)0); _exit(0); }	/* leak the lock */
	while ( wait(&st) != pid ) ;				/* reap the dead owner */
	hr_lock((short *)0);					/* hangs unless reclaimed */
	recover = 1;
	hr_unlock((short *)0);
	printf("recover=%d\n", recover);

	/* Phase 2: live contention with the watchdog armed -> must stay viol=0. */
	isparent = 1;
	for ( i = 0; i < NPROC - 1; i++ )
		if ( fork() == 0 ) { isparent = 0; break; }
	me = getpid() & 0x7fff;
	if ( me == 0 ) me = 1;

	for ( i = 0; i < NITER; i++ )
	{
		hr_lock((short *)0);
		*OWNERP = me;
		bad = 0;
		for ( w = 0; w < WORK; w++ )
			if ( *OWNERP != me )
				bad = 1;
		if ( bad )
			*VIOLP = (*VIOLP + 1);
		*COUNTP = (*COUNTP + 1);
		hr_unlock((short *)0);
	}

	if ( !isparent )
	{
		hr_lock((short *)0); *DONEP = (*DONEP + 1); hr_unlock((short *)0);
		_exit(0);
	}

	while ( *DONEP < NPROC - 1 )
		sink++;
	sprintf(buf, "count=%ld expect=%d viol=%d\n",
		*COUNTP, NPROC * NITER, *VIOLP & 0xffff);
	printf("%s", buf);
	fd = creat("/lockout", 0644);
	if ( fd >= 0 ) { write(fd, buf, strlen(buf)); close(fd); }
	sync();
	exit(0);
}
