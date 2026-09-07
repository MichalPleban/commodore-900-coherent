/*
 * n1/z8001/altemp.c -- allocate a stack temporary as *(FP - n).
 * Z8001: frame pointer is R13 (FPREG); pointer type is the segmented LPTR.
 */

#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

extern ival_t maxtemp;
extern ival_t basauto;
extern int mdtcoded;

/*
 * Allocate a new temporary and return a pointer to a TREE node
 * which describes it: "*(%FPREG - n)".
 * The type is set from the type of the node that is going to be stored.
 * The "flag" is true if the node needs to be allocated.
 */
TREE *
tempnode(tp, flag) register TREE *tp; int flag;
{
	register TREE	*tp1;

	if (flag != 0) {
		curtemp += mapssize(pertype[tp->t_type].p_size);
		if (curtemp > maxtemp)		/* reserve here: a modify-phase temp
						 * may see no later code1() watermark */
			maxtemp = curtemp;
	}
	tp1 = makenode(REG, iptrtype());
	tp1->t_reg = FPREG;
	tp1 = leafnode(tp1);
	tp1 = leftnode(SUB, tp1, iptrtype(), 0);
	tp1->t_rp = ivalnode((ival_t)curtemp);
	tp1 = leftnode(STAR, tp1, tp->t_type, tp->t_size);
	return tp1;
}

/*
 * Allocate a temporary from the tree-modify phase (modoper and friends).
 * Such a temporary is live across code()'s per-statement restart of
 * curtemp at maxauto, so raise maxauto over it: selection temporaries
 * then allocate beyond it instead of on top of it.  mdtcoded (set by
 * instruction emission, cleared here) marks that the previous statement
 * has been coded, so its temporaries are dead and allocation restarts
 * at the block's own auto size; within one statement they stack.
 */
TREE *
mdtempnode(tp) register TREE *tp;
{
	if (mdtcoded) {
		curtemp = basauto;
		mdtcoded = 0;
	} else if (curtemp < maxauto)
		curtemp = maxauto;
	tp = tempnode(tp, 1);
	maxauto = curtemp;
	return tp;
}

/* end of n1/i386/altemp.c */
