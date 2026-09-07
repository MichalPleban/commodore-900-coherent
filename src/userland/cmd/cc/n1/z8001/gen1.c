/*
 * n1/z8001/gen1.c
 * Address printing, function prologue/epilogue framing, switch compilation, and
 * the low-level instruction-emission primitives. Segmented Z8001.
 * Template: n1/i8086/gen1.c. Deltas vs i8086:
 *   - ramode[] is the Z8001 register file (R0..R15 word, RH/RL byte);
 *   - no x87 FPAC pseudo-register (that branch is dead, FPAC = -1);
 *   - conditional jumps are ONE opcode (ZJP) + a condition code, not a per-
 *     relation opcode (the optab relation rows hold CC_*);
 *   - gencoll's REG case: any base reg R1..R15 -> indexed mode A_X|reg (the
 *     Z8000 has no fixed BX/BP/SI/DI index registers);
 *   - genswitch is a correct if-chain for all n (the dense jump-table /
 *     table-search optimizations the i8086 used are not carried).
 * The emission primitives + prologue/epilogue framing are machine-independent
 * (they write CODE/AUTOS records into the n1->n2 IR; n2 emits real instructions).
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/* Machine-dependent allocation state (set up by routines here). Types match the
 * extern declarations in cc1.h (4.2.12 uses ival_t for the auto/temp counters). */
ival_t	maxauto;		/* Max autos in this function */
ival_t	basauto;		/* Block auto size as read from cc0, before any
				 * modify-phase temp raises maxauto over it */
int	mdtcoded;		/* Instructions emitted since the last modify-phase
				 * temp: that statement is coded, its temps dead */
ival_t	maxtemp;		/* Max temps in this function */
ival_t	curtemp;		/* Current temp */
PREGSET	regbusy;		/* Busy flags */
PREGSET	maxregbusy;		/* Union of regbusy over the whole function: the callee-saved
				 * registers cc2 (n2) must save/restore in the prolog/epilog. */

/*
 * Register -> addressing-mode table, indexed by 'real' register number
 * (mch.h). Word regs R0..R15 -> A_WR|n; byte regs RH0..RH7 -> A_BR|(0..7),
 * RL0..RL7 -> A_BR|(8..15) (the grouped Z8000 byte-register encoding). Pairs
 * and quads are 0 here -- genadr reaches their word halves via hihalf/lohalf.
 */
static	short	ramode[] = {
	A_WR|0,  A_WR|1,  A_WR|2,  A_WR|3,	/* R0..R3   */
	A_WR|4,  A_WR|5,  A_WR|6,  A_WR|7,	/* R4..R7   */
	A_WR|8,  A_WR|9,  A_WR|10, A_WR|11,	/* R8..R11  */
	A_WR|12, A_WR|13, A_WR|14, A_WR|15,	/* R12..R15 */
	/* RR0..RR14 (pairs): a long register is named by its EVEN word register,
	 * so the operand field carries R0,R2,...,R14 (= the pair's high half). */
	A_WR|0,  A_WR|2,  A_WR|4,  A_WR|6,
	A_WR|8,  A_WR|10, A_WR|12, A_WR|14,
	A_WR|0,  A_WR|4,  A_WR|8,  A_WR|12,	/* RQ0..RQ12 (quads)  */
	A_BR|0,  A_BR|1,  A_BR|2,  A_BR|3,	/* RH0..RH3 */
	A_BR|4,  A_BR|5,  A_BR|6,  A_BR|7,	/* RH4..RH7 */
	A_BR|8,  A_BR|9,  A_BR|10, A_BR|11,	/* RL0..RL3 */
	A_BR|12, A_BR|13, A_BR|14, A_BR|15	/* RL4..RL7 */
};

/*
 * Machine-dependent coder init: zero patcache[] entries that need a machine
 * variant the current build does not enable (Z8001: the Z8070 EPU patterns).
 */
coderinit()
{
	extern int patcsize;
	register int i;
	register PATFLAG *pfp;
	register PATFLAG pflag;

	for (pfp = patcache, i = 0; i < patcsize; pfp++, i++) {
		if (((pflag = *pfp) & MDPFLAGS) != 0)
			*pfp = (notvariant(VEPU) && ((pflag & PEPU) != 0))
				? 0 : (pflag & ~MDPFLAGS);
	}
}

/* Function prolog: clear the max auto / temp watermarks. */
doprolog()
{
	blkflab = 0;
	maxauto = 0;
	basauto = 0;
	mdtcoded = 1;
	maxtemp = 0;
	maxregbusy = 0;
}

/* Just before EPILOG: emit one AUTOS record telling cc2 the temp space and the
 * callee-saved register set used anywhere in the function (so cc2 can save/restore
 * it).  cc0's per-block cmask grows and shrinks; the UNION is what must be saved. */
doepilog()
{
	bput(AUTOS);
	iput(maxtemp);
	iput((ival_t)maxregbusy);
}

/* Read a new auto/register allocation item (auto size + register-busy mask). */
doautos()
{
	basauto = maxauto = iget();
	mdtcoded = 1;
	regbusy = iget();
	maxregbusy |= regbusy;
}

/* Unconditional jump. */
genubr(n)
{
	genl(ZJP, n);
}

/*
 * Conditional jump. The Z8000 has ONE conditional jump (ZJP) + a 4-bit cc;
 * the optab relation row holds the CC_* code. Emit ZJP, the cc as an immediate
 * operand, then the target label (n2 encodes JP cc,target).
 */
gencbr(c, n)
{
	/* optab[rel][0] is the per-condition relative-jump opcode ZJREL|cc; emit it
	 * with the target label, exactly like an unconditional jump. n2 maps the
	 * 0xD0..0xDF opcode range to JR cc. Matches the table [REL0] mechanism. */
	genl(optab[c-MIOBASE][0], n);
}

/*
 * Switch compilation. Correct for any n via an if-chain: compare the switch
 * value against each case and branch; fall through to the default.  The MI
 * (code.c) evaluates the switch expression into SWREG, so the comparisons MUST
 * use SWREG -- not a hard-wired register.  (The dense jump-table and
 * table-search strategies the i8086 used are not carried.)
 */
genswitch(def, n)
{
	register int i;
	register ival_t l;

	for (i = 0; i < n; ++i) {
		l = cases[i].c_val;
		if (l == 0)
			genrr(ZOR, A_WR|SWREG, A_WR|SWREG);	/* OR SWREG,SWREG -- Z if 0 */
		else
			genri(ZCP, A_WR|SWREG, l);		/* CP SWREG,#l */
		gencbr(EQ, cases[i].c_lab);
	}
	genubr(def);
}

/*
 * Dump a tree to stderr, under Z1DBG, for the "collect" botch below.
 *
 * This prints cc1's IN-MEMORY tree at the moment addressing-mode collection
 * fails, which is state no dump of the intermediate FILES can show: t_reg is an
 * allocation the selector just made, and t_flag/t_seg are annotations the
 * modify phase added.  cc3 prints the trees cc1 read and the CODE records it
 * wrote -- the two sides of this failure, not the failure itself -- and the
 * donor's own tool for the middle, snapf/-S, is compiled out (mch.h TINY=1)
 * until snapf is host-ported to stdarg.  So this stays, and it stays HERE:
 * addressing-mode collection is machine-dependent, and so is the diagnostic.
 */
static
dbgtree(tp, d)
TREE *tp;
{
	int i;

	if (tp == NULL)
		return;
	for (i = 0; i < d; i++)
		fprintf(stderr, "  ");
	fprintf(stderr, "op=%d type=%d size=%d flag=%x seg=%d ival=%ld reg=%d offs=%ld\n",
		tp->t_op, tp->t_type, tp->t_size, tp->t_flag, tp->t_seg,
		(long)tp->t_ival, tp->t_reg, (long)tp->t_offs);
	if (tp->t_op == LEAF || tp->t_op > 19) {
		dbgtree(tp->t_lp, d+1);
		if (tp->t_op != LEAF)
			dbgtree(tp->t_rp, d+1);
	}
}

/*
 * Output an address. 'tp' is the (address) tree; 'nsef' suppresses side-effect
 * escapes (LDA); 'pfx[npfx]' holds HI/LO half-selector prefixes.
 */
genadr(tp, nsef, npfx, pfx)
register TREE	*tp;
unsigned char	pfx[];
{
	register int	op;
	register int	bias;
	register int	memf;
	register int	byte;
	register int	reg;
	register ival_t	ival;
	int		mode;
	int		offs;
	lval_t		loffs;
	int		lidn;
	SYM		*gidp;

	static	char	basebias[] = {
		0,	0,		/* S8,   U8,    */
		1,	1,		/* S16,  U16,   */
		2,	2,		/* S32,  U32,   */
		2,	4,		/* F32,  F64,   */
		0,			/* BLK,         */
		0,	1,		/* FLD8, FLD16, */
		2,	2,		/* LPTR, LPTB,  */
		1,	1		/* SPTR, SPTB   */
	};

	mdtcoded = 1;
	while ((op = tp->t_op) == LEAF)
		tp = tp->t_lp;
	/* A register node: apply HI/LO half selectors, emit its addressing mode. */
	if (op == REG && (reg = tp->t_reg) != FPAC) {
		while (npfx--) {
			if (pfx[npfx] == M_LO)
				reg = lohalf(reg);
			else
				reg = hihalf(reg);
		}
		iput(ramode[reg]);
		return;
	}
	/* Constants/memory: HI/LO dial out the selected word; compute byte offset. */
	offs = 0;
	if (npfx) {
		memf = 0;
		if (op != ICON && op != LCON && op != DCON)
			++memf;
		bias = basebias[tp->t_type];
		while (npfx--) {
			byte = pfx[npfx];
			if (memf && bias == 2) {
				/* Z8000 is BIG-ENDIAN: a multi-word value in memory keeps
				 * its HIGH word at the base (lower address) and its LOW
				 * word at +2. (The i8086 donor was little-endian: HI@+2.) */
				if (byte == M_LO)
					offs += 2;
			} else if (byte == M_LO)
				offs += bias;
			bias >>= 1;
		}
	}
	if (op == DCON) {
		ival  = tp->t_dval[7-offs] & 0377;
		ival |= tp->t_dval[6-offs] << 8;
		iput(A_OFFS|A_IMM);
		iput(ival);
		return;
	}
	if (op == LCON) {
		if (npfx == 0) {	/* whole long: 32-bit immediate, high word then low */
			iput(A_OFFS|A_IMML);
			iput(upper(tp->t_lval));
			iput(lower(tp->t_lval));
			return;
		}
		ival = lower(tp->t_lval);
		if (offs == 0)
			ival = upper(tp->t_lval);
		iput(A_OFFS|A_IMM);
		iput(ival);
		return;
	}
	if (op == ICON) {
		ival = tp->t_ival;
		iput(A_OFFS|A_IMM);
		iput(ival);
		return;
	}
	/* General memory address: collect the addressing mode. */
	mode = A_DIR;
	loffs = offs;
	if (gencoll(tp, &mode, &loffs, &lidn, &gidp, 0, nsef) == 0) {
		if (getenv("Z1DBG")) {
			fprintf(stderr, "collect failed on:\n");
			dbgtree(tp, 0);
		}
		cbotch("collect");
	}
	if ((mode&A_AMOD) == A_IMM || nsef != 0)
		mode &= ~A_PREFX;
	/*
	 * A symbol-relative offset is two words, high then low: the full signed
	 * 32-bit displacement from the symbol (getfield reads it back the same
	 * way).  A bare offset (frame slot, register displacement) is one
	 * signed word.
	 */
	if ((mode&(A_LID|A_GID)) != 0) {
		if (loffs == 0)
			iput(mode);
		else {
			iput(mode|A_OFFS);
			iput(upper(loffs));
			iput(lower(loffs));
		}
	} else {
		offs = loffs;
		if (offs == 0)
			iput(mode);
		else {
			iput(mode|A_OFFS);
			iput(offs);
		}
	}
	if ((mode&A_LID) != 0)
		iput(lidn);
	else if ((mode&A_GID) != 0)
		sput(gidp->s_id);
}

/*
 * Walk an address tree, building up the addressing mode, offset and symbol base.
 * Caller sets *modep = A_DIR and *offsp = 0. 's' negates (under SUB); 'f' marks
 * a real indirection (STAR) so an address is not turned into an immediate.
 */
gencoll(tp, modep, offsp, lidnp, gidpp, s, f)
TREE	*tp;
int	*modep;
lval_t	*offsp;
int	*lidnp;
SYM	**gidpp;
{
	register int	op;
	register lval_t	offs;
	register int	seg;
	register int	r;

	while ((op = tp->t_op) == LEAF)
		tp = tp->t_lp;
	switch (op) {

	case ADDR:
		if (gencoll(tp->t_lp, modep, offsp, lidnp, gidpp, s, f) == 0)
			return (0);
		if (f == 0) {
			*modep &= ~A_AMOD;
			*modep |=  A_IMM;
		}
		break;

	case STAR:
		if (gencoll(tp->t_lp, modep, offsp, lidnp, gidpp, s, 1) == 0)
			return (0);
		break;

	case ADD:
	case SUB:
		if (gencoll(tp->t_lp, modep, offsp, lidnp, gidpp, s, f) == 0)
			return (0);
		if (op == SUB)
			s = !s;
		if (gencoll(tp->t_rp, modep, offsp, lidnp, gidpp, s, f) == 0)
			return (0);
		break;

	case ICON:
	case LCON:
		offs = grabnval(tp);
		if (s != 0)
			offs = -offs;
		*offsp += offs;
		break;

	case LID:
		if ((*modep&(A_GID|A_LID)) != 0 || s != 0)
			return (0);
		*modep |= A_LID;
		*lidnp  = tp->t_label;
		goto lidgid;

	case GID:
		if ((*modep&(A_GID|A_LID)) != 0 || s != 0)
			return (0);
		*modep |= A_GID;
		*gidpp  = tp->t_sp;
	lidgid:
		*offsp += tp->t_offs;
		/* Code-space symbols get the code-space flag (vs data space). */
		seg = tp->t_seg;
		if (seg == SCODE || seg == SLINK
		|| (isvariant(VLARGE)
		   && ((seg == SPURE && notvariant(VRAM))
		    || (seg == SSTRN && isvariant(VROM))))) {
			*modep &= ~A_PREFX;
			*modep |=  A_CS;
		}
		break;

	case REG:
		if ((*modep&A_AMOD) != A_DIR || s != 0)
			return (0);
		r = tp->t_reg;
		/* A far pointer held in a register PAIR (RR0..RR14): @RRn indirect
		 * (segmented seg:offset deref). The encoding names the pair by its
		 * even register = hihalf(r). */
		if (r >= RR0 && r <= RR14) {
			*modep = (*modep & ~A_AMOD) | A_IR | hihalf(r);
			break;
		}
		/* A single base register R1..R15 -> indexed addressing (R0 can't index). */
		if (r == R0 || r > R15)
			return (0);
		*modep = (*modep & ~A_AMOD) | A_X | r;
		break;

	default:
		return (0);
	}
	return (1);
}

/* ---- instruction-emission primitives (write CODE records to the IR) ---- */

/* op + one operand. */
genr(op, r)
{
	bput(CODE);
	bput(op);
	iput(r);
}

/* op + two operands. */
genrr(op, r1, r2)
{
	bput(CODE);
	bput(op);
	iput(r1);
	iput(r2);
}

/* op + register + immediate. */
genri(op, r, i)
{
	bput(CODE);
	bput(op);
	iput(r);
	iput(A_OFFS|A_IMM);
	iput(i);
}

/* op + local label. */
genl(op, l)
{
	bput(CODE);
	bput(op);
	iput(A_LID|A_DIR);
	iput(l);
}

/* op + global identifier. */
geng(op, g)
char	*g;
{
	bput(CODE);
	bput(op);
	iput(A_GID|A_DIR);
	sput(g);
}

/* op + immediate. */
geni(op, i)
{
	bput(CODE);
	bput(op);
	iput(A_OFFS|A_IMM);
	iput(i);
}

/* end of n1/z8001/gen1.c */
