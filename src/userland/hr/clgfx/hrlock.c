/*
 * hrlock.c - the hrgui global drawing lock, futex-style (GUI.md race fix).
 *
 * A binary lock word in the shared VRAM tail (shmem.h SHM_LOCK): 0 = free,
 * 0xFFFF = held.  Contention on it is RARE -- it only arises when a process is
 * preempted mid-primitive (a short burst of VRAM writes) and another drawing
 * process is scheduled -- so the design pays nothing in the common uncontended
 * case and traps into the kernel only when a process must actually block or wake
 * another:
 *
 *   acquire: TSET the word (hrtas.s).  Was free -> we own it, NO system call.
 *            Held -> trap into the hr driver (CIOMLOCK), which blocks us and,
 *            when woken, takes the word for us under sphi().
 *   release: read SHM_WAIT -- the count of processes blocked in the kernel,
 *            maintained there under sphi().  Zero (the common case) -> store 0
 *            to the word, NO system call.  Non-zero -> CIOMUNLOCK, which HANDS
 *            the lock off to a waiter WITHOUT freeing the word (see hr2.c): this
 *            way a flooding client cannot release and immediately re-grab the
 *            lock ahead of a blocked waiter -- it finds the word still held and
 *            must queue in the kernel like everyone else (fairness).
 *
 * Correctness with only TSET (the Z8001 has no CAS): the "release-then-check-
 * waiters" race is closed by the kernel RE-CHECKING the word under sphi() before
 * it sleeps -- if a contender saw the word held, went to trap, and we released
 * in between, the kernel finds the word free and does not sleep (it just takes
 * it).  A waiter only sleeps after confirming the word is still held under
 * sphi(), and we clear the word BEFORE reading SHM_WAIT, so no wake is ever
 * lost.  See _graphics/hr/src/driver/hr2.c (hrlockwait/hrlockwake).
 *
 * SHM_OWNER is our pid, stamped so the driver's watchdog can reclaim the lock if
 * we die holding it.  mypid is cached (getpid once): exec() resets these statics,
 * and hrgui clients exec, so a stale pid across a bare fork does not arise.
 */
#include "shmem.h"

#define FUTEX	((short *)(HRTAIL + SHM_LOCK))
#define WAITERS	(*(short *)(HRTAIL + SHM_WAIT))
#define OWNER	(*(short *)(HRTAIL + SHM_OWNER))

extern int	hr_tas();

static int	lkfd = -1;
static int	mypid;

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
	if ( mypid == 0 )
		mypid = getpid();		/* once per process (exec resets) */
	if ( hr_tas(FUTEX) == 0 ) {		/* FAST PATH: was free, now ours */
		OWNER = mypid;
		return;				/* ... no system call */
	}
	hr_lkio(CIOMLOCK);			/* SLOW: kernel blocks us, then takes */
						/* the word AND stamps OWNER for us   */
}

hr_unlock(lock)
short *lock;
{
	OWNER = 0;				/* clear the stamp while still holding */
	if ( WAITERS != 0 )			/* someone blocked behind us?          */
		hr_lkio(CIOMUNLOCK);		/* hand the lock off -- kernel keeps the */
						/* word HELD and grants it to a waiter,  */
						/* so we cannot barge back in ahead of it */
	else
		*FUTEX = 0;			/* uncontended: release directly, no syscall */
}
