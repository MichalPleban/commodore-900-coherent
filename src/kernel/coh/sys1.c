/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * General system calls.
 */
#include <coherent.h>
#include <acct.h>
#include <drvcon.h>
#include <errno.h>
#include <proc.h>
#include <sched.h>
#include <seg.h>
#include <signal.h>
#include <timeb.h>
#include <times.h>
#include <uproc.h>

/*
 * Send a SIGALARM signal in `n' seconds.
 */
ualarm(n)
unsigned n;
{
	register unsigned s;

	s = SELF->p_alarm;
	SELF->p_alarm = n;
	return (s);
}

/*
 * Change the size of our data segment.
 * This version hacked to work on the Z8001 but
 * is portable if the macroes are used properly.
 */
char *
ubrk(cp)
register char *cp;
{
	register SEG *sp;
	register paddr_t sb;
	register int si;

	si = SIPDATA;
#if Z8001
	/*
	 * A shared-library client's heap lives behind the attached
	 * library data in hardware segment 1 (or 2): the library's
	 * sbrk caches a break address in its own data segment (its
	 * `end').  Grow the segment the passed address actually lies
	 * in.  Unlike SIPDATA, those segments cannot spill past 64K
	 * (mproto() forces the next hardware segment after each).
	 */
	if (cp != NULL) {
		switch ((int)((vaddr_t)cp >> 24)) {
		case USTACK+1:
			si = SISSLIB;
			break;
		case USTACK+2:
			si = SIPSLIB;
			break;
		}
		if (SELF->p_segp[si] == NULL)
			si = SIPDATA;
	}
#endif
	sp = SELF->p_segp[si];
	sb = vtop(u.u_segl[si].sr_base);
	if (cp != NULL) {
#if Z8001
		if (si != SIPDATA && vtop(cp) - sb > ctob((paddr_t)MSSIZE)) {
			u.u_error = ENOMEM;
			sb += ctob((paddr_t)sp->s_size);
			return ((char *)ptov(sb));
		}
#endif
		segsize(sp, vtop(cp) - sb);
	}
	sb += ctob((paddr_t)sp->s_size);
	return ((char *)ptov(sb));
}

/*
 * Execute an l.out.
 */
uexece(np, argp, envp)
char *np;
char *argp[];
char *envp[];
{
	pexece(np, argp, envp);
}

/*
 * Exit.
 */
uexit(s)
{
	pexit(s<<8);
}

/*
 * Fork.
 */
ufork()
{
	return (pfork());
}

/*
 * Return date and time.
 */
uftime(tbp)
struct timeb *tbp;
{
	register int s;
	struct timeb timeb;

	timeb.time = timer.t_time;
	/* This should be a machine.h macro to avoid
	 * unnecessary long arithmetic and roundoff errors
	 */
	timeb.millitm = timer.t_tick*(1000/HZ);
	timeb.timezone = timer.t_zone;
	timeb.dstflag = timer.t_dstf;
	s = sphi();
	kucopy(&timeb, tbp, sizeof(timeb));
	spl(s);
}

/*
 * Get effective group id.
 */
ugetegid()
{
	return (u.u_gid);
}

/*
 * Get effective user id.
 */
ugeteuid()
{
	return (u.u_uid);
}

/*
 * Get group id.
 */
ugetgid()
{
	return (u.u_rgid);
}

/*
 * Get process id.
 */
ugetpid()
{
	return (SELF->p_pid);
}

/*
 * Get user id.
 */
ugetuid()
{
	return (u.u_ruid);
}

/*
 * Send the signal `sig' to the process with id `pid'.
 *
 * `pid' > 0	the process with that id.
 * `pid' == 0	every process in the sender's process group.
 * `pid' < -1	every process in process group -`pid'.
 * `pid' == -1	every process the sender may signal, except the scheduler,
 *		init, and kernel processes.
 *
 * A `sig' of 0 sends nothing: it only tests whether the target exists and
 * whether we would be allowed to signal it.
 */
ukill(pid, sig)
int pid;
register unsigned sig;
{
	register PROC *pp;
	register int sigflag;

	if (sig > NSIG) {
		u.u_error = EINVAL;
		return;
	}
	sigflag = 0;
	lock(pnxgate);
	if (pid > 0) {
		for (pp=procq.p_nforw; pp!=&procq; pp=pp->p_nforw) {
			if (pp->p_state == PSDEAD)
				continue;
			if (pp->p_pid == pid) {
				sigflag = 1;
				if (sig != 0) {
					if (sigperm(sig, pp))
						sendsig(sig, pp);
					else
						u.u_error = EPERM;
				}
				break;
			}
		}
	} else if (pid < -1) {
		pid = -pid;
		for (pp=procq.p_nforw; pp!=&procq; pp=pp->p_nforw) {
			if (pp->p_state == PSDEAD)
				continue;
			if (pp->p_group == pid) {
				sigflag = 1;
				if (sig != 0) {
					if (sigperm(sig, pp))
						sendsig(sig, pp);
					else
						u.u_error = EPERM;
				}
			}
		}
	} else if (pid == 0) {
		for (pp=procq.p_nforw; pp!=&procq; pp=pp->p_nforw) {
			if (pp->p_state == PSDEAD)
				continue;
			if (pp->p_group == SELF->p_group) {
				sigflag = 1;
				if (sig!=0 && sigperm(sig, pp))
					sendsig(sig, pp);
			}
		}
	} else {
		for (pp=procq.p_nforw; pp!=&procq; pp=pp->p_nforw) {
			if (pp->p_state == PSDEAD)
				continue;
			if (pp->p_pid == 0)
				continue;
			if (pp->p_pid == 1)
				continue;
			if ((pp->p_flags&PFKERN) != 0)
				continue;
			sigflag = 1;
			if (sig!=0 && super())
				sendsig(sig, pp);
		}
	}
	unlock(pnxgate);
	if (sigflag == 0)
		u.u_error = ESRCH;
	return (0);
}

/*
 * See if we have permission to send the signal, `sig' to the process, `pp'.
 */
sigperm(sig, pp)
register PROC *pp;
{
	if (u.u_uid == pp->p_uid)
		return (1);
	if (u.u_ruid == pp->p_ruid) {
		if (sig == SIGHUP
		||  sig == SIGINT
		||  sig == SIGQUIT
		||  sig == SIGTERM)
			return (1);
	}
	if (u.u_uid == 0) {
		u.u_flag |= ASU;
		return (1);
	}
	return (0);
}

/*
 * Lock a process in core.
 */
ulock(f)
{
	if (super() == 0)
		return;
	if (f)
		SELF->p_flags |= PFLOCK;
	else
		SELF->p_flags &= ~PFLOCK;
	return (0);
}

/*
 * Change priority by the given increment.
 */
unice(n)
register int n;
{
	n += SELF->p_nice;
	if (n < MINNICE)
		n = MINNICE;
	if (n > MAXNICE)
		n = MAXNICE;
	if (n<SELF->p_nice && super()==0)
		return;
	SELF->p_nice = n;
	return (0);
}

/*
 * Non existant system call.
 */
unone()
{
	u.u_error = EFAULT;
}

/*
 * Null system call.
 */
unull()
{
}

/*
 * Pause.  Go to sleep on a channel that nobody will wakeup so that only
 * signals will wake us up.
 */
upause()
{
	for (;;)
		sleep((char *)&u, CVPAUSE, IVPAUSE, SVPAUSE);
}

/*
 * Start profiling.  `bp' is the profile buffer, `n' is the size, `off'
 * is the offset in the users programme and `scale' is the scaling
 * factor.
 */
uprofil(bp, n, off, scale)
register char *bp;
{
	u.u_pbase = bp;
	u.u_pbend = bp + n;
	u.u_pofft = off;
	u.u_pscale = scale;
}

/*
 * Process trace.
 */
uptrace(req, pid, add, data)
int *add;
{
	if (req == 0) {
		SELF->p_flags |= PFTRAC;
		return (0);
	}
	return (ptset(req, pid, add, data));
}

/*
 * Set group id.
 */
usetgid(gid)
register int gid;
{
	if (u.u_gid!=gid && super()==0)
		return;
	u.u_gid = gid;
	u.u_rgid = gid;
	SELF->p_rgid = gid;
	return (0);
}

/*
 * Set user id.
 */
usetuid(uid)
register int uid;
{
	if (uid!=u.u_ruid && super()==0)
		return;
	u.u_uid = uid;
	u.u_ruid = uid;
	SELF->p_uid = uid;
	SELF->p_ruid = uid;
	return (0);
}

/*
 * Set up the action to be taken on a signal.
 */
int *
usignal(sig, f)
register int sig;
int (*f)();
{
	register PROC *pp;
	register sig_t s;
	register int (*o)();
	int ps;

	pp = SELF;
	if (sig<=0 || sig>NSIG || sig==SIGKILL) {
		u.u_error = EINVAL;
		return;
	}
	s = (sig_t)1 << --sig;
	o = u.u_sfunc[sig];
	/*
	 * sendsig() ORs bits into p_ssig at interrupt level, so the
	 * read-modify-write below must run at high priority: otherwise a
	 * signal posted between the load and the store is lost.
	 */
	ps = sphi();
	/* This order is critical to isig's use */
	if (f == SIG_IGN) {
		pp->p_isig |= s;
		u.u_sfunc[sig] = f;
	} else {
		u.u_sfunc[sig] = f;
		pp->p_isig &= ~s;
	}
	pp->p_ssig &= ~s;
	spl(ps);
	return (o);
}

/*
 * Load a device driver.
 */
usload(m, np, cp)
char *np;
CON *cp;
{
	if (super() == 0)
		return;
	pload(m, np, cp);
	return (0);
}

/*
 * Set time and date.
 */
ustime(tp)
register time_t *tp;
{
	register int s;

	if (super() == 0)
		return;
	s = sphi();
	ukcopy(tp, &timer.t_time, sizeof(*tp));
	spl(s);
	return (0);
}

/*
 * Return process times.
 */
utimes(tp)
struct tbuffer *tp;
{
	register PROC *pp;
	struct tbuffer tbuffer;

	pp = SELF;
	tbuffer.tb_utime = pp->p_utime;
	tbuffer.tb_stime = pp->p_stime;
	tbuffer.tb_cutime = pp->p_cutime;
	tbuffer.tb_cstime = pp->p_cstime;
	kucopy(&tbuffer, tp, sizeof(tbuffer));
	return (0);
}

/*
 * Unload a device driver.
 */
usuload(m)
register int m;
{
	if (super() == 0)
		return;
	puload(m);
	return (0);
}


/*
 * Wait for a child to terminate.
 */
uwait(stp)
int *stp;
{
	register PROC *pp;
	register PROC *ppp;
	register PROC *cpp;
	register int pid;

	ppp = SELF;
	for (;;) {
		lock(pnxgate);
		cpp = NULL;
		pp = &procq;
		while ((pp=pp->p_nforw) != &procq) {
			if (pp == ppp)
				continue;
			if (pp->p_ppid != ppp->p_pid)
				continue;
			if ((pp->p_flags&PFSTOP) != 0)
				continue;
			if ((pp->p_flags&PFWAIT) != 0) {
				pp->p_flags &= ~PFWAIT;
				pp->p_flags |= PFSTOP;
				unlock(pnxgate);
				if (stp != NULL)
					putuwd(stp, 0177);
				return (pp->p_pid);
			}
			if (pp->p_state == PSDEAD) {
				ppp->p_cutime += pp->p_utime + pp->p_cutime;
				ppp->p_cstime += pp->p_stime + pp->p_cstime;
				if (stp != NULL)
					putuwd(stp, pp->p_exit);
				pid = pp->p_pid;
				unlock(pnxgate);
				relproc(pp);
				return (pid);
			}
			cpp = pp;
		}
		unlock(pnxgate);
		if (cpp == NULL) {
			u.u_error = ECHILD;
			return;
		}
		sleep((char *)ppp, CVWAIT, IVWAIT, SVWAIT);
	}
}
