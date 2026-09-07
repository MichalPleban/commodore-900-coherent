/*
 * Read, write, and compare the address fields of the i1 stream.  The address-mode bit
 * layout (A_AMOD/A_REGM/A_PREFX/A_OFFS/A_LID/A_GID and the A_* mode values) is the
 * cc1->n2 i1 contract in h/mch.h.
 */
#ifdef   vax
#include "INC$LIB:cc2.h"
#else
#include "cc2.h"
#endif

/*
 * Read in an address field.  The style of the address is determined by flag bits
 * that hide in the address mode; those bits are cleared when the mode is stored.
 */
getfield(opcode, afp)
register AFIELD	*afp;
{
	register int	mode;

	mode = iget();
	afp->a_mode = mode & (A_PREFX|A_AMOD|A_REGM);
	afp->a_sp = NULL;
	afp->a_value = 0;
	if ((mode&A_AMOD) == A_IMML) {		/* 32-bit immediate: cc1 writes hi then lo */
		register unsigned int	hi, lo;
		hi = iget() & 0xFFFF;
		lo = iget() & 0xFFFF;
		afp->a_value = ((unsigned long)hi << 16) | lo;
		return;
	}
	/*
	 * A symbol-relative offset (A_LID or A_GID accompanies A_OFFS) is two
	 * words, high then low: the full signed 32-bit displacement from the
	 * symbol, covering byte offsets [0x8000, 0xFFFF] into a large object as
	 * well as negative address folds.  A bare offset (frame slot, immediate,
	 * register displacement) is one word, read signed.
	 */
	if ((mode&A_OFFS) != 0) {
		if ((mode&(A_LID|A_GID)) != 0) {
			register long	v;
			v  = (long)(iget() & 0xFFFF) << 16;
			v |= iget() & 0xFFFF;
			if ((v & 0x80000000L) != 0)
				v |= ~0xFFFFFFFFL;
			afp->a_value = v;
		} else
			afp->a_value = iget();
	}
	if ((mode&A_LID) != 0)
		afp->a_sp = llookup(iget(), 0);
	else if ((mode&A_GID) != 0) {
		sget(id, NCSYMB);
		afp->a_sp = glookup(id, 0);
	}
}

/*
 * Unassemble an address field, writing it back to the output file in intermediate
 * format (the VASM / re-emit path).
 */
genfield(afp, flag)
register AFIELD	*afp;
{
	register SYM	*sp;
	register int	mode;

	mode = afp->a_mode;
	if (afp->a_value != 0 || (mode&A_AMOD)==A_IMM)
		mode |= A_OFFS;
	if ((sp = afp->a_sp) != NULL) {
		if ((sp->s_flag&S_LABNO) != 0)
			mode |= A_LID;
		else
			mode |= A_GID;
	}
	iput(mode);
	/* A symbol-relative offset round-trips as two words, high then low. */
	if ((mode&A_OFFS) != 0) {
		if ((mode&(A_LID|A_GID)) != 0) {
			iput((ival_t)(afp->a_value >> 16));
			iput((ival_t)afp->a_value);
		} else
			iput((ival_t)afp->a_value);
	}
	if ((mode&A_LID) != 0)
		iput(sp->s_labno);
	else if ((mode&A_GID) != 0)
		sput(sp->s_id);
}

/*
 * Compare the address-field parts of two instructions.
 */
cmpfield(ip1, ip2)
INS	*ip1, *ip2;
{
	register AFIELD	*afp1, *afp2;
	register int	n;

	if ((n = ip1->i_naddr) != 0) {
		afp1 = &ip1->i_af[0];
		afp2 = &ip2->i_af[0];
		do {
			if (afp1->a_mode != afp2->a_mode
			||  afp1->a_sp != afp2->a_sp
			||  afp1->a_value != afp2->a_value)
				return (0);
			++afp1;
			++afp2;
		} while (--n);
	}
	return (1);
}
