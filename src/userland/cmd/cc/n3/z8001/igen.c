/*
 * n3/z8001/igen.c
 * C compiler - intermediate file printer.
 *	machine and assembly format dependent output routines.  Segmented Z8001.
 * Template: n3/i8086/igen.c.
 */
#ifdef vax
#include "INC$LIB:cc3.h"
#else
#include "cc3.h"
#endif

#define	NSEGDIR	6		/* segments with a directive: SCODE..SBSS */

/*
 * How to enter or leave a segment.  These are the names the ORIGINAL Z8001
 * cc3 printed (they are in its string table); as-z8001 has no segment
 * directives of its own -- cc2 writes the object directly -- so nothing
 * consumes them but a reader.
 */
char	*seg_enter[NSEGDIR] = {
	".shri",	/* SCODE */
	".link",	/* SLINK */
	".shrd",	/* SPURE */
	".strn",	/* SSTRN */
	".prvd",	/* SDATA */
	".bssd"		/* SBSS  */
};

char	*seg_leave[NSEGDIR] = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

/*
 * Process AUTOS items.
 * The opcode byte has been read.  cc1 writes two values (n1/z8001/gen1.c):
 * the frame size, and the mask of callee-saved registers cc0 handed out as
 * register variables.  Bit n of the mask is word register Rn.
 */
genautos()
{
	register int	numauto;
	register int	regmask;
	register int	reg;
	register int	sepchar;

	numauto = iget();
	regmask = iget();
	fprintf(ofp, "\tautos\t%d", numauto);
	sepchar = '\t';
	for (reg=R0; reg<=R15; ++reg) {
		if ((regmask&01) != 0) {
			fprintf(ofp, "%c%s", sepchar, regnames[reg]);
			sepchar = ' ';
		}
		regmask >>= 1;
	}
	fprintf(ofp, "\n");
}

/*
 * Output a double value.
 */
gendval(dp)
register dval_t	dp;
{
	register int i;

	for (i = 0; i < sizeof(dval_t); i += 1)
		fprintf(ofp, "%02x", dp[i] & 0377);
}

/*
 * Generate a machine dependent leaf node.
 * The Z8001 back end has none (common/z8001/mdlnam.c is empty), so reaching
 * here means the tree came from another machine's front end.
 */
genmdl(op)
{
	cbotch("bad mdl: %d", op);
}

/*
 * Generate a machine dependent operator node.
 */
genmdo(op)
{
	cbotch("bad mdo: %d", op);
}

/*
 * Generate a .comm record.
 * The operands must be read.
 */
gencomm()
{
	sget(id, NCSYMB);
	if (isvariant(VASM))
		fprintf(ofp, "\t.comm\t");
	else
		fprintf(ofp, "common\t");
	fprintf(ofp, "%s\t%ld\n", id, (long)zget());
}

/*
 * Generate an assembly operator involving a name.
 */
genname(op, id)
char *id;
{
	switch (op) {
	case FNAME:
		fprintf(ofp, "%s\tfile name %s\n", CMTSTR, id);
		break;
	case MNAME:
		fprintf(ofp, "%s\tmodule name %s\n", CMTSTR, id);
		break;
	case GLABEL:
		fprintf(ofp, "\t.globl %s\n%s:\n", id, id);
		break;
	case SLABEL:
		fprintf(ofp, "%s:\n", id);
		break;
	case UREFER:
		fprintf(ofp, "%s\tundefined reference %s\n", CMTSTR, id);
		break;
	default:
		cbotch("genname: bad op: %d", op);
	}
}

/*
 * Generate an assembly operator involving an integer value.
 */
genival(op, i)
long i;
{
	register char *s;

	switch (op) {
	case LINE:
		fprintf(ofp, "%s\tline number %ld\n", CMTSTR, i);
		break;
	case BLOCK:
		fprintf(ofp, "\t.blkb\t0x%lx\n", i);
		break;
	case ALIGN:
		fprintf(ofp, "\t.even\n");
		break;
	case ENTER:
		/* Only the six loadable segments have a directive; SANY and up
		 * (ops.h) are residence flags that never reach an ENTER, and
		 * indexing the table with one would read off its end. */
		if (dotseg >= 0 && dotseg < NSEGDIR
		 && (s=seg_leave[dotseg]) != NULL)
			fprintf(ofp, "\n\t%s\n\n", s);
		dotseg = i;
		if (dotseg >= 0 && dotseg < NSEGDIR
		 && (s=seg_enter[dotseg]) != NULL)
			fprintf(ofp, "\n\t%s\n\n", s);
		break;
	case LLABEL:
		fprintf(ofp, "L%ld:\n", i);
		break;
	default:
		cbotch("genival: bad op: %d", op);
	}
}

/* end of n3/z8001/igen.c */
