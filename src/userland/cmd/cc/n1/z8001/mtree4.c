/*
 * n1/z8001/mtree4.c -- machine tree-modification helpers (machine-independent
 * apart from idiom comments; ported verbatim from i386).
 */

#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * This routine figures out the left subgoal context for AND.
 * The Z8000 AND always writes a register destination -- there is no
 * `AND mem,#imm' / `TEST mem,#imm' as on the 8086 -- so the left operand
 * of a mask test (if(x & BIT)) must be loaded into a register, never left
 * as an addressable memory operand.  Always compute the left as a value.
 */
getldown(tp, c)
TREE *tp;
{
	return MRVALUE;
}

/*
 * This routine figures out the right hand subgoal contexts
 * for the shift instruction.
 */
getrdown(tp, c)
TREE *tp;
{
	register int op;

	return ((op=tp->t_rp->t_op)==ICON || op==LCON) ? MRADDR : MRVALUE;
}

/*
 * Fix up the type of a reordered ADD node.
 * Watch for pointers.  When the child-derived type and the node's own type
 * are the same machine kind (and neither is a pointer), the node keeps its
 * own type: reordering an ADD cluster must not change the sum's signedness,
 * which selects the signed vs unsigned form in consumers (shift, divide,
 * modulus, compare).
 */
fixaddtype(tp)
TREE *tp;
{
	register tt;
	register lt, rt;

	lt = tp->t_lp->t_type;
	rt = tp->t_rp->t_type;
	if (ispoint(lt))
		tt = lt;
	else if (ispoint(rt))
		tt = rt;
	else if (islong(lt))
		tt = lt;
	else if (islong(rt))
		tt = rt;
	else
		tt = lt;
	if (!ispoint(tt) && !ispoint(tp->t_type)
	 && modkind(tp->t_type) == modkind(tt))
		tt = tp->t_type;
	tp->t_type = tt;
}

/*
 * Zap the type field of tree node 'tp'
 * into the type used for offset addressing trees.
 */
setofstype(tp)
TREE *tp;
{
	tp->t_type = OFFS;
}

/* end of n1/i386/mtree4.c */
