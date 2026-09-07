/*
 * n1/z8001/gen2.c
 * External and static initializer generation. Segmented Z8001.
 * Machine-INDEPENDENT body (ported from i8086): emits ZBYTE/ZWORD/ZGPTR data
 * pseudo-ops + the DECVAX software-fp conversion (IEEE=0). Uses gencoll (gen1.c)
 * and the data pseudo-ops from pseudops.h.
 */

#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif
#include "pseudops.h"

static	char	ilinit[] = "initializer too complex";
static	dval_t	dvalzero;

/*
 * This routine handles blocks of bytes (IBLOCK) items.
 * It is passed the count.
 * If the "ZBYTE" could be a machine independent notion,
 * then this could be moved into "cc1.c".
 */
iblock(n)
register int	n;
{
	while (n--) {
		bput(CODE);
		bput(ZBYTE);
		iput(A_OFFS|A_DIR);
		iput(bget());
	}
}

/*
 * This routine compiles any "IEXPR" trees.
 * The "INIT" node has been pulled off by "work" (in "cc1.c")
 * and the tree has been run through the tree optimizer.
 */
iexpr(tp, tt)
register TREE	*tp;
{
	register char	*dcp;
	register int	op;
	register int	bytes;
	dval_t 		d;
	int		mode;
	lval_t		offs;
	int		lidn;
	SYM		*gidp;

	tp = basenode(tp);
	if (!isflt(tt)) {
		mode = A_DIR;
		offs = 0;
		if (gencoll(tp, &mode, &offs, &lidn, &gidp, 0, 1) == 0
		|| (mode&A_AMOD) != A_DIR) {
			cerror(ilinit);
			return;
		}
		mode &= ~A_PREFX;
		/*
		 * A long integer gets put out as a pair of DW opcodes.
		 * Big-endian Z8000: HIGH half first, then low (the i8086
		 * donor emitted low-first -- static long initializers were
		 * word-swapped in the data image; the float path below was
		 * already byte-order-fixed, this one was missed).
		 */
		if (tt==S32 || tt==U32) {
			if ((mode&(A_LID|A_GID)) != 0) {
				cerror(ilinit);
				return;
			}
			bput(CODE);
			bput(ZWORD);
			iput(A_OFFS|mode);
			iput((int)(offs >> 16));
			bput(CODE);
			bput(ZWORD);
			iput(A_OFFS|mode);
			iput((int)(offs&0xFFFFL));
			return;
		}
		bput(CODE);
		if (tt==LPTR || tt==LPTB) {
			if ((mode&(A_LID|A_GID)) == 0 && (int)(offs >> 16) != 0) {
				/* constant far pointer: SEGMENT word first (BE) */
				bput(ZWORD);
				iput(A_OFFS|mode);
				iput((int)(offs >> 16));
				bput(CODE);
				bput(ZWORD);
				iput(A_OFFS|mode);
				iput((int)(offs&0xFFFFL));
				return;
			}
			bput(ZGPTR);
		}
		else if (isbyte(tt))
			bput(ZBYTE);
		else
			bput(ZWORD);
		/*
		 * A symbol-relative offset is two words, high then low: the
		 * full signed 32-bit displacement from the symbol (matches
		 * genadr and getfield).  A symbol-less value is one word.
		 */
		if (offs == 0)
			iput(mode);
		else if ((mode&(A_LID|A_GID)) != 0) {
			iput(A_OFFS|mode);
			iput(upper(offs));
			iput(lower(offs));
		} else {
			iput(A_OFFS|mode);
			iput((int)  offs);
		}
		if ((mode&A_LID) != 0)
			iput(lidn);
		else if ((mode&A_GID) != 0)
			sput(gidp->s_id);
		return;
	}
	op = tp->t_op;
	if (op==ICON || op==LCON) {
		dcp = (char *) d;
		dvallval(dcp, grabnval(tp));
	} else if (op == DCON)
		dcp = (char *) tp->t_dval;
	else {
		cerror(ilinit);
		dcp = dvalzero;
	}
	bytes = 8;
	if (tt == F32) {
		bytes = 4;
#if IEEE
		fvaldval(dcp);
#endif
	}
	/* Big-endian Z8000: the float image is MSB-first (dcp[0] holds the exponent
	 * end), so emit the bytes FORWARD.  (The i8086 template emitted dcp[bytes-1..0]
	 * -- correct only for a little-endian target.) */
	for (op = 0; op < bytes; op++) {
		bput(CODE);
		bput(ZBYTE);
		iput(A_OFFS|A_DIR);
		iput(dcp[op]&0xFF);
	}
}

/*
 * Convert an "lval_t" (a 32 bit integer)
 * into either IEEE or DECVAX double precision float and
 * store it into the 8 byte character array to which "dcp" points.
 * The original pack is written as shifts
 * (rather than by a pun with a union) because the long
 * byte order is different on the PDP-11 and the 8086
 * and the code should work both places.
 * Assumes 32 bit long and 8 bit bytes.
 */
dvallval(dcp, lval)
char	*dcp;
lval_t	lval;
{
	register int	bexp, sign;
	register int	i;

	for (i=0; i<8; dcp[i++]=0)
		;
	if (lval != 0) {
		sign = 0;
		if (lval < 0) {
			lval = -lval;
			sign = 0200;
		}
		dcp[4] = lval >> 24;
		dcp[5] = lval >> 16;
		dcp[6] = lval >>  8;
		dcp[7] = lval;
#if IEEE
		bexp = 1023 + 52;
#else
		bexp =  128 + 56;
#endif
		do {
			--bexp;
			shleft(dcp);
#if IEEE
		} while ((dcp[1]&020) == 0);
		dcp[1] &= ~0360;
		dcp[1] |= (bexp<<4) & 0360;
		dcp[0]  = sign | ((bexp>>4)&0177);
#else
		} while ((dcp[1]&0200) == 0);
		dcp[1] &= ~0200;
		dcp[1] |= (bexp<<7) & 0200;
		dcp[0]  = sign | ((bexp>>1)&0177);
#endif
	}
}

/*
 * Shift a 64 bit integer (packed into 8 8 bit bytes)
 * one position to the left.
 * Shift a 0 into the empty bit position on the right.
 */
shleft(dcp)
register char *dcp;
{
	register int	icarry, ocarry;
	register int	i;

	ocarry = 0;
	for (i=7; i>=0; --i) {
		icarry = ocarry;
		ocarry = 0;
		if ((dcp[i]&0200) != 0)
			++ocarry;
		dcp[i] <<= 1;
		dcp[i] |=  icarry;
	}
}

#if IEEE
/*
 * Convert an IEEE double precision floating point number
 * (packed into 8 8 bit bytes) into a single precision IEEE floating
 * point number packed into the first 4 of the 8 bytes.
 * The exponant must be rebiased.
 */
fvaldval(dcp)
register char *dcp;
{
	register int	sign, bexp;

	sign = dcp[0]&0200;
	bexp = ((dcp[0]<<4)&03760) + ((dcp[1]>>4)&017);
	if (bexp != 0)
		bexp += 127 - 1023;
	shleft(dcp);
	shleft(dcp);
	shleft(dcp);
	dcp[1] &= ~0200;
	if ((bexp&01) != 0)
		dcp[1] |= 0200;
	dcp[0] = sign | ((bexp>>1)&0177);
}
#endif
