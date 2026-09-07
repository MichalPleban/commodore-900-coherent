/*
 * n1/z8001/reg1.c
 * Machine-specific parts of the cc1 register allocator. Segmented Z8001.
 * Template: n1/i8086/reg1.c. Z8001 deltas: byte values may occupy only the
 * byte-addressable registers R0..R7 (mch.h BYTEREGS) instead of i8086's
 * AX/BX/CX/DX; and the Z8000 dynamic shift (SDA/SDL) takes its count in ANY
 * register, so the i8086 "keep CX free for the shift count" restriction is gone.
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * Select a register. 'tp' is the tree; 'c' is a context (PAIR or ANY*); 'flag'
 * is true if ANY should be resolved.
 */
regselect(tp, c, flag)
TREE	*tp;
{
	register REGDESC	*rp;
	register KIND		kind;
	register PREGSET	busy;
	register int		byte;
	register PERTYPE	*ptp;

	ptp = &pertype[tp->t_type];
	kind = ptp->p_pair;
	byte = bytereg(tp);
	if (c != PAIR) {
		if (flag == 0 && byte == 0)
			return (c);
		kind = ptp->p_kind;
	}
	busy = curbusy;
	/* RR0's exclusion from far-pointer ADDRESSING is a property of the role the
	 * register plays, not of the value's type: the Z8000 forbids R0/RR0 only as
	 * an indirect, base or index register (Z8000 CPU Technical Manual, 5.2 "Use
	 * of CPU Registers", p.5-2).  That role is exactly reg[].r_lvalue, where
	 * table1.c withholds KLP from RR0 -- so the ANYL arm below already refuses
	 * it, and no extra test belongs here.  A far pointer that is merely HELD or
	 * returned (r_rvalue KLP; RR0 is the long/pointer return pair) is legal and
	 * must stay legal, or every such value pays a needless copy. */
	for (rp = &reg[FRREG]; rp < &reg[NRREG]; ++rp) {
		if ((rp->r_phys & busy) != 0)
			continue;
		if (c == ANYL) {
			if ((rp->r_lvalue & kind) == 0)
				continue;
		} else {
			if ((rp->r_rvalue & kind) == 0)
				continue;
		}
		if (byte && (rp->r_phys & ~BYTEREGS) != 0)
			continue;
		return (rp - &reg[0]);
	}
	return (-1);
}

/*
 * Return true if register 'r' is a usable temporary for tree 'tp'.
 */
isusable(tp, c, r)
register TREE	*tp;
register int	r;
{
	register int		op;
	register PERTYPE	*ptp;
	register KIND		kind;
	register int		byte;

#if !TINY
	if (sflag > 2)
		snapf("Isusable(%P, %C, %R)? ", tp, c, r);
#endif
	op = tp->t_op;
	ptp = &pertype[tp->t_type];
	if ((op >= MUL && op <= REM) || (op >= AMUL && op <= AREM))
		kind = ptp->p_pair;
	else
		kind = ptp->p_kind;
	byte = bytereg(tp);
	if (c == MLVALUE) {
		if ((reg[r].r_lvalue & kind) == 0)
			goto no;
	} else {
		if ((reg[r].r_rvalue & kind) == 0)
			goto no;
	}
	if (byte && (reg[r].r_phys & ~BYTEREGS) != 0)
		goto no;
#if !TINY
	if (sflag > 2)
		snapf("yes\n");
#endif
	return (1);
no:
#if !TINY
	if (sflag > 2)
		snapf("no\n");
#endif
	return (0);
}

/*
 * Test if a byte register is needed (machine-independent).
 */
bytereg(tp)
register TREE *tp;
{
	register op;

	if (isbyte(tp->t_type))
		return (1);
	if ((op = tp->t_op) == LEAF || op == CONVERT || op == CAST)
		tp = tp->t_lp;
	if (isbyte(tp->t_type))
		return (1);
	/* A store/RMW into a byte lvalue needs the value temp in a byte-addressable
	 * register (R0..R7), because the byte rules dial out its low half via [LO R]:
	 * ASSIGN, the compound-assigns (AADD..ASHR), AND prefix/postfix ++/-- on a byte
	 * (INCBEF..DECAFT) -- aft.t/bef.t fetch the old byte with `ZLDB [LO R],[AL]'.
	 * Omitting the inc/dec ops let a word temp land in R8..R15, where lohalf() is -1
	 * and genadr read ramode[-1] (col.c's `*p++'-style byte postfix). */
	if ((op == ASSIGN || (op >= AADD && op <= ASHR) || (op >= INCBEF && op <= DECAFT))
	 && isbyte(tp->t_lp->t_type))
		return (1);
	return (0);
}

/* end of n1/z8001/reg1.c */
