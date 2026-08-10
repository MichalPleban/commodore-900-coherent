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

/* Per-window fast-path "drawing lock-free now" flag (shmem.h SHM_INDRAW), one
 * byte per window written only by that window's client.  Defined HERE -- a
 * separate translation unit from the client's blit and the server's drain loop
 * -- on purpose: the store in hr_setdraw is an optimiser barrier before the
 * client reads `stacking' (so the Dekker order holds), and the server's repeated
 * hr_getdraw in the drain loop is an un-hoistable call (this K&R compiler has no
 * `volatile').  See zview srvlock (drain) and clgfx cl_pbegin (handshake). */
#define INDRAW	((char *)(HRTAIL + SHM_INDRAW))

hr_setdraw(wid, v)
{
	INDRAW[wid] = v;
}

hr_getdraw(wid)
{
	return INDRAW[wid] & 0xff;
}

/* ------------------------------------------------------------------ */
/* connect acknowledgements in the tail (shmem.h SHM_ACK)             */
/* ------------------------------------------------------------------ */
/* Lives here rather than in hrapp.c because the SERVER needs the publish half
 * and does not link hrapp.o.  A slot is one writer (the server) and one reader
 * (the client that owns the pid), and every field is a word the CPU stores
 * atomically, so ak_pid stamped LAST is all the handshake a reader needs. */

hr_ackput(pid, wid, w, h)
{
	register HRACK *a;
	register int i;

	for ( i = 0; i < HRACK_N; i++ )	/* reuse this pid's slot if any */
		if ( hr_ack(i)->ak_pid == pid )
			break;
	if ( i == HRACK_N )
		for ( i = 0; i < HRACK_N; i++ )
			if ( hr_ack(i)->ak_pid == 0 )
				break;
	if ( i == HRACK_N )
		return -1;
	a = hr_ack(i);
	a->ak_wid = wid;
	a->ak_w = w;
	a->ak_h = h;
	a->ak_pid = pid;		/* published last: makes the slot valid */
	return 0;
}

hr_ackget(pid, pwid, pw, ph)
int *pwid, *pw, *ph;
{
	register HRACK *a;
	register int i;

	for ( i = 0; i < HRACK_N; i++ )
	{
		a = hr_ack(i);
		if ( a->ak_pid == pid )
		{
			*pwid = a->ak_wid;
			*pw = a->ak_w;
			*ph = a->ak_h;
			return 1;
		}
	}
	return 0;
}

/* pid 0 clears every slot (server start-up: the tail is uninitialised RAM). */
hr_ackclr(pid)
{
	register int i;

	for ( i = 0; i < HRACK_N; i++ )
		if ( pid == 0 || hr_ack(i)->ak_pid == pid )
			hr_ack(i)->ak_pid = 0;
	return 0;
}

/* ------------------------------------------------------------------ */
/* per-window event rings (shmem.h SHM_EVQ)                           */
/* ------------------------------------------------------------------ */
/* Single producer (server) / single consumer (the window's client), so the
 * indices need no lock -- only ordered stores, which this in-order CPU gives.
 * The kernel is touched ONLY to block or to wake, never to move an event. */

static int	evfd = -1;		/* an hr-driver fd for the doorbell */

static
hr_evio(cmd, i)
{
	if ( evfd < 0 )
		evfd = open("/dev/dmgr", 2);	/* non-exclusive, any client may */
	if ( evfd >= 0 )
		ioctl(evfd, cmd, (char *)i);
	return 0;
}

/* Reset a ring (i < 0 = all).  The server does this at start-up, because the
 * tail is uninitialised RAM, and again whenever a window id is reused -- a
 * stale client must never inherit the previous occupant's queue. */
hr_evinit(i)
{
	register HREVQ *q;
	register int k;

	if ( i < 0 )
	{
		for ( k = 0; k < EVQ_N; k++ )
			hr_evinit(k);
		return 0;
	}
	q = hr_evq(i);
	q->eq_head = 0;
	q->eq_tail = 0;
	q->eq_wait = 0;
	q->eq_over = 0;
	return 0;
}

/* Producer.  NEVER blocks: a full ring sets eq_over and drops the event, so a
 * client that has stopped draining can no longer wedge the server (which is
 * what the old pipe write could do, hence qexpose/flushexp). */
hr_evput(i, ev)
short *ev;
{
	register HREVQ *q;
	register short *d;
	register int k;

	q = hr_evq(i);
	if ( (short)(q->eq_head - q->eq_tail) >= EVQ_SLOTS )
	{
		q->eq_over = 1;
		return -1;
	}
	d = q->eq_ev[q->eq_head & EVQ_MASK];
	for ( k = 0; k < 8; k++ )
		d[k] = ev[k];
	q->eq_head++;			/* published AFTER the payload */
	if ( q->eq_wait )
		hr_evio(CIOEVWAKE, i);	/* only trap if someone is asleep */
	return 0;
}

/* Consumer.  Returns 1 and fills ev, or 0 when the ring is empty. */
hr_evget(i, ev)
short *ev;
{
	register HREVQ *q;
	register short *s;
	register int k;

	q = hr_evq(i);
	if ( q->eq_head == q->eq_tail )
		return 0;
	s = q->eq_ev[q->eq_tail & EVQ_MASK];
	for ( k = 0; k < 8; k++ )
		ev[k] = s[k];
	q->eq_tail++;
	return 1;
}

/* Read and clear "the server had to drop events for me". */
hr_evover(i)
{
	register HREVQ *q;
	register int was;

	q = hr_evq(i);
	was = q->eq_over;
	q->eq_over = 0;
	return was;
}

/* Block until ring i has something.  Announces eq_wait BEFORE the final
 * emptiness test so a producer between the two cannot lose the wake -- the
 * driver re-checks under sphi() as well, closing it on the kernel side too.
 * Returns immediately (no system call) when an event is already queued.
 *
 * THE SESSION-DEATH EXIT LIVES HERE.  When the server dies, its watchdog
 * (zview.c srvwatch / zvwatch.c) clears the tail magic and rings every
 * doorbell; a client that would wait on a dead session has nothing left to
 * wait FOR -- no events will ever come, and any drawing it still does lands
 * on the restored text console.  Every client's idle point is this function,
 * so this one check ends them all: cheaper and surer than teaching each
 * application's event loop about dying.  (The magic is already set before
 * any client can exist -- the server stamps the tail before the rc script
 * and the launcher icons -- so a live session never trips this.) */
hr_evwait(i)
{
	register HREVQ *q;

	if ( hr_glob()->magic != HR_MAGIC )
		exit(1);			/* the session is over */
	q = hr_evq(i);
	if ( q->eq_head != q->eq_tail )
		return 0;
	q->eq_wait = 1;
	if ( q->eq_head != q->eq_tail )	/* raced with a producer: no sleep */
	{
		q->eq_wait = 0;
		return 0;
	}
	hr_evio(CIOEVWAIT, i);
	q->eq_wait = 0;
	if ( hr_glob()->magic != HR_MAGIC )	/* the doorbell was the wake-up-and-die */
		exit(1);
	return 0;
}
