/*
 * n1/z8001/amd.c
 * Set the addressing-mode tree flags (T_*) for a node. Segmented Z8001.
 * Template: n1/i8086/amd.c. The constant / half-constant / segment-class logic
 * is machine-independent and kept verbatim; the register-specific tests are
 * Z8001: the stack registers are FPREG(R13)/SPREG(R15), a register that makes a
 * tree addressable is a register PAIR (@RRn segmented indirect) or a non-R0 word
 * register (@Rn, near), and LDA computes an effective address from any base reg
 * R1..R15 (R0 cannot be an addressing base/index on the Z8000).
 *
 * The structure mirrors i8086; the exact addressing-mode acceptance is
 * validated against the decoder.
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

TREE	*findoffs();

/* a Z8001 register that can serve as an addressing base/index (not R0). */
#define	isbasereg(r)	((r) != R0 && (r) <= R15)
/* a register PAIR holds a segmented (far) pointer for @RRn indirect. */
#define	ispairreg(r)	((r) >= RR0 && (r) <= RR14)
/* a FAR (segmented seg:offset) pointer -- pointer + long(4-byte) class. */
#define	isfarptr(t)	(ispoint(t) && islong(t))

/*
 * Determine access mode; the mode set is stored into 't_flag'. Subtrees are
 * already marked. Usually called by 'walk'.
 */
amd(tp, ptp)
register TREE	*tp;
TREE		*ptp;
{
	register TREE	*xp;
	register ival_t	half;
	register int	op;
	register REGNAME r;
	register FLAG	flag;
	register FLAG	mask;
	register KIND	kind;
	register TYPE	t;
	register lval_t	n;
	register int	i;
	register int	seg;

	if ((op = tp->t_op) == LEAF) {
		tp->t_flag = tp->t_lp->t_flag;
		return;
	}
	flag = 0;
	if (op == ICON || op == LCON) {
		if ((n = grabnval(tp)) == 0)
			flag |= T_0;
		else if (n == 1)
			flag |= T_1;
		else if (n == 2)
			flag |= T_2;
		if (n >= -128 && n <= 127)
			flag |= T_BYTE;
		if (op == LCON) {
			flag |= T_LCN;
			half = lower(n);
			if (half == 0)
				flag |= T_LHC;
			if (half == -1)
				flag |= T_LHS;
			half = upper(n);
			if (half == 0)
				flag |= T_UHC;
			if (half == -1)
				flag |= T_UHS;
		} else
			flag |= T_ICN;
	}
	if (op == DCON) {
		flag |= T_DCN;
		i = 0;
		while (i < sizeof(dval_t) && tp->t_dval[i] == 0)
			++i;
		if (i == sizeof(dval_t))
			flag |= T_0;
	}
	if (op == REG) {
		flag |= T_DIR;
		kind = pertype[tp->t_type].p_kind;
		if ((reg[tp->t_reg].r_lvalue & kind) != 0)
			flag |= T_LREG;
		if ((reg[tp->t_reg].r_rvalue & kind) != 0)
			flag |= T_RREG;
#if !ONLYSMALL
		if (tp->t_reg == SPREG || tp->t_reg == FPREG)
			flag |= T_SREG;
#endif
		/* Only R0..R7 have byte halves.  A value in R8..R15 -- typically a
		 * `register' variable out of bind.c's callee-saved R6..R12 pool -- has
		 * none, so a byte rule dialing [LO] on it would index ramode[-1].
		 * Marked so leaves.t can steer
		 * those to the word-register narrow.  Real word registers only: the
		 * pseudo goals (ANYR/TEMP/...) also carry r_lohalf == -1. */
		if (tp->t_reg <= R15 && reg[tp->t_reg].r_lohalf < 0)
			flag |= T_NBH;
	} else if (op == ADDR) {
		if (isvariant(VSMALL))
			flag |= T_ADS;
		else {
			xp = tp->t_lp;
			if (xp->t_op == LID || xp->t_op == GID) {
				switch (xp->t_seg) {
				case SCODE:
				case SLINK:
					flag |= T_ACS;
					break;
				case SPURE:
					if (isvariant(VRAM))
						flag |= T_ADS;
					else
						flag |= T_ACS;
					break;
				case SSTRN:
					if (notvariant(VROM))
						flag |= T_ADS;
					else
						flag |= T_ACS;
					break;
				case SDATA:
				case SBSS:
				case SANY:	/* extern: DA operand with external reloc,
						 * same X-mode form as a defined static.
						 * The donor pools SANY GIDs instead, so its
						 * switch never sees one; the X-mode defer
						 * (mtree2.c) keeps ADDR(GID) live for SANY
						 * too, so it needs the T_ADS mark here or
						 * no table can address it (store() loops
						 * to the NSTORE botch). */
					flag |= T_ADS;
					break;
				}
			}
		}
	} else if (op == LID || op == GID) {
#if ONLYSMALL
		flag |= T_DIR;
#else
		seg = tp->t_seg;
		if (isvariant(VSMALL)
		|| (ptp != NULL && ptp->t_op == CALL && tp == ptp->t_lp)
		|| seg == SCODE
		|| seg == SLINK
		|| (seg == SPURE && notvariant(VRAM))
		|| ((ptp == NULL || ptp->t_op != ADDR)	/* a static DEREF is directly DA-addressable
					 * (return g / g=x), whatever its type: a pointer-typed
					 * GID names a pointer OBJECT, which loads and stores like
					 * any other scalar.  An array or function decays to
					 * ADDR(GID) -- an address VALUE -- and that keeps the
					 * pooled/LDA path. */
		    && (seg == SANY || seg == SDATA || seg == SBSS
		     || (seg == SPURE && isvariant(VRAM))
		     || (seg == SSTRN && notvariant(VROM)))))
			flag |= T_DIR;
#endif
	} else if ((op == ADD || op == SUB) && ispoint(t = tp->t_type)) {
		mask = T_CON;
		if (op == SUB)
			mask = T_NUM;
		xp = tp;
		do {
			if ((xp->t_rp->t_flag & mask) == 0)
				break;
			xp = xp->t_lp;
		} while (xp->t_op == ADD || xp->t_op == SUB);
		if ((xp->t_flag & T_REG) != 0) {
			while (xp->t_op == LEAF)
				xp = xp->t_lp;
			r = xp->t_reg;
#if !ONLYSMALL
			if (notvariant(VSMALL)) {
				if (r == FPREG)
					flag |= T_LSS;
			} else
#endif
				if (isbasereg(r))
					flag |= T_LEA;
		}
	} else if (op == STAR) {
		xp = findoffs(tp);
		if ((xp->t_flag & T_CON) != 0 && notvariant(VLARGE))
			flag |= T_DIR;
		else if ((xp->t_flag & T_REG) != 0) {
			while (xp->t_op == LEAF)
				xp = xp->t_lp;
			r = xp->t_reg;
#if !ONLYSMALL
			if (notvariant(VSMALL)) {
				if (r == FPREG || ispairreg(r))
					flag |= T_DIR;
			} else
#endif
				if (isbasereg(r))
					flag |= T_DIR;
		}
		flag |= T_OFS;
	}
	/* Preserve the modoper-set fold marker across amd's flag recompute, so it
	 * survives every pass and reaches findoffs (which runs before amd in modleaf). */
	tp->t_flag = flag | (tp->t_flag & T_FOLDOFS);
}

/*
 * A constant address offset is folded into a Z8000 displacement, which is 16 bits wide
 * and is added to the OFFSET word alone -- it cannot reach the SEGMENT.  So a `long'
 * constant whose upper half is neither 0 nor a sign extension names a SEGMENT, and
 * folding it would emit its low half only: `*(char *)(0x3a000000L + o)' became a bare
 * @RRn store into segment 0 with nothing left of the 0x3a, and no diagnostic.  Such a
 * constant must stay in the tree, where ADDL/SUBL adds all 32 bits into the pair.
 */
#define	segconst(f)	(((f) & T_LCN) != 0 && ((f) & (T_UHC|T_UHS)) == 0)

/*
 * Given an offset-type tree, run down the chain of indirections and additions
 * looking for the tree that must be loaded into a register to make an
 * addressable tree. Return a pointer to it. (Machine-independent.)
 */
TREE *
findoffs(tp)
register TREE *tp;
{
	register flag, ct = 0, op;
	register int fold;

	/* A far-pointer field deref that modoper marked T_FOLDOFS is a LOAD/STORE,
	 * which CAN address @RRn+disp -- fold its constant offset into the displacement
	 * (1 instruction shorter).  Without the mark a far base materializes (below), so
	 * an ALU/compare memory operand -- with no @RRn+disp form -- stays @RRn disp 0. */
	fold = (tp->t_flag & T_FOLDOFS) != 0;
	while (tp->t_op == LEAF)
		tp = tp->t_lp;
	for (;;) {
		tp = tp->t_lp;
		switch (op = tp->t_op) {
		case SUB:	/* can't load a negative address */
			if (tp->t_rp->t_flag & T_NUM) {
				if (segconst(tp->t_rp->t_flag))
					return tp;	/* segment, not a displacement */
				if ((isfarbase(tp) || frameindexed(tp)) && !fold)
					return tp;	/* far/frame-indexed: materialize (disp 0) */
				break;
			}
			return tp;
		case ADD:	/* add one address max */
			if ((flag = (tp->t_rp->t_flag)) & T_NUM) {
				/* A FAR (segmented) pointer's constant offset is a Z8000 BA
				 * displacement, which is LOAD/STORE-only.  Materialize it into
				 * the pair (deref @RRn disp 0, usable by every operator) UNLESS
				 * this deref is a marked load/store, which folds it into
				 * the @RRn+disp addressing.  Same for a frame-relative INDEXED
				 * base (`&local[i]', also a pair deref).  Near FP+const fields
				 * and static base+index (both X-mode) always fold. */
				if (segconst(flag))
					return tp;	/* segment, not a displacement */
				if ((isfarbase(tp) || frameindexed(tp)) && !fold)
					return tp;
				break;
			}
			else if (flag & T_CON && 0 == ct++)
				break;
			return tp;
		case LEAF:	/* DIVERGES from the donor (ONLYSMALL has no segmented
				 * pointers). A mid-walk LEAF wrapping an ADDRESS
				 * (T_LSS/T_LEA -- e.g. a struct field base &q+off) must
				 * be DESCENDED THROUGH to reach the FP/register base so
				 * the offset folds into the displacement. But a LEAF
				 * wrapping a MEMORY LVALUE (a far pointer to be loaded
				 * for an @RRn deref, flag T_DIR|T_OFS, NOT T_LSS/T_LEA)
				 * must be RETURNED wrapped, so iselect finds the
				 * LEAF-dispatched load pattern (else it can't load it ->
				 * store cascade). The donor only skips LEADING leaves. */
			if ((tp->t_flag & (T_LSS|T_LEA)) != 0)
				break;
			return tp;
		default:
			return tp;
		}
	}
}

/*
 * True if a subtree resolves to a FAR (segmented) pointer base -- a register PAIR,
 * or a pointer of far(long) type.  An ADD is commutative, so the pointer operand may
 * be on EITHER side (cc0 emits `a[i]' as `i*scale + a' with the base on the RIGHT);
 * a SUB keeps its base on the left.  Checked on the base, not on the node itself:
 * by selection time the address ADD has been retyped to OFFS (setofstype), so its
 * own t_type no longer distinguishes far from near.
 */
static
isfartree(tp)
register TREE *tp;
{
	for (;;) {
		switch (tp->t_op) {
		case LEAF:
			tp = tp->t_lp;
			break;
		case ADD:	/* base on either operand */
			return isfartree(tp->t_lp) || isfartree(tp->t_rp);
		case SUB:	/* base on the left (the right is the subtracted offset) */
			tp = tp->t_lp;
			break;
		case REG:
			return ispairreg(tp->t_reg);
		default:
			return isfarptr(tp->t_type);
		}
	}
}

isfarbase(tp)
register TREE *tp;
{
	return isfartree(tp->t_lp);		/* the address spine under the ADD/SUB */
}

/* True if an address spine reaches the frame pointer (R13). */
static
hasfp(tp)
register TREE *tp;
{
	for (;;) {
		switch (tp->t_op) {
		case LEAF:
			tp = tp->t_lp;
			break;
		case ADD:
			return hasfp(tp->t_lp) || hasfp(tp->t_rp);
		case SUB:
			tp = tp->t_lp;
			break;
		case REG:
			return tp->t_reg == FPREG;
		default:
			return 0;
		}
	}
}

/*
 * True if the base of address node `tp' (under a constant offset we are about to fold)
 * is an FP-relative address that ALSO carries a variable index -- i.e. the base is
 * itself an ADD/SUB over the frame pointer (e.g. `&local_array[i]').  Such a base is
 * materialized into a register PAIR (@RRn), which has NO displacement form for an
 * ALU/compare operand -- so the constant offset must be materialized too (deref @RRn
 * disp 0), exactly like a far pointer.  A bare `FP + const' field (no index) is X-mode
 * `const(FP)', and a STATIC base + index is X-mode `addr(Rindex)' -- both ALU-encodeable
 * with the offset as a displacement, so neither qualifies.
 */
frameindexed(tp)
register TREE *tp;
{
	tp = tp->t_lp;				/* base under the const offset */
	while (tp->t_op == LEAF)
		tp = tp->t_lp;
	return (tp->t_op == ADD || tp->t_op == SUB) && hasfp(tp);
}

/* end of n1/z8001/amd.c */
