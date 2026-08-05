/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
int	ureboot();
struct systab sysdtab[NMDCALL] ={
	0,		  INT,	unone,		/* 128 = sgrow */
	0,		 VOID,	ubpt,		/* 129 = bpt */
	0,		 VOID,	uhalt,		/* 130 = halt */
	0,		 VOID,	ureboot,	/* 131 = reboot */
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
 * Reboot the machine.  Mark all file systems clean, flush everything, then
 * jump to the ROM restart entry (romconf.rom_restart).  Does not return.
 */
ureboot()
{
	if (super() == 0)
		return;
	/*
	 * Sync everything and remount all file systems read only.  Syncing
	 * flushes pending changes -- which makes each file system consistent,
	 * so mssync() marks it clean -- and the read-only remount stops anything
	 * from dirtying it again before the restart.  A file system that was
	 * consistent thus comes back read/write next boot; a crash (no clean
	 * reboot) leaves it dirty -> read only + check.
	 */
	fsshutdown();
	restart();
	/* NOTREACHED (restart re-enters the ROM cold-start) */
}

/*
 * Check to see if the segmentation violation was a red or
 * yellow stack limit violation.  If it was, then, if
 * necessary, back up the PC with a grown stack.
 * Returns non-zero if the violation was handled (the stack is big
 * enough now).  Returns zero if it was not, either because it was
 * no stack violation at all or because the stack could not be grown;
 * our caller then sends the SIGSEGV, which kills the process rather
 * than retrying the faulting instruction forever.
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
	/*
	 * The stack is a single downward growing hardware segment, which
	 * `uproto' refuses to map beyond MSSIZE-2 clicks.  Anything larger
	 * would also wrap `ctob' (MSSIZE clicks is exactly 64K), so stop
	 * here and let the process take the SIGSEGV.
	 */
	if (nb >= MSSIZE-1)
		return (0);
	if (segsize(sp, ctob((vaddr_t)nb)) == 0)
		return (0);
	return (1);
}
