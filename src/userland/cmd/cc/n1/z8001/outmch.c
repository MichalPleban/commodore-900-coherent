/*
 * n1/z8001/outmch.c
 * Small machine-dependent output routines called from out.c -- mostly function
 * calls and argument lists. Segmented Z8001. Template: n1/i8086/outmch.c.
 * Z8001 deltas: NO segment-register (ES/DS) juggling for "alien" calls -- the
 * Z8000 carries the segment in the address, so the i8086 ES push/pop + ZLDES
 * mapping have no analog (alien calls fall back to cbotch for now, gated by the
 * VALIEN variant). The call is one opcode (ZCALL); n2 picks near/far from the
 * address operand. maptype maps a word opcode to its byte variant.
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * Output a call: push the args, emit the call opcode + target address, then pop
 * the args (caller-popped). The left subtree is the function;
 * if it is a STAR the call is indirect.
 */
outcall(tp, cxt, lab)
register TREE *tp;
{
	register int	nb;
	register int	iflag;

	if (isvariant(VALIEN)
	&&  tp->t_lp->t_op == GID && tp->t_lp->t_seg == SALIEN)
		cbotch("alien call (TODO)");	/* foreign-convention calls not yet supported */

	nb = outargs(tp->t_rp, 0);
	iflag = 0;
	tp = tp->t_lp;
	if (tp->t_op == STAR) {
		iflag = 1;
		tp = tp->t_lp;
		if (!isadr(tp->t_flag)) {
			while (tp->t_op == LEAF)
				tp = tp->t_lp;
			outofs(tp);
		}
	}
	outopcall(iflag);
	genadr(tp, iflag == 0, 0, NULL);
	if (nb != 0)
		/* INC R15,#nb is 2 bytes vs ADD's 4 for nb<=16 (8 word-args); the SP
		 * cleanup's flags are dead (C never reads carry off a stack adjust), so
		 * INC -- which doesn't touch carry -- is interchangeable here. */
		genri(nb <= 16 ? ZINC : ZADD, A_RSP, nb);
}

/*
 * Output the argument list, pushed right-to-left. Structure arguments (PTB
 * type) are copied onto the reserved stack slot by an inline LDIRB block move
 * (the Z8000 self-repeating byte move) -- no runtime helper. RR2=source far
 * pointer, RR4=destination (the reserved slot at the stack top), R0=byte count;
 * all three live in the caller-saved R0-R5 set, free during arg
 * marshalling.
 */
outargs(atp, isalien)
TREE	*atp;
{
	register TREE	*tp;
	register int	s, n;
	TYPE		type;

	if ((tp = atp) == NULL)
		return (0);
	if (tp->t_op == ARGLST) {
		s  = outargs(tp->t_rp, isalien);
		s += outargs(tp->t_lp, isalien);
		return (s);
	}
#if !ONLYSMALL
	if ((type = tp->t_type) == LPTB || type == SPTB) {
#else
	if ((type = tp->t_type) == SPTB) {
#endif
		n = s = tp->t_size;
		if (isvariant(VALIGN))
			n = (s+1) & ~01;
		if (n != 0) {
			genri(ZSUB, A_RSP, n);		/* reserve the slot; R15 -> dest */
			output(tp, MFNARG, 0, 0);	/* push the source far pointer */
			genrr(ZLDL, A_WR|R2, A_IR|R14);	/* RR2 = source (pop into a pair) */
			genri(ZINC, A_RSP, 4);		/* drop the pushed pointer (INC: 2B, flags dead) */
			genrr(ZLDL, A_WR|R4, A_WR|R14);	/* RR4 = dest = stack top */
			genri(ZLD, A_WR|R0, s);		/* R0 = byte count */
			bput(CODE); bput(ZLDIRB);	/* LDIRB @RR4,@RR2,R0 */
			iput(A_IR|R4); iput(A_IR|R2); iput(A_WR|R0);
		}
		return (n);
	}
	/*
	 * An argument already materialized in a REGISTER -- e.g. a far-pointer
	 * regvar (`f(p)' with p in a pair).  output()->outtree would deref the
	 * leaf's garbage t_lp, since there is no
	 * PFNARG rule that survives outtree for a bare REG.  Push the live
	 * register directly: a 4-byte type (far pointer / long) is a pair (PUSHL),
	 * a 2-byte type a word (PUSH).  genadr emits the pair via ramode[].
	 */
	if (tp->t_op == REG) {
		bput(CODE);
		bput(pertype[type].p_size > 2 ? ZPUSHL : ZPUSH);
		genadr(tp, 0, 0, NULL);
		return (pertype[type].p_size);
	}
	output(tp, MFNARG, 0, 0);
	return (pertype[type].p_size);
}

/*
 * The [TL OP], [TR OP], [TN OP] table macros: map a word opcode to its byte
 * variant when the relevant operand is a byte type.
 */
maptype(opvariant, opcode, tp)
int		opvariant;
register int	opcode;
register TREE	*tp;
{
	if (opvariant == M_TL)
		tp = tp->t_lp;
	else if (opvariant == M_TR)
		tp = tp->t_rp;
	else if (opvariant != M_TN)
		return (opcode);
	if (isbyte(tp->t_type)) {
		if (opcode == ZADD)  opcode = ZADDB;
		else if (opcode == ZSUB)  opcode = ZSUBB;
		else if (opcode == ZINC)  opcode = ZINCB;
		else if (opcode == ZDEC)  opcode = ZDECB;
		else if (opcode == ZAND)  opcode = ZANDB;
		else if (opcode == ZOR)   opcode = ZORB;
		else if (opcode == ZXOR)  opcode = ZXORB;
		else if (opcode == ZLD)   opcode = ZLDB;
		else if (opcode == ZCP)   opcode = ZCPB;
		else if (opcode == ZNEG)  opcode = ZNEGB;
		else if (opcode == ZCOM)  opcode = ZCOMB;
		else if (opcode == ZCLR)  opcode = ZCLRB;
		else if (opcode == ZTEST) opcode = ZTESTB;
	}
	return (opcode);
}

/*
 * Emit the call opcode. The Z8000 has one CALL (direct or register-indirect via
 * the address operand); n2 selects near CALR / far CALL from the address mode.
 */
outopcall(iflag)
{
	bput(CODE);
	bput(ZCALL);
}

/* end of n1/z8001/outmch.c */
