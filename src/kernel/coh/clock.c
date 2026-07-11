/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
/*
 * Coherent.
 * Clock.
 * The clock comes in two parts.  There is the routine `clock' which
 * gets called every tick at high priority.  It does the minimum it
 * can and returns as soon as possible.  The second routine, `stand',
 * gets called whenever we are about to return from an interrupt to
 * user mode a low priority.  It can look at flags that the clock set
 * and do the things the clock really wanted to do but didn't have time.
 * Stand is truly the kernel of the system.
 */
#include <coherent.h>
#include <drvcon.h>
#include <proc.h>
#include <sched.h>
#include <stat.h>
#include <timeout.h>
#include <uproc.h>
#include <mdata.h>

/*
 * This routine is called once every tick (1/HZ seconds).
 * It gets called with the programme counter that was interrupted
 * a flag telling whether we were in user or kernel mode and the
 * previous priority we were in.
 */
clock(pc, umode)
vaddr_t pc;
{
	register PROC *pp;

	/*
	 * Ignore clock interrupts till we are ready.
	 */
	if (batflag == 0)
		return;

	/*
	 * Update timers.  Decrement time slice.
	 */
	utimer += 1;
	timer.t_tick += 1;
	timl.t_tinc -= 1;
	quantum -= 1;

	/*
	 * Give processes their schedule values per tick.
	 */
	if (procq.p_lforw->p_cval > CVCLOCK) {
		procq.p_lforw->p_cval -= CVCLOCK;
		procq.p_cval += CVCLOCK;
	}

	/*
	 * Tax current process and update his times.
	 */
	pp = SELF;
	pp->p_cval >>= 1;
	if (umode == 0)
		pp->p_stime++;
	else {
		pp->p_utime++;
		u.u_ppc = pc;
	}
}

/*
 * Do everything the clock wanted to do but couldn't as it would have
 * taken too long.
 * Also perform any system bookkeeping required at regular intervals.
 */
stand()
{
	int s;

	u.u_error = 0;

	/*
	 * Update the clock.
	 */
	while (timer.t_tick >= HZ) {
		timer.t_time++;
		timer.t_tick -= HZ;
		alcflag = 1;
		outflag = 1;
	}

	/*
	 * Check expiration of quantum.
	 */
	if (quantum <= 0) {
		quantum = 0;
		disflag = 1;
	}

	/*
	 * Sound off alarms if needed.
	 */
	if (alcflag) {
		s = sphi();
		if (!locked(pnxgate)) {
			register PROC *pp;

			alcflag = 0;
			pp = &procq;
			while ((pp=pp->p_nforw) != &procq)
				if (pp->p_alarm!=0 && --pp->p_alarm==0)
					sendsig(SIGALRM, pp);
		}
		spl(s);
	}

	/*
	 * Check the timer queue if necessary.
	 */
	if (timl.t_tinc <= 0) {
		register TIM *tp;
		register int (*tf)();
		dold_t dold;

		s = sphi();
		dsave(dold);
		while (timl.t_tinc <= 0) {
			if ((tp = timl.t_next) == NULL) {
				timl.t_tinc = MAXINT;
				break;
			}
			drest(timl.t_dmap);
			timl.t_next = tp->t_next;
			dcopy(timl.t_dmap, tp->t_dmap);
			timl.t_tinc += tp->t_tinc;
			tf = tp->t_func;
			tp->t_func = NULL;
			spl(s);
			(*tf)(tp->t_farg);
			sphi();
		}
		drest(dold);
		spl(s);
	}

	/*
	 * Timeout any devices.
	 */
	if (outflag) {
		register int n;

		outflag = 0;
		for (n=0; n<drvn; n++) {
			if (drvl[n].d_time == 0)
				continue;
			s = sphi();
			dtime((dev_t)makedev(n, 0));
			spl(s);
		}
	}

	/*
	 * Do profiling.
	 */
	if (u.u_pscale != 0) {
		register unsigned p;
		register vaddr_t a;

		p = u.u_pscale;
		a = (int *)u.u_pbase +
		    pscale(u.u_ppc-u.u_pofft, p/sizeof (int));
		if (a < u.u_pbend)
			putuwd(a, getuwd(a)+1);
	}

	/*
	 * Check for signals and execute them.
	 */
	if (SELF->p_ssig)
		actvsig();

	/*
	 * Should we dispatch?
	 */
	if ((SELF->p_flags&PFDISP) != 0) {
		SELF->p_flags &= ~PFDISP;
		disflag = 1;
		wakeup((char *)&stimer);
	}

#ifdef QWAKEUP
	/*
	 * Dispatch pending wakeups.
	 */
	while (ntowake)
		wakeup2();

#endif
	/*
	 * Redispatch.
	 * This used to be a function call in tsave,
	 * expanded in line here.
	 */
	if (disflag) {
		register PROC *pp;

#ifndef QWAKEUP
		s=sphi();
#endif
		if ((pp=SELF)!=iprocp)
			setrun(pp);
		dispatch();
#ifndef QWAKEUP
		spl(s);
#endif
	}
}
