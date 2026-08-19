
/*
 * allocate a free message buffer.
 * The process as the client is made to wait if none are available.
 */
struct mbuf *
hralloc(self)
register uint self;
{
	register struct client	*cp;
	register struct mbuf	*p;
	int	s;

	cp = &client[self];
	s = sphi();
	while ( not ismbfree() ) {
		spl(s);
		hrlock(self, WANTMB);
		cp->flags |= WANTMB;
		hrsleep( &cp->head, self, SLP_MBUF);
		cp->flags &= ~WANTMB;
		wakeup(&cp->flags);
		s = sphi();
	}
	p = mbufp;
	mbufp = p->mb_next;
	spl(s);
	return p;
}


/*
 * put back a free message buffer.
 * This message buffer was allocated but not used and must be
 * reattached to the message queue.
 */
hrunalloc(p)
struct mbuf	*p;
{
	int	s;

	s = sphi();
	p->mb_next = mbufp;
	mbufp = p;
	spl(s);
}


/*
 * free a message buffer.
 * The 1st message buffer in the client's queue is attached to the free
 * buffer list. All clients waiting for a message buffer are notified.
 */
hrfree(self)
uint self;
{
	register struct mbuf	*p;
	register struct client	*cp;
	int	s;

	s = sphi();
	cp = &client[self];
	p = cp->head;
	if ( not (cp->head = p->mb_next) )
		cp->tail = 0;		/* queue drained: never leave `tail' stale */
	if ( p == mstail )
		mstail = 0;
	p->mb_next = mbufp;
	mbufp = p;
	spl(s);
	if ( ismbfree() )
		for (cp=client; cp<endof( client); ++cp)
			if ((cp->flags & WANTMB) ||
			   ((cp->flags & (BUSY+NEEDMSG)) == BUSY+NEEDMSG) )
				wakeup( &cp->head);
}


/*
 * flush an entire message queue.
 * All clients waiting for a message buffer are notified.
 * The queue invariant is `head == 0' means empty, and `tail' is meaningful
 * only while `head' is not 0; an empty queue must therefore be left alone.
 * Splicing through `tail' here with nothing queued wrote through a stale or
 * null pointer and then set the free list head to 0, after which hralloc( )
 * blocked for ever at CVNOSIG - any client could do it with one CIOFLUSH.
 */
hrflush(self)
uint self;
{
	register struct mbuf	*p;
	register struct client	*cp;
	int	s;

	s = sphi();
	cp = &client[self];
	if ( not (p = cp->head) )
	{
		spl(s);
		return;
	}
	cp->head = 0;
	cp->tail->mb_next = mbufp;
	cp->tail = 0;
	mbufp = p;
	if ( self == hrsmgr )
		mstail = 0;
	spl(s);
	if ( ismbfree() )
		for (cp=client; cp<endof( client); ++cp)
			if ((cp->flags & WANTMB) ||
			   ((cp->flags & (BUSY+NEEDMSG)) == BUSY+NEEDMSG) )
				wakeup( &cp->head);
}


/*
 * Return false if the number of free message buffers falls below
 * the reasonable limit.
 */
ismbfree()
{
	register struct mbuf	*p;
	register int	i;
	int		s;

	s = sphi();
	p = mbufp;
	for ( i=0 ; i < 15 ; i++ )
	{
		if ( not p )
		{
			spl(s);
			return 0;
		}
		p = p->mb_next;
	}
	spl(s);
	return ~0;
}


/*
 * wait outside until the door is open, enter, and lock it.
 */
hrlock(self, flag)
uint self;
uint flag;
{
	register struct client	*cp;

	cp = &client[self];
	while ( cp->flags & flag )
		sleep( &cp->flags, 0, 0, 0);
	cp->flags |= flag;
}


/*
 * wait inside  door until condition is met.
 * Unlock the door and leave. Interruptable. Returns FALSE if interrupted.
 */
int hrwait(self, stype)
uint self;
uint stype;
{
	register struct client	*cp;

	cp = &client[self];
	while ( cp->flags & (1 << stype) )
	{
		hrsleep( (stype==SLP_READ ? &cp->datc : &cp->data), self, stype);
		if (SELF->p_ssig && nondsig( ))
		{
			u.u_error = EINTR;
			cp->flags &= ~(1 << stype);
			wakeup( &cp->flags);
			return FALSE;
		}
	}
	wakeup(&cp->flags);
	return TRUE;
}



/*
 * link a message buffer to the client's queue.
 * Wake up anyone waiting for a non-empty client queue.
 */
hrlink(dst, p)
uint	dst;
register struct mbuf *p;
{
	register struct client	*cp;
	struct mbufp *p2;	/*** DEBUG	***/
	int	s;

	s = sphi();
#ifdef DEBUG
	if ( dst == p->mb_m.m_src )
		printf("\007LINKME: dst = src = %d\n", dst);
	for ( cp = client; cp < endof( client); cp++ )
	{
		if ( not cp->ocount )
			continue;
		for ( p2=cp->head ; p2 ; p2=p2->mb_next )
		{
			if ( p == p2 )
			{
				printf("\007LINK %d: in list %d\n", dst, cp-client);
				break;
			}
		}
	}
	for ( p2=mbufp ; p2 ; p2=p2->mb_next )
		if ( p2 == p )
			printf("\007LINK %d: in free list\n", dst);
	if ( (p2 = client[dst].head) )
	{
		while ( p2 && p2->mb_next )
			p2 = p2->mb_next;
		if ( p2 != client[dst].tail )
			printf("\007LINK: bad tail %d\n", dst);
	}
#endif
	cp = &client[dst];
	if (cp->head)
		cp->tail->mb_next = p;
	else
	{
		cp->head = p;
		wakeup( &cp->head);
	}
	cp->tail = p;
	p->mb_next = 0;
	spl(s);
}


/*
 * send signal to process group
 * The signal `sig' is sent to all processes grouped under client `d'.
 * High priority is needed when scanning the proc table.
 *
 * `perm' is TRUE only on the userland path (CIOSIG).  /dev/dmgr is mode 666
 * and non-exclusive, so any user reaches that path with any `d' and any `sig';
 * it therefore gets exactly the kill(2) rules - a legal signal number and
 * sigperm( ) against each target process, EPERM if it fails.  The driver's own
 * signals (the SMGR hold handshake, the teardown SIGSEGV out of hrkey) pass
 * FALSE: their destination is a constant, they must not be refused, and one of
 * them runs at interrupt level where `u' is not the target's user at all.
 */
static
hrsignal( d, sig, perm)
uint	d;
uint	sig;
uint	perm;
{
	register PROC	*pp;
	uint		g,
			s;

	if (d >= DESTMAX)
	{
		/* Internal callers pass a constant, so a bad `d' from one of
		 * them really is a driver bug and stays worth a panic; from
		 * userland it is just a bad argument. */
		if ( not perm )
			panic( "bad dest");
		baddest( );
		return (0);
	}
	if ( perm && (sig == 0 || sig > NSIG) )
	{
		u.u_error = EINVAL;	/* same test ukill( ) applies */
		return (0);
	}
	pp = &procq;
	if ( d < WINDOW )
	{
		if ( g = client[d].pid )
		{
			s = sphi();
			while ( (pp=pp->p_nforw) != &procq )
				if ( pp->p_pid == g )
				{
					if ( not perm )
						sendsig( sig, pp);
					else if ( sigperm( sig, pp) )
						sendsig( sig, pp);
					else
						u.u_error = EPERM;
					break;
				}
			spl( s);
		}
	}
	else
	{
		if (g = client[d].group)
		{
			s = sphi( );
			while ( (pp=pp->p_nforw) != &procq )
				if ( pp->p_group == g )
				{
					if ( not perm )
						sendsig( sig, pp);
					else if ( sigperm( sig, pp) )
						sendsig( sig, pp);
					else
						u.u_error = EPERM;
				}
			spl( s);
		}
	}
}


/*
 * A destination index that arrived from userland was out of range.
 * This is bad user input, not a kernel inconsistency, so it is reported to
 * the caller instead of halting the machine: /dev/dmgr is mode 666, and the
 * old panic( ) here let any user stop the system with a single ioctl.
 * Each call site does its own cleanup (an allocated mbuf, a held state) and
 * returns; this only fixes the error that is reported.
 */
static
baddest( )
{

	u.u_error = EINVAL;
}


/* Cursor hide depth: 0 = cursor shown, >0 = one or more hide brackets active.
 * The cursor is a driver-global resource that the window server AND every
 * direct-render client (GUI.md Model A) must bracket around their own blits or
 * the sprite smears (a blit under the drawn sprite would be stomped by the
 * save-under restore).  Because those processes run concurrently and each keeps
 * its OWN per-process count, the ONLY place they can compose correctly is here:
 * the driver counts the total outstanding hides and reveals the cursor only when
 * the LAST bracket ends.  (ms_en stays the on-screen state its many readers -
 * hrmouse, CIOMOUSE, the evmgr save/restore - already depend on.) */
int	mshide;

/* The hrgui shared-data segment (userland shmem.h: HRTAIL 0x38000000, the
 * GDS segment machine.h defines and the console driver maps).  The driver
 * cannot include that header, so mirror the fields it needs at their fixed
 * absolute addresses -- keep in sync with shmem.h:
 *   HRGLOB_CURX/Y = published so direct-render clients know exactly where the
 *                sprite is and hide it under overlapping blits (SHM_GLOB 0x3000,
 *                fields at +2/+4 after `magic').
 * (The old drawing-lock word at HRTAIL+0x3800 is gone: the lock is now the
 * kernel mutex `hrlocked' below, which hrmouse reads directly to defer the
 * cursor.) */
#define HRGLOB_CURX	(*(short *)0x38003002L)		/* 0x38000000+0x3000+2 */
#define HRGLOB_CURY	(*(short *)0x38003004L)		/* +4                  */

/* Drawing-lock futex state.  The lock word, blocked-waiter count and owner pid
 * all live in the shared-data segment (mirror shmem.h SHM_LOCK/WAIT/OWNER at
 * their fixed absolute addresses) so the uncontended fast path is pure userland
 * (hrlock.c TSETs the word, no syscall).  The kernel runs only the slow path
 * (hrlockwait/hrlockwake below) and the dead-owner watchdog.  hrmouse reads
 * HRFUTEX to defer the async cursor while a draw is in flight. */
#define HRFUTEX	(*(short *)0x38003800L)	/* 0x38000000+0x3800: 0 free/0xFFFF held */
#define HRWAIT	(*(short *)0x38003802L)	/* +0x3802: blocked-waiter count         */
#define HROWNER	(*(short *)0x38003804L)	/* +0x3804: pid of current holder        */

/* Per-window "drawing lock-free now" flags (shmem.h SHM_INDRAW 0x3820, one byte
 * per window, MAX_WINDOWS = 16).  A topmost fully-visible client blits with one
 * of these raised INSTEAD of the futex, so the cursor must be deferred for them
 * exactly as for a lock holder -- else the sprite can land inside the client's
 * in-flight blit and the next move's save-under restore stomps its pixels. */
#define HRINDRAW	((char *)0x38003820L)
#define HRINDRAW_N	16

static int
hrindraw()
{
	register char	*p;
	register int	n;

	p = HRINDRAW;
	for ( n = HRINDRAW_N; n; n-- )
		if ( *p++ )
			return 1;
	return 0;
}

/* Per-window event rings (shmem.h SHM_EVQ 0x4200, HREVQ = 264 bytes: head,
 * tail, wait, over, then EVQ_SLOTS*8 words).  The events themselves move in
 * userland with no system call; the kernel only parks a client whose ring is
 * empty and wakes it when the server queues something.  Same shape as the
 * drawing-lock futex above -- keep the addresses in sync with shmem.h. */
#define HREVQ_BASE	0x38004200L	/* 0x38000000 + 0x4200 */
#define HREVQ_SIZE	264
#define HREVQ_N		17
#define evq(i)		((short *)(HREVQ_BASE + (long)(i) * HREVQ_SIZE))
#define EVQ_HEAD	0		/* short offsets within a ring header */
#define EVQ_TAIL	1
#define EVQ_WAIT	2
static char	hrevchan[HREVQ_N];	/* one sleep channel per ring */
static char	hrlkchan;	/* sleep channel for blocked waiters   */
static int	hrwdon;		/* dead-owner watchdog is queued       */
static int	hrstrk;		/* consecutive stuck-holder observations */
static int	hrlast;		/* holder pid seen at the last watchdog fire */
static int	hrhandoff;	/* a lock hand-off to a waiter is pending */
static int	hrreleaser;	/* pid that just released -- may NOT take the hand-off */
static TIM	hrlktim;	/* watchdog callout                    */
#define HRLKWD	25		/* watchdog poll interval (ticks ~0.25s at 100 Hz) */

/* Low-level: put the cursor on screen (idempotent). */
hrmsedraw()
{
	extern	void hrmouse();

	if ( not mousebuf.ms_en )
	{
		mousebuf.ms_en = ~0;
		hrshow(mousebuf.ms_x/8, mousebuf.ms_y);
		timeout( &timebuf, 1, hrmouse, 0);
	}
}

/* Low-level: take the cursor off screen (idempotent). */
hrmseerase()
{
	if ( mousebuf.ms_en )
	{
		mousebuf.ms_en = 0;
		hrudraw();
	}
}

/* CIOMSEON: end one hide bracket (or the baseline show when none is active). */
hrmseon()
{
	if ( mshide == 0 )
		hrmsedraw();		/* baseline / idempotent show */
	else if ( --mshide == 0 )
		hrmsedraw();		/* last bracket ended -> restore */
}

/* CIOMSEOFF: begin one hide bracket; the first one erases the sprite. */
hrmseoff()
{
	if ( mshide++ == 0 )
		hrmseerase();
}

/* Force the cursor to an absolute state and drop every outstanding hide.  Used
 * only when the event manager changes/teardown (a hard state set, not a
 * bracket) - which also recovers the cursor if a crashed client leaked a hide. */
hrmsereset(on)
{
	mshide = 0;
	if ( on )
		hrmsedraw();
	else
		hrmseerase();
}


void
hrmouse( )
{
	struct mbuf	*p;
	static uint	lastk;
	static		lastxrel,
			lastyrel;
	static int	mspend;		/* a cursor redraw is wanted             */
	static int	msdefer;	/* consecutive ticks it has been deferred */
	register uint	k,
			i;
	register int	x,
			y,
			xrel,
			yrel;
	int	s;

	if ( !mousebuf.ms_en || hrsmgr == NOEVMGR )
		return;
	timeout( &timebuf, 1, hrmouse, 0);
	i = in( XPORT);
	xrel = i & (1<<MBITS)-1;
	k = i & (DSMENU|DWMENU|DACTION);
	yrel = in( YPORT) & (1<<MBITS)-1;
	x = (int)mouse.m_msg[2] + (xrel-lastxrel<<16-MBITS>>16-MBITS);
	if (x < 0)
		x = 0;
	else if (x >= XMAX - MOUSEWI)
		x = XMAX - MOUSEWI - 1;
	y = (int)mouse.m_msg[3] + (yrel-lastyrel<<16-MBITS>>16-MBITS);
	if (y < 0)
		y = 0;
	else if (y >= YMAX - MOUSEHI)
		y = YMAX - MOUSEHI - 1;

	if (x!=mouse.m_msg[2] || y!=mouse.m_msg[3])
	{
		if (mouse.m_msg[0] != SM_MOUSE)
			wakeup( &client[hrsmgr].head);
		mouse.m_msg[0] = SM_MOUSE;
		mouse.m_msg[1] = hrticks;
		mouse.m_msg[2] = x;
		mouse.m_msg[3] = y;
		mspend = 1;		/* sprite wants to move to (x,y) */
	}
	/* Redraw the sprite, preferably while NO userland op holds the drawing
	 * lock -- a blit racing this erase/draw is what left the stray cursor.  We
	 * cannot spin at timer level, so if the lock is held we defer to a later tick
	 * (mspend).  But a client flooding output holds the lock across a whole
	 * screen repaint almost continuously, so an UNBOUNDED defer would freeze the
	 * cursor; after a few ticks we redraw anyway.  A forced redraw is safe unless
	 * the cursor sits exactly on the cells being blitted, and that is precisely
	 * the "painting where the cursor is" case the client hides for -- so at worst
	 * a brief flicker there (the save-under restore may put back a stale patch,
	 * healed by the very next redraw), never a frozen pointer.  hrdraw publishes
	 * the drawn position to the tail so clients hide the sprite accurately.
	 * A raised SHM_INDRAW flag defers exactly like a held lock: it is the
	 * topmost client's lock-free equivalent (clgfx cl_pbegin fast path). */
	if (mspend && ((HRFUTEX == 0 && !hrindraw()) || ++msdefer >= 4))
	{
		hrudraw();			/* erase at the old drawn position */
		hrdraw(mouse.m_msg[2], mouse.m_msg[3]);	/* draw at the latest position */
		mspend = 0;
		msdefer = 0;
	}

	s = sphi();
	if ( (k^lastk) && (p=mbufp) ) {
		mbufp = p->mb_next;
		if ( mstail )
		{
			if ( not (p->mb_next = mstail->mb_next) )
				client[hrsmgr].tail = p;
			mstail->mb_next = p;
		}
		else
		{
			if ( not (p->mb_next = client[hrsmgr].head) )
			{
				client[hrsmgr].tail = p;
				wakeup( &client[hrsmgr].head);
			}
			client[hrsmgr].head = p;
		}
		mstail = p;
		p->mb_m.m_src = DRIVER;
		p->mb_m.m_msg[0] = SM_MKEY;
		p->mb_m.m_msg[1] = hrticks;
		p->mb_m.m_msg[2] = k^lastk | x;
		p->mb_m.m_msg[3] = k | y;
		lastk = k;
#ifdef DEBUG
		for ( p = client[hrsmgr].head; p ; p=p->mb_next )
			if ( p == mstail )
				break;
		if ( not p )
			printf("\007\007HRMOUSE: bad mstail\n");
		else
		{
			while ( p->mb_next )
				p = p->mb_next;
			if ( p != client[hrsmgr].tail )
				printf("\007HRMOUSE: bad queue tail\n");
		}
#endif
	}
	spl(s);

	lastxrel = xrel;
	lastyrel = yrel;
	++hrticks;
}


/* ---- drawing-lock futex slow path (CIOMLOCK / CIOMUNLOCK) ------------------ *
 * The uncontended lock is taken/released entirely in userland (hrlock.c TSETs
 * the shared word HRFUTEX); the kernel is entered ONLY on contention.  Both
 * routines run under sphi(), where on this uniprocessor nothing else executes,
 * so a plain read-modify-write of HRFUTEX is atomic -- no kernel-side TSET is
 * needed.  The invariant that closes the classic lost-wake race with only TSET
 * (no CAS): a waiter sleeps only after re-confirming HRFUTEX is still held here
 * under sphi(), and hr_unlock clears HRFUTEX BEFORE reading HRWAIT, so if the
 * releaser missed our HRWAIT bump we will find the word free and not sleep. */

/* Is process `pid' still alive (present in procq and not exiting)?  Caller must
 * hold sphi() -- we scan procq, exactly like hrsignal. */
static
hralive(pid)
uint	pid;
{
	register PROC	*pp;

	pp = &procq;
	while ( (pp = pp->p_nforw) != &procq )
		if ( pp->p_pid == pid && pp->p_state != PSDEAD )
			return 1;
	return 0;
}

/* Dead-owner recovery.  With the fast path a holder never enters the kernel, so
 * a client that dies holding the lock leaves HRFUTEX stuck; without recovery any
 * blocked waiter would freeze the GUI.  Armed only while a waiter is blocked
 * (HRWAIT != 0), this watchdog reclaims the lock once its holder is gone.
 *
 * It must NEVER reclaim from a LIVE holder -- that would reintroduce the very
 * mutual-exclusion breach this rewrite fixed -- so reclaim requires a POSITIVE
 * dead check on a real pid: two consecutive fires with the word held by the
 * SAME non-zero pid (hrstrk, so a transient is not enough) AND that pid gone
 * (hralive false).  We deliberately do NOT reclaim on HROWNER == 0: a holder is
 * briefly `held with owner 0' both just after its TSET (before stamping its pid)
 * and mid-unlock (after clearing the pid, before clearing the word), and on this
 * uniprocessor a process can be parked in either window across several ticks --
 * reclaiming there would steal a LIVE lock.  The only case left unrecovered is a
 * crash in that couple-instruction window before the pid is stamped: a stuck
 * lock (restart-recoverable), never silent corruption.  Correctness over
 * liveness. */
hrlockwd()
{
	int	s, own, held;

	s = sphi();
	held = (HRFUTEX != 0);
	own  = HROWNER;
	if ( HRWAIT != 0 && held && own != 0 && own == hrlast )
		hrstrk = hrstrk + 1;
	else
		hrstrk = 0;
	hrlast = (held ? own : -1);
	if ( hrstrk >= 2 && own != 0 && not hralive(own) )
	{
		HRFUTEX = 0;			/* reclaim from the dead holder */
		HROWNER = 0;
		hrhandoff = 0;			/* drop any stale hand-off */
		hrstrk  = 0;
		wakeup(&hrlkchan);		/* let a blocked waiter take it */
	}
	if ( HRWAIT != 0 )
		timeout(&hrlktim, HRLKWD, hrlockwd, 0);	/* waiters remain: keep watching */
	else
		hrwdon = 0;
	spl(s);
}

/* CIOMLOCK: block until the lock is free or handed to us, then take it (slow
 * path only).  The hand-off is refused to the process that just released it
 * (hrreleaser): that is what stops a flooding client from releasing and
 * immediately re-grabbing the lock past a waiter (e.g. the server needing it to
 * update z-order/clip).  Any OTHER waiter may take the hand-off. */
hrlockwait()
{
	int	s;

	s = sphi();
	while ( HRFUTEX != 0 && not (hrhandoff && SELF->p_pid != hrreleaser) )
	{
		HRWAIT = HRWAIT + 1;
		if ( not hrwdon )		/* watch for a holder that dies wedged */
		{
			hrwdon = 1;
			timeout(&hrlktim, HRLKWD, hrlockwd, 0);
		}
		sleep(&hrlkchan, CVGATE, IVGATE, SVGATE);
		HRWAIT = HRWAIT - 1;
	}
	if ( hrhandoff && SELF->p_pid != hrreleaser )
		hrhandoff = 0;			/* consume the hand-off */
	HRFUTEX = ~0;				/* take it (0xFFFF); idempotent on a hand-off */
	HROWNER = SELF->p_pid;			/* stamp: race-free (still under sphi) */
	spl(s);
}

/* CIOMUNLOCK: hand the lock to a waiter.  With waiters present we keep HRFUTEX
 * held and mark a hand-off (the releaser cannot re-take it) -- so releasing does
 * NOT free the word, and a flooding client's next hr_tas sees it held and must
 * queue in the kernel like everyone else, giving waiters a fair turn. */
hrlockwake()
{
	int	s;

	s = sphi();
	if ( HRWAIT != 0 )
	{
		hrreleaser = SELF->p_pid;	/* whoever released may not grab it back */
		hrhandoff  = 1;
		wakeup(&hrlkchan);		/* FUTEX stays held: transfer, don't free */
	}
	else
		HRFUTEX = 0;			/* raced to no waiters: release the word */
	spl(s);
}

/* ------------------------------------------------------------------ */
/* event-ring doorbell (CIOEVWAIT / CIOEVWAKE)                        */
/* ------------------------------------------------------------------ */
/* The rings live in the shared VRAM tail and the server/clients move events
 * through them with no system call at all.  The kernel is needed for one thing
 * only: parking a client whose ring is empty, and waking it again.  Exactly the
 * futex split used for the drawing lock above.
 *
 * The sleep/wake race is closed on BOTH sides: the client sets eq_wait before
 * its final emptiness test (hrlock.c hr_evwait), and here we re-test head!=tail
 * under sphi() before sleeping, so an event queued in between is seen rather
 * than slept through.  Interruptible (CVPIPE, below CVNOSIG) so a client that
 * also takes signals -- zclock's SIGALRM tick -- still gets them. */
hrevwait(i)
register int i;
{
	register short *q;
	int	s;

	if ( i < 0 || i >= HREVQ_N )
		return;
	q = evq(i);
	s = sphi();
	while ( q[EVQ_HEAD] == q[EVQ_TAIL] )
	{
		q[EVQ_WAIT] = 1;
		sleep(&hrevchan[i], CVPIPE, IVPIPE, SVPIPE);
		if ( SELF->p_ssig )		/* signalled: let it run its handler */
			break;
	}
	q[EVQ_WAIT] = 0;
	spl(s);
}

hrevwake(i)
register int i;
{
	register short *q;
	int	s;

	if ( i < 0 || i >= HREVQ_N )
		return;
	q = evq(i);
	s = sphi();
	if ( q[EVQ_WAIT] )
	{
		q[EVQ_WAIT] = 0;
		wakeup(&hrevchan[i]);
	}
	spl(s);
}


#define	CTS	0x04
#define	ALS	0x08
uint	hrshift;		/* shift keys	*/

void
hrkey()
{
	register struct mbuf	*p;
	register uint	key;
	int	 i;

	/*
	 * Clear interrupt condition in chip and read data from PA and PC3
	 */
	outb(ZCIO1+PACAS, C_IPIUS);
	key = inb(ZCIO1+PADATA) & 0x7f;
	if ( inb(ZCIO1+PCDATA) & PC2 )
		key += 0x80;
	if ( hrsmgr != NOEVMGR && (p = mbufp) )
	{
		mbufp = p->mb_next;
		hrlink(hrsmgr, p);
		wakeup(&client[hrsmgr].head);	/* should catch all weird cases */
		p->mb_m.m_src = DRIVER;
		p->mb_m.m_msg[0] = SM_KKEY;
		p->mb_m.m_msg[1] = hrticks;
		p->mb_m.m_msg[2] = key;
	}
	outb(ZCIO1+PCDATA, 0);
	outb(ZCIO1+PCDATA, PC3);	
	if ( key == 0x80+0x1d )
		hrshift &= ~CTS;
	else if ( key == 0x80+0x38 )
		hrshift &= ~ALS;
	else if ( key == 0x1d )
		hrshift |= CTS;
	else if ( key == 0x38 )
		hrshift |= ALS;
	/* Alt+Ctrl+HELP is the emergency escape hatch that unloads the driver.
	 * The parentheses are load bearing: `==' binds tighter than `|', so
	 * `hrshift == ALS|CTS' parsed as `(hrshift == ALS) | CTS', which is
	 * always non-zero - the modifier test was dead and HELP alone tore the
	 * GUI down.  Equality, not a mask test: hrshift holds these two bits
	 * and nothing else (see the four cases above), so the two are equivalent
	 * today, and for something this destructive the exact-modifier form is
	 * the safer one to leave behind if another modifier bit is ever added. */
	else if ( key == 0x54 && hrshift == (ALS|CTS) )
	{
		hruload();
		hrsignal( DMGR, SIGSEGV, FALSE );
	}
}


/* Paint the sprite: save the framebuffer bytes under the 24x16 cell into
 * ms_sav, then lay down ink and outline through the opacity mask
 * (screen = screen & ~mask | (mask & ~ink); screen 1 = white, so ink pixels
 * go black and the outline pixels white).  hrhide() restores ms_sav; the two
 * MUST alternate (the ms_en / hrudraw+hrdraw pairing guarantees it), or a
 * second paint would capture the sprite itself as "background". */
hrshow(xcbase, y)
int	xcbase;
register int	y;
{
	register char	*p;
	register char	*q;
	register char	*m;
	char	*s;
	int	yn;

	if ( y < YSPLIT )
		p = (char *)SEG0 + y*(XMAX/8) + xcbase;
	else
		p = (char *)SEG1 + (y-YSPLIT)*(XMAX/8) + xcbase;
	q = (char *)mousebuf.ms_buf;
	m = (char *)mousebuf.ms_msk;
	s = (char *)mousebuf.ms_sav;
	for ( yn = nel(mousebuf.ms_buf) ; yn ; yn-- )
	{
		*s++ = *p;  *p = *p & ~*m | *m & ~*q;  p++; q++; m++;
		*s++ = *p;  *p = *p & ~*m | *m & ~*q;  p++; q++; m++;
		*s++ = *p;  *p = *p & ~*m | *m & ~*q;  p++; q++; m++;
		q++; m++; s++;	/* bad practice, skip over unused byte */
		y++;
		if ( y != YSPLIT )
			p += (XMAX/8) - 3;
		else
			p = (char *)SEG1 + xcbase;
	}
}

/* Take the sprite off screen: put back the saved background bytes. */
hrhide(xcbase, y)
int	xcbase;
register int	y;
{
	register char	*p;
	register char	*s;
	int	yn;

	if ( y < YSPLIT )
		p = (char *)SEG0 + y*(XMAX/8) + xcbase;
	else
		p = (char *)SEG1 + (y-YSPLIT)*(XMAX/8) + xcbase;
	s = (char *)mousebuf.ms_sav;
	for ( yn = nel(mousebuf.ms_sav) ; yn ; yn-- )
	{
		*p++ = *s++;
		*p++ = *s++;
		*p++ = *s++;
		s++;	/* bad practice, skip over unused byte */
		y++;
		if ( y != YSPLIT )
			p += (XMAX/8) - 3;
		else
			p = (char *)SEG1 + xcbase;
	}
}


hrlshift(dx)
{
	register ulong	*lp;
	register int	rows;

	if (!dx)
		return;
	lp = mousebuf.ms_buf;
	for ( rows = nel(mousebuf.ms_buf); rows; rows-- )
	{
		sdll(lp, dx);
		lp++;
	}
	lp = mousebuf.ms_msk;
	for ( rows = nel(mousebuf.ms_msk); rows; rows-- )
	{
		sdll(lp, dx);
		lp++;
	}
}



/*
 * undraw mouse cursor
 */
hrudraw()
{
	hrhide(mousebuf.ms_x/8, mousebuf.ms_y);
}


/*
 * set and draw mouse cursor
 */
hrdraw(x, y)
int	x;
int	y;
{
	int	obit;
	mousebuf.ms_x = x;
	mousebuf.ms_y = y;
	obit = mousebuf.ms_bit;
	hrlshift( (mousebuf.ms_bit = 7 - (x&7)) - obit);
	hrshow(x/8, y);
	HRGLOB_CURX = x;		/* publish the live sprite position for clients */
	HRGLOB_CURY = y;
}
