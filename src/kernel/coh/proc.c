/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Process handling and scheduling.
 */
#include <coherent.h>
#include <acct.h>
#include <errno.h>
#include <inode.h>
#include <proc.h>
#include <ptrace.h>
#include <sched.h>
#include <seg.h>
#include <signal.h>
#include <stat.h>
#include <uproc.h>

/*
 * Initialisation.
 * Set up the hash table queues.
 */
pcsinit()
{
	register PROC *pp;
	register PLINK *lp;

	pp = &procq;
	SELF = pp;
	procq.p_nforw = pp;
	procq.p_nback = pp;
	procq.p_lforw = pp;
	procq.p_lback = pp;
	for (lp=&linkq[0]; lp<&linkq[NHPLINK]; lp++) {
		lp->p_lforw = lp;
		lp->p_lback = lp;
	}
}

/*
 * Initiate a process.  `f' is a kernel function that is associated with
 * the process.
 */
PROC *
process(f)
int (*f)();
{
	register PROC *pp1;
	register PROC *pp;
	register SEG *sp;
	MCON mcon;

	if ((pp=kalloc(sizeof(PROC))) == NULL)
		return (NULL);
	pp->p_flags = PFCORE;
	pp->p_state = PSRUN;
	pp->p_ttdev = NODEV;
	if (f != NULL) {
		pp->p_flags |= PFKERN;
		sp = salloc((size_t)UPASIZE, SFSYST|SFHIGH|SFNSWP);
		if (sp == NULL) {
			kfree(pp);
			return (NULL);
		}
		pp->p_segp[SIUSERP] = sp;
		msetsys(&mcon, f, sp->s_mbase);
		kscopy((char *)&mcon, sp, offset(uproc, u_syscon),
			sizeof(mcon));
	}
	lock(pnxgate);
next:
	pp->p_pid = cpid++;
	if (cpid >= NPID)
		cpid = 2;
	pp1 = &procq;
	while ((pp1=pp1->p_nforw) != &procq) {
		if (pp1->p_pid < pp->p_pid)
			break;
		if (pp1->p_pid == pp->p_pid)
			goto next;
	}
	/*
	 * Link `pp' up completely before anybody else points at it: the
	 * chain is walked at interrupt level, where a half spliced entry
	 * would be followed through a null `p_nforw'.
	 */
	pp->p_nforw = pp1;
	pp->p_nback = pp1->p_nback;
	pp1->p_nback->p_nforw = pp;
	pp1->p_nback = pp;
	unlock(pnxgate);
	return (pp);
}

/*
 * Remove a process from the next queue and release and space.
 */
relproc(pp)
register PROC *pp;
{
	lock(pnxgate);
	pp->p_nback->p_nforw = pp->p_nforw;
	pp->p_nforw->p_nback = pp->p_nback;
	unlock(pnxgate);
	kfree(pp);
}

/*
 * Create a clone of ourselves.
 *	N.B. - consave(&mcon) returns twice; anything not initialized
 *	in automatic storage before the call to segadup() will not be
 *	initialized when the second return from consave() commences.
 */
pfork()
{
	register PROC *cpp;
	register PROC *pp;
	register int i;
	register int s;
	MCON mcon;

	if ((cpp=process(NULL)) == NULL) {
		u.u_error = EAGAIN;
		return;
	}
	s = sphi();	/* Make usave a null macro if unnecessary */
	usave();	/* Put the current copy of uarea into its segment */
	spl(s);
	if (segadup(cpp) == 0) {
		u.u_error = EAGAIN;
		relproc(cpp);
		return;
	}
	u.u_rdir->i_refc++;
	u.u_cdir->i_refc++;
	fdadupl();
	pp = SELF;
	cpp->p_uid = pp->p_uid;
	cpp->p_ruid = pp->p_ruid;
	cpp->p_rgid = pp->p_rgid;
	cpp->p_ppid = pp->p_pid;
	cpp->p_ttdev = pp->p_ttdev;
	cpp->p_group = pp->p_group;
	cpp->p_ssig = pp->p_ssig;
	cpp->p_isig = pp->p_isig;
	cpp->p_cval = CVCHILD;
	cpp->p_ival = IVCHILD;
	cpp->p_sval = SVCHILD;
	cpp->p_rval = RVCHILD;
	s = sphi();	/* Added for IBMSWAP */
	consave(&mcon);
	spl(s);		/* Added for IBMSWAP */
	if ((pp=SELF) != cpp) {
		segfinm(cpp->p_segp[SIUSERP]);
		kscopy((char *)&mcon, cpp->p_segp[SIUSERP],
			offset(uproc, u_syscon), sizeof(mcon));
		mfixcon(cpp);
		s = sphi();
		setrun(cpp);
		spl(s);
		return (cpp->p_pid);
	} else {
		u.u_btime = timer.t_time;
		u.u_flag = AFORK;
		for (i=0; i<NUSEG; i++)
			u.u_segl[i].sr_segp = pp->p_segp[i];
		pp->p_flags |= PFSPROTO;
		segload();
		return (0);
	}
}

/*
 * Die.
 */
pexit(s)
{
	register PROC *pp1;
	register PROC *pp;
	register int n;

	pp = SELF;

	/*
	 * Write out accounting directory and close all files associated with
	 * this process.
	 */
	setacct();
	ldetach(u.u_rdir);
	ldetach(u.u_cdir);
	fdaclose();
	if (pp == slprocp)
		slprocp = NULL;

	/*
	 * This must be done in backwards order to prevent freeing the
	 * user area before we free the shared text segment.
	 */
	n = NUSEG;
	while (n--) {
		if (pp->p_segp[n] == NULL)
			continue;
		sfree(pp->p_segp[n]);
		pp->p_segp[n] = NULL;
	}

	/*
	 * Wakeup our parent.  If we have any children, init will become the
	 * new parent.  If there are any children we are tracing who are
	 * waiting for us, we wake them up.
	 */
	pp1 = &procq;
	while ((pp1=pp1->p_nforw) != &procq) {
		if (pp1->p_pid == pp->p_ppid) {
			if (pp1->p_state==PSSLEEP && pp1->p_event==(char *)pp1)
				wakeup((char *)pp1);
		}
		if (pp1->p_ppid == pp->p_pid) {
			pp1->p_ppid = 1;
			if (pp1->p_state == PSDEAD)
				wakeup((char *)eprocp);
			if ((pp1->p_flags&PFTRAC) != 0)
				wakeup((char *)&pts.pt_req);
		}
	}

	/*
	 * We have just released this process' segments, so wake the swapper
	 * if it is waiting on its timer: there may now be enough core to
	 * swap somebody back in without waiting out the rest of the interval.
	 */
	if (stimer.t_func != NULL)
		wakeup((char *)&stimer);

	/*
	 * And finally mark us as dead and give up the processor forever.
	 */
	pp->p_exit = s;
	pp->p_state = PSDEAD;
	dispatch();
}

/*
 * Sleep on the event `e'.  This gives up the processor until someone
 * wakes us up.  Since it is possible for many people to sleep on the
 * same event, the caller when awakened should make sure that what he
 * was waiting for has completed and if not, go to sleep again.  `cl'
 * is the cpu value we get to get the cpu as soon as we are woken up.
 * `sl' is the swap value we get to keep us in memory for the duration
 * of the sleep.  `sr' is the swap value that allows us to get swapped
 * in if we have been swapped out.
 */
sleep(e, cl, sl, sr)
char *e;
{
	register PROC *bp;
	register PROC *fp;
	register PROC *pp;
	register int s;

	pp = SELF;

	/*
	 * See if we have a signal awaiting.
	 */
	if (cl<CVNOSIG && pp->p_ssig && nondsig()) {
		sphi();
		envrest(&u.u_sigenv);
	}

	/*
	 * Get ready to go to sleep and do so.
	 */
	s = sphi();
	pp->p_state = PSSLEEP;
	pp->p_event = e;
	pp->p_lctim = utimer;
	addu(pp->p_cval, cl);
	pp->p_ival = sl;
	pp->p_rval = sr;
	fp = &linkq[hash(e)];
	bp = fp->p_lback;
	pp->p_lforw = fp;
	fp->p_lback = pp;
	pp->p_lback = bp;
	bp->p_lforw = pp;
	spl(s);
	dispatch();

	/*
	 * We have just woken up.  Get ready to return.
	 */
	subu(pp->p_cval, cl);
	pp->p_ival = 0;
	pp->p_rval = 0;

	/*
	 * Check for an interrupted system call.
	 */
	if (cl<CVNOSIG && pp->p_ssig && nondsig()) {
		sphi();
		envrest(&u.u_sigenv);
	}
}

/*
 * Wake up all processes sleeping on the event `e'.
 */
wakeup(e)
char *e;
{
	register PROC *pp;
	register PROC *pp1;
	register int s;

	pp1 = &linkq[hash(e)];
	pp = pp1;
	s = sphi();
	while ((pp=pp->p_lforw) != pp1) {
		if (pp->p_event == e) {
			pp->p_lback->p_lforw = pp->p_lforw;
			pp->p_lforw->p_lback = pp->p_lback;
			addu(pp->p_cval, (utimer-pp->p_lctim)*CVCLOCK);
			setrun(pp);
			pp = pp1;
			spl(s);
			s = sphi();
		}
	}
	spl(s);
}

/*
 * Reschedule the processor.
 */
dispatch()
{
	register PROC *pp1;
	register PROC *pp2;
	register unsigned v;
	register int s;

	s = sphi();
	pp1 = iprocp;
	pp2 = &procq;
	v = 0;
	while ((pp2=pp2->p_lforw) != &procq) {
		v -= pp2->p_cval;
		if ((pp2->p_flags&PFCORE) == 0)
			continue;
		pp1 = pp2->p_lforw;
		pp1->p_cval += pp2->p_cval;
		pp2->p_cval = v;
		pp1->p_lback = pp2->p_lback;
		pp1->p_lback->p_lforw = pp1;
		pp1 = pp2;
		break;
	}
	spl(s);
	quantum = NCRTICK;
	disflag = 0;
	if (pp1 != SELF) {
		s = sphi();
		SELF = pp1;
		if (consave(&u.u_syscon) == 0)
			conrest(pp1->p_u->s_mbase, offset(uproc, u_syscon));
		else if ((SELF->p_flags&PFKERN) == 0)
			segload();
		spl(s);
	}
}

/*
 * Add a process to the run queue.
 * This routine must be called at high priority.
 */
setrun(pp1)
register PROC *pp1;
{
	register PROC *pp2;
	register unsigned v;

	v = 0;
	pp2 = &procq;
	for (;;) {
		pp2 = pp2->p_lback;
		if ((v+=pp2->p_lforw->p_cval) >= pp1->p_cval)
			break;
		if (pp2 == &procq)
			break;
	}
	pp2->p_lforw->p_lback = pp1;
	pp1->p_lforw = pp2->p_lforw;
	pp2->p_lforw = pp1;
	pp1->p_lback = pp2;
	v -= pp1->p_cval;
	pp1->p_cval = v;
	pp1->p_lforw->p_cval -= v;
	pp1->p_state = PSRUN;
}

/*
 * Wait for the gate `g' to unlock, and then lock it.
 */
lock(g)
register GATE g;
{
	register int s;

	s = sphi();
	while (g[0]) {
		g[1]++;
		sleep((char *)g, CVGATE, IVGATE, SVGATE);
		g[1]--;
	}
	g[0] = 1;
	spl(s);
}

/*
 * Unlock the gate `g'.
 */
unlock(g)
register GATE g;
{
	g[0] = 0;
	if (g[1]) {
		disflag = 1;
		wakeup((char *)g);
	}
}
