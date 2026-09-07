/*
 * The emit-time listing: print the buffered function the way cc2 is about to encode it.
 * cc3 shows the i1 stream cc1 wrote; between the two, cc2's peephole rewrites the INS
 * list and genins/genprolog make encoding choices below it, so the two views disagree --
 * a folded PUSHL, a compare whose operand came back to a register, a frame reservation
 * that vanishes, a small load that becomes LDK.  This prints the later view, in cc3's
 * syntax, so one pattern reads against either.  Off unless $CC2LIST is set to something
 * other than 0; nothing in the pipeline reads it.
 */
#ifdef   vax
#include "INC$LIB:cc2.h"
#else
#include "cc2.h"
#endif

extern char	*opname[];

/*
 * A register operand names a pair when the opcode is a long one, matching how the
 * hardware reads the A_REGM nibble; a byte register is the grouped encoding, 0..7 RHn
 * and 8..15 RLn.
 */
static
lreg(regm, dword)
{
	printf(dword ? "rr%d" : "r%d", regm);
}

/*
 * Symbol and/or displacement part of an address: a bare number where there is no symbol,
 * else the name with a signed displacement appended.
 */
static
laddr(afp)
register AFIELD	*afp;
{
	register SYM	*sp;

	if ((sp = afp->a_sp) != NULL) {
		if ((sp->s_flag&S_LABNO) != 0)
			printf("L%d", sp->s_labno);
		else
			printf("%s", sp->s_id);
		if ((SIGNEDADDRESS)afp->a_value == 0)
			return;
		if ((SIGNEDADDRESS)afp->a_value > 0)
			putchar('+');
	}
	printf("%ld", (long)(SIGNEDADDRESS)afp->a_value);
}

static
lafield(afp, dword)
register AFIELD	*afp;
{
	register int	mode, regm;

	mode = afp->a_mode & A_AMOD;
	regm = afp->a_mode & A_REGM;
	switch (mode) {

	case A_WR:
		lreg(regm, dword);
		break;

	case A_BR:
		if (regm < 8)
			printf("rh%d", regm);
		else
			printf("rl%d", regm-8);
		break;

	case A_IMM:
	case A_IMML:
		putchar('$');
		laddr(afp);
		break;

	case A_IR:
		if (afp->a_value != 0)
			printf("rr%d(%ld)", regm, (long)(SIGNEDADDRESS)afp->a_value);
		else
			printf("@rr%d", regm);
		break;

	case A_DIR:
		if ((afp->a_mode&A_CS) != 0)
			printf("code:");
		laddr(afp);
		break;

	case A_X:
		laddr(afp);
		printf("(r%d)", regm);
		break;

	default:
		printf("?mode%d", mode>>4);
		break;
	}
}

/*
 * The Z8000 condition codes as as-z8001 spells them; the same table cc3 prints from.
 */
static	char	*lccnames[16] = {
	"",	"lt",	"le",	"ule",	"ov",	"mi",	"eq",	"ult",
	"un",	"ge",	"gt",	"ugt",	"nov",	"pl",	"ne",	"uge"
};

static
lcode(ip)
register INS	*ip;
{
	register OPINFO	*opp;
	register int	i, op;

	op = ip->i_op;
	opp = &opinfo[op];
	/* genins substitutes the one-word LDK below the INS list, so name the form it
	 * will actually encode, not the ZLD the list holds. */
	if (ldkform(ip))
		op = ZLDK;
	if (opname[op] != NULL)
		printf("\t%s", opname[op]);
	else
		printf("\top%d", op);
	for (i = 0; i < ip->i_naddr; ++i) {
		printf(i == 0 ? "\t" : ", ");
		lafield(&ip->i_af[i], (opp->op_flag&OP_DWORD) != 0);
	}
	putchar('\n');
}

/*
 * Whether a listing was asked for.  The switch is the environment and not one of
 * cc2's arguments: those belong to the driver, which never wants a listing, and the
 * argument that would have carried it is xflag's in a non-TINY build.  Asked once.
 */
listwanted()
{
	static int	state;
	extern char	*getenv();
	register char	*ep;

	if (state == 0)
		state = ((ep = getenv("CC2LIST")) != NULL && *ep != '0') ? 1 : 2;
	return (state == 1);
}

/*
 * Print the function currently in the INS list.  Called from genprolog, after the
 * optimizer loop and before the frame reservation is laid down.
 */
listing()
{
	register INS	*ip;
	register int	cc, res;

	if ((res = framebytes()) != 0)
		printf("\tsub\tr15, $%d\n", res);
	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		switch (ip->i_type) {
		case CODE:
			lcode(ip);
			break;
		case JUMP:
			cc = ip->i_rel & 0xF;
			printf("\tjr\t");
			if (lccnames[cc][0] != 0)
				printf("%s,", lccnames[cc]);
			printf("L%d\n", ip->i_labno);
			break;
		case LLABEL:
			printf("L%d:\n", ip->i_labno);
			break;
		case EPILOG:
			printf("\tepilog\n");
			break;
		}
	}
}

/* end of n2/z8001/listing.c */
