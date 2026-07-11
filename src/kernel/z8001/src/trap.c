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
 * Coherent for Commodore M-series z8001 processor
 * running in Segmented mode.
 * Common handler for system traps.
 */
#include <coherent.h>
#include <errno.h>
#include <proc.h>
#include <signal.h>
#include <systab.h>
#include <uproc.h>
#include <seg.h>

/*
 * Machine dependent system call table.
 */
int	unone();
int	ubpt();
int	uhalt();
struct systab sysdtab[NMDCALL] ={
	0,		  INT,	unone,		/* 128 = sgrow */
	0,		 VOID,	ubpt,		/* 129 = bpt */
	0,		 VOID,	uhalt,		/* 130 = halt */
};

/* Segment trap reason bits */
#define	PWW	0x20			/* Primary write warning (Yellow) */
#define	SLV	0x04			/* Segment length violation (Red) */

/*
 * This is called when a processor trap occurs.
 * The peculiar register declaration forces saving
 * of r6-r12 which may or may not be saved depedning
 * on number of registers declared.
 */
#define	REGS	r0,r1,r2,r3,r4,r5,r6,r7,r8,r9,r10,r11,r12,r13,r14,r15
/* ARGSUSED */
trap(emap, omap, sp, REGS, sig, id, fcw, pc)
unsigned emap, omap;
char *sp, *pc;
unsigned sig, id, fcw;
{
	long l;
	register struct systab *stp;
	register int n;

	u.u_error = 0;
	if ((fcw & MFSYS) != 0) {	/* System mode trap */
#if DDT
		ddt(sig);
#else
		panic("System trap type %d at %p", sig, pc);
#endif
		return;
	} else if (sig != SIGSYS) {	/* Not system call */
		if (sig == SIGSEGV)
			if (stviol(id, pc, sp))
				return;
#if DDT
		ddt(sig);
#endif
		sendsig(sig, SELF);
		return;
	}
	if ((n = (id&0xFF)) < NMICALL)
		stp = &sysitab[n];
	else if (n>=SMDCALL && n<SMDCALL+NMDCALL)
		stp = &sysdtab[n-SMDCALL];
	else {
		sendsig(SIGSYS, SELF);
		return;
	}
	ukcopy((char *)(sp+sizeof(pc)), (char *)u.u_args, stp->s_alen);
	if (u.u_error)
		goto err;
	u.u_io.io_seg = IOUSR;
	if (envsave(&u.u_sigenv)) {
		u.u_error = EINTR;
		goto err;
	}
	l = (*(long(*)())stp->s_func)(u.u_args[0],
				      u.u_args[1],
				      u.u_args[2],
				      u.u_args[3],
				      u.u_args[4],
				      u.u_args[5]);
	if (u.u_error) {
	err:
		l = -1;
		putuwd(MUERR, u.u_error);
		if (u.u_error == EFAULT)
			sendsig(SIGSYS, SELF);
	}
	switch (stp->s_type) {
	case INT:
		r1 = l;
		break;
	case PTR:
	case LONG:
		r0 = ((struct l *)&l)->l_hi;
		r1 = ((struct l *)&l)->l_lo;
	case VOID:
		break;
	default:
		panic("trap: bad return type\n");
	}
}

/*
 * Send a breakpoint signal to ourselves.
 */
ubpt()
{
	sendsig(SIGTRAP, SELF);
}

/*
 * Enter the debugger.
 */
uhalt()
{
	if (super() == 0)
		return;
	halt();
}

/*
 * Check to see if the segmentation violation was a red or
 * yellow stack limit violation.  If it was, then, if
 * necessary, back up the PC with a grown stack.
 * Returns non-zero if a stack growth attempt is made.
 */
stviol(id, pc, usp)
register int id;
register vaddr_t pc, usp;
{
	register SEG *sp;
	register unsigned nb;

	if (id & SLV) {			/* Red stack warning */
		if (getuwi(pc) != 0xA1FD)		/* ld r13, r15 */
			return (0);
		if (getuwi(pc-4) == 0x1CE9)		/* ldm (rr14), rx,$y */
			regl[OPCOFF] -= 4;
		else {
			nb = getuwi(pc -2);
			if (nb == 0x2FED		/* ld (rr14), r13 */
			 || nb == 0x1DEC)		/* ldl (rr14), rr12 */
				regl[OPCOFF] -= 2;
		}
	} else if ((id & PWW) == 0)	/* not a Yellow stack warning */
		return (0);

	sp = SELF->p_segp[SISTACK];
	nb = usp;
	nb = btocru(0-nb);
	if (nb < sp->s_size)
		return (1);
	else if (nb == sp->s_size)
		nb++;
	segsize(sp, (vaddr_t)ctob(nb));
	return (1);
}
