/*
 * Read the intermediate (i1) CODE stream into the MI's INS list.  getcode() is almost
 * machine-independent; getfield() (afield.c) is the only Z8001-specific reader.  Two
 * Z8001 i1 conventions are handled here (pseudops.h):
 *
 *   0xC0..0xC3  data directives (ZBYTE/ZWORD/ZLPTR/ZGPTR) -- these flow through the
 *               normal CODE path (opinfo op_naddr 1, OF_*_ data style drives emit).
 *   0xD0..0xDF  the conditional relative-jump band (ZJREL|cc) -- becomes a JUMP node
 *               whose i_rel carries the 4-bit Z8000 condition code.
 *
 * The one unconditional jump JP (style OF_JP) likewise becomes a JUMP node (cc =
 * always); the MI's span-dependent machinery relaxes it and emit puts out JR.
 */
#ifdef   vax
#include "INC$LIB:cc2.h"
#else
#include "cc2.h"
#endif

#define	CC_ALWAYS	8		/* Z8000 condition code T (always)	*/

/*
 * Build a span-dependent JUMP node for a branch with condition code `cc'.  cc1
 * emits the target as a label-direct address field (A_LID|A_DIR, labno) -- the same
 * shape the donor JUMP node consumes -- so read it as the mode word + label number.
 */
static INS *
jumpnode(cc)
{
	register INS	*ip;

	if (iget() != (A_LID|A_DIR))
		cbotch("jump target");
	ip = newi(sizeof(INS));
	ip->i_type = JUMP;
	ip->i_long = 0;
	ip->i_rel = cc;
	ip->i_fp = NULL;
	ip->i_bp = NULL;
	ip->i_labno = iget();
	ip->i_sp = NULL;
	ip->i_ip = NULL;
	return (ip);
}

/*
 * Read in a code item.
 * Return a pointer to a filled in INS node.
 */
INS *
getcode()
{
	register INS	*ip;
	register OPINFO	*opp;
	register int	i;
	register int	opcode;

	opcode = bget();
	if (opcode >= ZJREL && opcode <= (ZJREL|0xF))
		return (jumpnode(opcode & 0xF));	/* conditional JR cc */
	opp = &opinfo[opcode];
	if (opp->op_style == OF_JP)
		return (jumpnode(CC_ALWAYS));		/* JP -> relaxed relative jump */
	ip = newn(opp->op_naddr);
	ip->i_type = CODE;
	ip->i_fp = NULL;
	ip->i_bp = NULL;
	for (i=0; i<opp->op_naddr; ++i)
		getfield(opcode, &ip->i_af[i]);
	ip->i_op = opcode;
	ip->i_naddr = opp->op_naddr;
	return (ip);
}

/*
 * Read in an AUTOS item that appears in a function body.  All it does is read the
 * size of the stack frame into the global cell `framesize', used by genprolog to
 * put out the function prolog.  The register field (the second ival_t) is unused.
 */
getautos()
{
	framesize = iget();
	framemask = iget();	/* callee-saved register-variable mask (cc0) */
}
