/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 *	Pseudo-terminal driver (GUI.md sec 4).
 *
 *	LOADABLE (/drv/pty).  This driver is not linked into the resident
 *	kernel - the Z8001 code segment is a hard 64K, so every non-essential
 *	driver lives in its own loadable segment (see notty.c / hrtty).  It is
 *	installed at GUI start-up with `/etc/load /drv/pty' (zview does this,
 *	just as it loads /drv/hr) and resolves the resident TTY line discipline
 *	(ttin/ttout/tt...) from the kernel symbol table via `ld -k'.
 *
 *	`load' allocates ONE segment per configuration table it finds, and the
 *	master and slave sides MUST share this segment's state (ptty[]), so the
 *	whole driver is a SINGLE major (9) with the minor selecting the side:
 *		minor & PTY_MASTER == 0 -> slave  (/dev/ttyp<n>, an ordinary tty)
 *		minor & PTY_MASTER != 0 -> master (/dev/ptyp<n>, the emulator)
 *	minor & PTY_UNIT is the pair index in both cases.  py{open,close,read,
 *	write,ioctl} are thin dispatchers over the slave (pts) and master (ptc)
 *	halves below.
 *
 *	The SLAVE is an ordinary TTY running the stock line discipline
 *	(drv/tty.c): its ttread/ttwrite/ttioctl/ttsetgrp/ttsignal give the shell
 *	a real controlling terminal - ^C -> SIGINT, cooked/cbreak/raw, echo,
 *	erase/kill, stty - for free.  Almost no code lives here.
 *
 *	The MASTER is the GUI terminal emulator's handle.  Four wiring points
 *	(GUI.md sec 4.3), each modeled on existing code:
 *	 1. master write -> slave input: for each byte ttin(slave_tp, c) under
 *	    sphi() - exactly what hrterm1.c htcopy() does.  Runs cooking / echo /
 *	    signals.
 *	 2. slave t_start -> master read wakeup: the slave's start routine, having
 *	    no hardware, simply wakes any blocked master reader (leaving the
 *	    cooked output in t_oq for the master to drain).
 *	 3. master read drives drainage: there is no TX-empty interrupt, so
 *	    ptcread pumps ttout(slave_tp) itself and issues the T_DRAIN / T_HILIM
 *	    writer wakeups that ttstart normally gives after output progress.
 *	 4. carrier = master-open state: T_CARR is asserted on master open; master
 *	    close calls tthup(slave) -> SIGHUP + clears T_CARR, so the shell's
 *	    ttread/ttwrite return EIO.  That is how closing the window hangs up
 *	    the shell.
 *
 *	Controlling-terminal claim (GUI.md sec 4.4): this kernel has no
 *	TIOCNOTTY, and p_ttdev is inherited across fork, so a GUI client forked
 *	by the window server may carry a stale controlling terminal.  On the
 *	FIRST slave open we therefore force-claim: clear the opener's
 *	p_group / p_ttdev, then let ttsetgrp() adopt this slave.  The shell the
 *	emulator execs opens the slave as fds 0/1/2, so it becomes the session
 *	leader with this pty as its controlling terminal.
 */

#include <coherent.h>
#include <drvcon.h>
#include <errno.h>
#include <stat.h>
#include <tty.h>
#include <proc.h>
#include <uproc.h>
#include <sched.h>
#include <signal.h>

#define	NPTY		8	/* number of master/slave pairs		*/
#define	PTY_MASTER	0x08	/* minor bit: 1 = master (ptyp) side	*/
#define	PTY_UNIT	0x07	/* minor bits: pair index (0..NPTY-1)	*/
				/* NPTY 8 is the ceiling this minor	*/
				/* encoding allows: unit 0..7 in the	*/
				/* low 3 bits, bit 3 selects the master	*/

/*
 * Functions.
 */
int	pyopen();
int	pyclose();
int	pyread();
int	pywrite();
int	pyioctl();
int	pyload();
int	ptcopen();
int	ptcclose();
int	ptcread();
int	ptcwrite();
int	ptcioctl();
int	ptsopen();
int	ptsclose();
int	ptsread();
int	ptswrite();
int	ptsioctl();
int	ptstart();
int	nulldev();
int	nonedev();

/*
 * Configuration table.  A SINGLE major (index 9) drives both sides; `load'
 * scans the symbol table for `??con_' (two-char prefix), so this is named
 * `pycon'.  c_load (pyload) wires the slave line discipline once the segment
 * is mapped.
 */
CON pycon ={
	DFCHR,				/* Flags */
	9,				/* Major index */
	pyopen,				/* Open */
	pyclose,			/* Close */
	nulldev,			/* Block */
	pyread,				/* Read */
	pywrite,			/* Write */
	pyioctl,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	pyload,				/* Load */
	nulldev				/* Unload */
};

/*
 * Slave line-discipline state: one TTY per pair.  Initialised by pyload()
 * (t_start = ptstart, t_param = NULL) rather than a static initialiser, so no
 * data-segment pointer needs relocating when the loadable is mapped.
 */
TTY	ptty[NPTY];

int	ptmopen[NPTY];		/* 1 while the master side is open           */
int	ptseof[NPTY];		/* 1 after the slave was opened then closed  */

/* ------------------------------------------------------------------ */
/* load hook: wire each pair's start routine into the resident tty     */
/* line discipline (runs with our segment mapped, GATE held).          */
/* ------------------------------------------------------------------ */

pyload()
{
	register int unit;

	for (unit = 0; unit < NPTY; unit++)
		ptty[unit].t_start = ptstart;
		/* t_param stays NULL (BSS-zeroed): pty has no hardware params */
}

/* ------------------------------------------------------------------ */
/* dispatchers: the minor's PTY_MASTER bit selects master vs slave     */
/* ------------------------------------------------------------------ */

pyopen(dev, mode)
dev_t dev;
{
	if (minor(dev) & PTY_MASTER)
		ptcopen(dev, mode);
	else
		ptsopen(dev, mode);
}

pyclose(dev, mode)
dev_t dev;
{
	if (minor(dev) & PTY_MASTER)
		ptcclose(dev);
	else
		ptsclose(dev);
}

pyread(dev, iop)
dev_t dev;
IO *iop;
{
	if (minor(dev) & PTY_MASTER)
		ptcread(dev, iop);
	else
		ptsread(dev, iop);
}

pywrite(dev, iop)
dev_t dev;
IO *iop;
{
	if (minor(dev) & PTY_MASTER)
		ptcwrite(dev, iop);
	else
		ptswrite(dev, iop);
}

pyioctl(dev, com, vec)
dev_t dev;
struct sgttyb *vec;
{
	if (minor(dev) & PTY_MASTER)
		ptcioctl(dev, com, vec);
	else
		ptsioctl(dev, com, vec);
}

/* ------------------------------------------------------------------ */
/* slave side (pts) - a plain tty                                     */
/* ------------------------------------------------------------------ */

ptsopen(dev, mode)
dev_t dev;
{
	register TTY *tp;
	register int unit, s;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	tp = &ptty[unit];
	s = sphi();
	if (tp->t_open == 0) {
		ttopen(tp);
		/*
		 * First opener of a fresh pair: force this pty to become the
		 * opener's controlling terminal even if it inherited one
		 * (GUI.md sec 4.4) - clear the stale group/ctty so ttsetgrp
		 * below adopts us.
		 */
		SELF->p_group = 0;
		SELF->p_ttdev = NODEV;
	}
	tp->t_open++;
	spl(s);
	ttsetgrp(tp, dev);
}

ptsclose(dev)
dev_t dev;
{
	register TTY *tp;
	register int unit, s;

	unit = minor(dev) & PTY_UNIT;
	tp = &ptty[unit];
	s = sphi();
	if (tp->t_open > 0 && --tp->t_open == 0) {
		ttclose(tp);		/* drains oq (master reads it), resets */
		ptseof[unit] = 1;	/* tell the master: slave is gone -> EOF */
		wakeup((char *)&ptty[unit]);
	}
	spl(s);
}

ptsread(dev, iop)
dev_t dev;
IO *iop;
{
	register int unit;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	ttread(&ptty[unit], iop, SFCW);
}

ptswrite(dev, iop)
dev_t dev;
IO *iop;
{
	register int unit;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	ttwrite(&ptty[unit], iop, SFCW);
}

ptsioctl(dev, com, vec)
dev_t dev;
struct sgttyb *vec;
{
	register int unit, s;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	s = sphi();
	ttioctl(&ptty[unit], com, vec);
	spl(s);
}

/*
 * Slave start routine (tp->t_start).  Called by ttstart() whenever the shell
 * writes or an input echo is queued.  With no hardware to drive, it just wakes
 * a master reader blocked in ptcread(); the cooked bytes stay in t_oq for the
 * master to drain.  If no master is attached, discard the output so the writer
 * (and ttclose's drain) do not hang.
 */
ptstart(tp)
register TTY *tp;
{
	register int unit;

	unit = tp - ptty;
	if (unit < 0 || unit >= NPTY)
		return;
	if (!ptmopen[unit]) {
		while (ttout(tp) >= 0)
			;
		return;
	}
	wakeup((char *)&ptty[unit]);
}

/* ------------------------------------------------------------------ */
/* master side (ptc) - the emulator's handle                          */
/* ------------------------------------------------------------------ */

ptcopen(dev, mode)
dev_t dev;
{
	register TTY *tp;
	register int unit, s;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	tp = &ptty[unit];
	if (ptmopen[unit]) {		/* the master is single-open */
		u.u_error = ENXIO;
		return;
	}
	s = sphi();
	if (tp->t_open == 0)
		ttopen(tp);		/* init the line discipline if idle */
	tp->t_flags |= T_CARR;		/* master present => carrier up */
	ptmopen[unit] = 1;
	ptseof[unit] = 0;
	spl(s);
}

ptcclose(dev)
dev_t dev;
{
	register TTY *tp;
	register int unit, s;

	unit = minor(dev) & PTY_UNIT;
	tp = &ptty[unit];
	s = sphi();
	ptmopen[unit] = 0;
	tthup(tp);			/* SIGHUP the shell, clear carrier */
	wakeup((char *)&tp->t_iq);
	wakeup((char *)&tp->t_oq);
	spl(s);
}

/*
 * Master read: drain the slave's cooked output queue into the user buffer.
 * Block while empty and the slave is still open; return EOF (0 bytes) once the
 * slave has closed.  Replicates ttstart()'s post-progress writer wakeups
 * (T_HILIM / T_DRAIN) since nothing else drives the slave's output flow.
 */
ptcread(dev, iop)
dev_t dev;
register IO *iop;
{
	register TTY *tp;
	register int c, o, unit;
	int got;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	tp = &ptty[unit];
	got = 0;
	while (iop->io_ioc) {
		o = sphi();
		while ((c = ttout(tp)) < 0) {
			if (got) {		/* deliver what we already have */
				spl(o);
				goto flow;
			}
			if (ptseof[unit]) {	/* slave gone: end of file */
				spl(o);
				return;
			}
			sleep((char *)&ptty[unit], CVTTIN, IVTTIN, SVTTIN);
			if (SELF->p_ssig && nondsig()) {
				u.u_error = EINTR;
				spl(o);
				return;
			}
		}
		spl(o);
		if (ioputc(c, iop) < 0)
			break;
		got++;
	}
flow:
	o = sphi();
	if ((tp->t_flags & T_HILIM) != 0 && tp->t_oq.cq_cc <= OLOLIM) {
		tp->t_flags &= ~T_HILIM;
		wakeup((char *)&tp->t_oq);
	}
	if ((tp->t_flags & T_DRAIN) != 0 && tp->t_oq.cq_cc == 0
	    && (tp->t_flags & T_INL) == 0 && tp->t_nfill == 0) {
		tp->t_flags &= ~T_DRAIN;
		wakeup((char *)&tp->t_oq);
	}
	spl(o);
}

/*
 * Master write: inject each byte as slave terminal input via ttin(), exactly
 * as hrterm1.c's htcopy() does.  ttin() runs cooking, echo (queued back to the
 * slave's output, hence readable by this same master) and signal generation.
 */
ptcwrite(dev, iop)
dev_t dev;
register IO *iop;
{
	register TTY *tp;
	register int c, s, unit;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	tp = &ptty[unit];
	while ((c = iogetc(iop)) >= 0) {
		s = sphi();
		ttin(tp, c);
		spl(s);
	}
}

/*
 * Master ioctl: there is a single TTY per pair, so line-discipline ioctls
 * (stty, TIOCSETP, ...) issued on the master configure the same slave line.
 */
ptcioctl(dev, com, vec)
dev_t dev;
struct sgttyb *vec;
{
	register int unit, s;

	unit = minor(dev) & PTY_UNIT;
	if (unit >= NPTY) {
		u.u_error = ENXIO;
		return;
	}
	s = sphi();
	ttioctl(&ptty[unit], com, vec);
	spl(s);
}
