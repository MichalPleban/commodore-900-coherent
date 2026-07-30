/*
 * hrlock.c - the hrgui global drawing lock.
 *
 * The lock is a kernel mutex in the hr driver (CIOMLOCK/CIOMUNLOCK): the
 * check-and-sleep on acquire and the clear-and-wake on release both run under
 * the kernel's sphi(), so mutual exclusion actually holds and no wake is lost
 * -- the same gate the kernel uses internally.  A userland spin lock cannot be
 * correct here (a spinner cannot yield the CPU to the preempted holder, so it
 * proceeds best-effort into the section -- measured as 44/60 violations); and a
 * split user/kernel lock loses wakes.  So acquire/release are plain ioctls; the
 * lock argument is ignored (there is a single drawing lock).
 */
#include "shmem.h"

static int	lkfd = -1;

static
hr_lkio(cmd)
{
	if ( lkfd < 0 )
		lkfd = open("/dev/dmgr", 2);	/* any hr-driver node (non-exclusive) */
	if ( lkfd >= 0 )
		ioctl(lkfd, cmd, (char *)0);
}

hr_lock(lock)
short *lock;
{
	hr_lkio(CIOMLOCK);
}

hr_unlock(lock)
short *lock;
{
	hr_lkio(CIOMUNLOCK);
}
