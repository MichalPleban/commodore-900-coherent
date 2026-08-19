/*
 * locktest5.c - verify the futex-style drawing lock (Option A).
 *
 *   Phase 1 (recovery) -- RUN FIRST, before the parent ever locks, so the child
 *     inherits mypid==0 and getpid()s its OWN pid (hrlock.c caches mypid, and
 *     fork inherits it; real clients exec, which resets it).  The child takes the
 *     lock via the fast path (the kernel never sees it) then _exit()s WITHOUT
 *     unlocking.  The parent then blocks on the lock; the watchdog must reclaim
 *     it from the dead child or the parent hangs.  recover==1 iff reacquired;
 *     recl1 is the reclaim count (>=1 proves the watchdog acted).
 *   Phase 0 (fast path): NUNCON uncontended lock/unlock pairs in one process with
 *     the slow-path counters reset -- slowL==0 && slowU==0 proves the TSET fast
 *     path never traps into the kernel (the whole point of the change).
 *   Phase 2 (regression): NPROC processes contend; viol must be 0 and reclF must
 *     equal recl1 (the watchdog must NEVER steal a LIVE lock).
 *
 * Needs the hi-res machine (the futex word lives in the user-mapped VRAM tail).
 * Result -> /lockout (+ sync).  Good result:
 *   recover=1 recl1=1 uncon=5000 slowL=0 slowU=0 reclF=1 cnt=60 exp=60 viol=0
 */
#include <stdio.h>

extern int	hr_lock();
extern int	hr_unlock();

#define TAIL	0x38000000L
#define FUTEXP	((short *)(TAIL + 0x3800))	/* the lock word itself          */
#define DIAGL	((short *)(TAIL + 0x3840))	/* driver slow-path lock count   */
#define DIAGU	((short *)(TAIL + 0x3842))	/* driver slow-path unlock count */
#define DIAGR	((short *)(TAIL + 0x3844))	/* driver watchdog reclaim count */
#define DIAGF	((short *)(TAIL + 0x3846))	/* userland fast-path acquire count */
#define P2OWN	((short *)(TAIL + 0x3820))
#define P2VIOL	((short *)(TAIL + 0x3822))
#define P2CNT	((short *)(TAIL + 0x3824))
#define P2DONE	((short *)(TAIL + 0x3826))
#define P2FXZ	((short *)(TAIL + 0x3828))	/* times FUTEX==0 while I hold it */
#define P2FX	((short *)(TAIL + 0x382a))	/* FUTEX value captured at a viol */
#define P2INT	((short *)(TAIL + 0x382c))	/* intruder owner captured at viol */

#define NUNCON	5000
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
	int	slowl, slowu, recl1;
	long	w;
	char	buf[160];

	loaddrv();
	*DIAGL = 0; *DIAGU = 0; *DIAGR = 0; *DIAGF = 0;
	*P2OWN = 0; *P2VIOL = 0; *P2CNT = 0; *P2DONE = 0;
	*P2FXZ = 0; *P2FX = 0; *P2INT = 0;

	/* Phase 1 FIRST: dead FAST-PATH owner (its own pid) -> watchdog reclaims. */
	pid = fork();
	if ( pid == 0 ) { hr_lock((short *)0); _exit(0); }	/* child stamps its pid */
	while ( wait(&st) != pid ) ;
	hr_lock((short *)0);					/* hangs unless reclaimed */
	recover = 1;
	hr_unlock((short *)0);
	recl1 = *DIAGR & 0xffff;

	/* Phase 0: uncontended fast path must never trap into the kernel. */
	*DIAGL = 0; *DIAGU = 0;					/* isolate from Phase 1 */
	for ( i = 0; i < NUNCON; i++ )
	{
		hr_lock((short *)0);
		hr_unlock((short *)0);
	}
	slowl = *DIAGL & 0xffff;
	slowu = *DIAGU & 0xffff;

	/* Phase 2: live contention -> viol must stay 0, no live-lock reclaim. */
	*DIAGL = 0; *DIAGU = 0; *DIAGF = 0;	/* isolate Phase-2 fast/slow counts */
	isparent = 1;
	for ( i = 0; i < NPROC - 1; i++ )
		if ( fork() == 0 ) { isparent = 0; break; }
	me = getpid() & 0x7fff;
	if ( me == 0 ) me = 1;

	for ( i = 0; i < NITER; i++ )
	{
		hr_lock((short *)0);
		*P2OWN = me;
		if ( *FUTEXP == 0 )			/* I hold, yet the word says free?! */
			*P2FXZ = (*P2FXZ + 1);
		bad = 0;
		for ( w = 0; w < WORK; w++ )
			if ( *P2OWN != me )
				bad = 1;
		if ( bad )
		{
			*P2VIOL = (*P2VIOL + 1);
			*P2FX = *FUTEXP;		/* capture context at a violation */
			*P2INT = *P2OWN;
		}
		*P2CNT = (*P2CNT + 1);
		hr_unlock((short *)0);
	}

	if ( !isparent )
	{
		hr_lock((short *)0); *P2DONE = (*P2DONE + 1); hr_unlock((short *)0);
		_exit(0);
	}

	while ( *P2DONE < NPROC - 1 )
		sink++;
	sprintf(buf,
	    "rec=%d uncon=%d sL0=%d sU0=%d fast=%d p2slowL=%d p2slowU=%d viol=%d fxz=%d fx=%x int=%d cnt=%d\n",
		recover, NUNCON, slowl, slowu, *DIAGF & 0xffff,
		*DIAGL & 0xffff, *DIAGU & 0xffff, *P2VIOL & 0xffff,
		*P2FXZ & 0xffff, *P2FX & 0xffff, *P2INT & 0xffff, *P2CNT & 0xffff);
	fd = creat("/lockout", 0644);
	if ( fd >= 0 ) { write(fd, buf, strlen(buf)); close(fd); }
	sync();
	exit(0);
}
