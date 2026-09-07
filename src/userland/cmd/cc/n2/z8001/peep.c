/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Peephole optimizer.  Walk the code list, tracking the state of the machine registers,
 * and delete or simplify instructions that do not change that state.  subdec narrows a
 * small-constant ADD/SUB into a one-word INC/DEC; shldouble turns a short shift left
 * into a chain of register doublings; callreloc retargets the
 * pointer load feeding an indirect call; and the register-state pass propagates copies,
 * deletes redundant loads/copies, and substitutes a held memory operand by its register.
 */
#ifdef   vax
#include "INC$LIB:cc2.h"
#else
#include "cc2.h"
#endif

/* a Z8000 condition code (a JR/JP i_rel) that READS carry: ULE=3, C/ULT=7, UGT=0xB,
 * NC/UGE=0xF -- the unsigned-compare branches. */
static
carrycc(cc)
{
	return (cc == 3 || cc == 7 || cc == 0xB || cc == 0xF);
}

/* an op that READS the carry flag (so an earlier ADD/SUB carry is still live). */
static
carryreader(op)
{
	switch (op) {
	case ZADC: case ZADCB: case ZSBC: case ZSBCB:
	case ZRLC: case ZRLCB: case ZRRC: case ZRRCB:
		return (1);
	}
	return (0);
}

/* an op that CLOBBERS carry before any read (so an earlier carry is dead). */
static
carryclobber(op)
{
	switch (op) {
	case ZADD: case ZADDB: case ZADDL: case ZSUB: case ZSUBB: case ZSUBL:
	case ZCP: case ZCPB: case ZCPL: case ZINC: case ZINCB: case ZDEC: case ZDECB:
	case ZNEG: case ZNEGB: case ZMULT: case ZMULTL: case ZDIV: case ZDIVL:
	case ZSLL: case ZSLLB: case ZSLLL: case ZSLA: case ZSLAB: case ZSLAL:
	case ZSDL: case ZSDLB: case ZSDLL: case ZSDA: case ZSDAB: case ZSDAL:
		return (1);
	}
	return (0);
}

/*
 * Is the carry result of instruction `ip' dead?  Scan forward in emit order: a carry
 * reader keeps it live (unsafe); a carry clobber, a CALL, or end-of-function makes it dead
 * (safe).  Loads/logical/bit/exts leave carry untouched -- keep scanning.
 */
static
carrydead(ip)
register INS	*ip;
{
	register INS	*p;

	for (p = ip->i_fp; p != &ins; p = p->i_fp) {
		if (p->i_type == JUMP) {
			if (carrycc(p->i_rel & 0xF))
				return (0);		/* an unsigned-compare branch reads carry */
			continue;
		}
		if (p->i_type != CODE)
			continue;			/* LLABEL etc: carry passes through */
		if (p->i_op == ZCALL)
			return (1);			/* the callee clobbers flags */
		if (carryreader(p->i_op))
			return (0);
		if (carryclobber(p->i_op))
			return (1);
		/* load/logical/bit/exts: carry untouched -- keep scanning */
	}
	return (1);				/* fell off the end -- carry dead */
}

/*
 * ADD/SUB Rd,#1..16 -> INC/DEC Rd,#k.  INC/DEC are one word vs ADD/SUB-immediate's two;
 * the original MWC backend uses them for every small-constant bump.  Convert only a
 * word/byte register destination with a plain 1..16 immediate whose carry is dead.
 */
static
subdec()
{
	register INS	*ip;
	register AFIELD	*d, *s;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_naddr != 2)
			continue;
		if (ip->i_op != ZADD && ip->i_op != ZSUB)
			continue;
		d = &ip->i_af[0];
		s = &ip->i_af[1];
		if ((d->a_mode&A_AMOD) != A_WR && (d->a_mode&A_AMOD) != A_BR)
			continue;
		if ((s->a_mode&A_AMOD) != A_IMM || s->a_sp != NULL)
			continue;
		if (s->a_value < 1 || s->a_value > 16)
			continue;
		if (!carrydead(ip))
			continue;
		ip->i_op = (ip->i_op == ZADD) ? ZINC : ZDEC;	/* genins OF_INCDEC: count-1 nibble */
		++changes;
	}
}

/* incexpand: the inverse of subdec.  The cc1 pointer ++/-- tables (bef.t/aft.t
 * [OP1]) emit INC/DEC directly, stepping a far pointer's offset by the element
 * SIZE.  The Z8000 INC/DEC count field is only 4 bits (1..16), so a step > 16
 * (a pointer to a struct/array bigger than 16 bytes) would silently truncate --
 * genins OF_INCDEC masks count-1 into the nibble, e.g. `vp++' on a 20-byte
 * element becomes INC #4.  Widen any such INC/DEC back to a full
 * ADD/SUB, which carries the whole 16-bit immediate.  Runs before subdec, which
 * only ever narrows 1..16 and so leaves these >16 adds alone.  A count of 1..16
 * always fits, so the common scalar/small-element ++ keeps its one-word INC. */
static
incexpand()
{
	register INS	*ip;
	register AFIELD	*s;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_naddr != 2)
			continue;
		if (ip->i_op != ZINC && ip->i_op != ZDEC
		 && ip->i_op != ZINCB && ip->i_op != ZDECB)
			continue;
		s = &ip->i_af[1];
		if ((s->a_mode&A_AMOD) != A_IMM || s->a_sp != NULL)
			continue;
		if (s->a_value >= 1 && s->a_value <= 16)
			continue;			/* fits the 4-bit INC/DEC count */
		switch (ip->i_op) {
		case ZINC:	ip->i_op = ZADD;  break;
		case ZINCB:	ip->i_op = ZADDB; break;
		case ZDEC:	ip->i_op = ZSUB;  break;
		case ZDECB:	ip->i_op = ZSUBB; break;
		}
		++changes;
	}
}

/* a Z8000 condition code (a JR/JP i_rel) that READS the overflow flag: LT=1, LE=2,
 * OV/PE=4, GE=9, GT=0xA, NOV/PO=0xC -- the signed-compare and overflow branches. */
static
overflowcc(cc)
{
	return (cc == 1 || cc == 2 || cc == 4 || cc == 9 || cc == 0xA || cc == 0xC);
}

/* an op that DEFINES the overflow flag (so an earlier V is dead).  The arithmetic
 * shifts appear (a right shift is one with a negative count, so SRA is ZSLA here);
 * the logical shifts
 * are absent on purpose: the manual gives SLL/SRL/SDL "V: Undefined", so they neither
 * define V nor can be relied on to preserve it.  The list is deliberately short --
 * omitting a definer only keeps the scan going, which refuses a substitution rather
 * than making an unsafe one. */
static
overflowclobber(op)
{
	switch (op) {
	case ZADD: case ZADDB: case ZADDL: case ZSUB: case ZSUBB: case ZSUBL:
	case ZADC: case ZADCB: case ZSBC: case ZSBCB:
	case ZCP: case ZCPB: case ZCPL: case ZINC: case ZINCB: case ZDEC: case ZDECB:
	case ZNEG: case ZNEGB: case ZMULT: case ZMULTL: case ZDIV: case ZDIVL:
	case ZSLA: case ZSLAB: case ZSLAL: case ZSDA: case ZSDAB: case ZSDAL:
		return (1);
	}
	return (0);
}

/*
 * Is the overflow result of instruction `ip' dead?  The same forward scan carrydead()
 * makes, over the V flag: a signed branch keeps it live (unsafe); an op that defines V,
 * a RESFLG/SETFLG naming V, a CALL, or end-of-function makes it dead (safe).
 */
static
overflowdead(ip)
register INS	*ip;
{
	register INS	*p;

	for (p = ip->i_fp; p != &ins; p = p->i_fp) {
		if (p->i_type == JUMP) {
			if (overflowcc(p->i_rel & 0xF))
				return (0);
			continue;
		}
		if (p->i_type != CODE)
			continue;
		if (p->i_op == ZCALL)
			return (1);			/* the callee clobbers flags */
		if ((p->i_op == ZRESFLG || p->i_op == ZSETFLG) && p->i_naddr == 1
		 && (p->i_af[0].a_mode&A_AMOD) == A_IMM && (p->i_af[0].a_value & 1))
			return (1);			/* mask bit 0 is V: written outright */
		if (overflowclobber(p->i_op))
			return (1);
	}
	return (1);
}

/*
 * SLL/SLLB/SLLL Rd,#n -> a chain of n `ADD/ADDB/ADDL Rd,Rd' doublings.  A Z8000 shift
 * is a two-word instruction costing 13 + 3n cycles whatever the count, while a doubling
 * is one word and 4 cycles (8 for the long).  So the shift is 4 bytes and 16, 19, 22
 * cycles for n = 1, 2, 3 against 2, 4, 6 bytes and 4, 8, 12 for the word chain.  SHLMAXW
 * and SHLMAXL below say how far the chain is allowed to run.
 *
 * Three of the four flags a chain leaves are the flags the shift would have left.
 * Carry: the manual defines SLL's carry as "the last bit shifted from the destination",
 * which after n shifts is the original bit at msb-(n-1); the chain's INTERMEDIATE
 * carries are dropped and the surviving one is the carry out of the last doubling, the
 * msb of 2^(n-1) x, the same original bit.  S and Z follow the final result on both
 * sides.  D and H are unaffected by SLL and by ADD/ADDL, and ADDB writes them but only
 * DAB reads them, which cc1 never emits.
 *
 * V is where they part.  The manual gives SLL "V: Undefined" -- it is SLA, the
 * arithmetic shift, that defines V as the sign changing during the shift -- while ADD
 * defines V as signed overflow, which a doubling reports for exactly the values whose
 * top bit the shift discards.  cc1 takes a signed test straight off a shift's flags
 * with no intervening compare (`x << 2 < 0' emits the shift and a JR GE, and GE on the
 * Z8000 is S xor V), so the difference is reachable.  overflowdead() above is the
 * guard: a shift whose V a signed branch goes on to read keeps its shift.
 *
 * Two is where the chain stops.  At one and two it costs nothing: 2 and 4 bytes against
 * the shift's 4, for 12 and 11 fewer cycles, so it can never be a regression at any site
 * and needs no knowledge of which sites are hot.  From three on it is a trade -- two more
 * bytes for a shrinking cycle return, 10 at three, 9 at four, 1 at twelve, none at
 * thirteen -- and the bytes are spent at every site while the cycles are only collected
 * at the ones that run.  Declining it leaves the shift, which is the size and the speed
 * of the SLA the original backend emits for the same source, so the floor still holds.
 * The long stops at two for a harder reason than a trade: ADDL is 8 cycles, so three
 * doublings are 6 bytes and 24 cycles against SLLL's 4 and 22 -- bigger AND slower.
 */
#define	SHLMAXW	2	/* SLL/SLLB: ADD is 4 cycles; a chain of 2 is free		*/
#define	SHLMAXL	2	/* SLLL: ADDL is 8, so 3 doublings cost 24 against SLLL's 22	*/

static
shldouble()
{
	register INS	*ip, *np;
	register AFIELD	*d, *s;
	int		aop, max, n;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_naddr != 2)
			continue;
		switch (ip->i_op) {
		case ZSLL:	aop = ZADD;  max = SHLMAXW; break;
		case ZSLLB:	aop = ZADDB; max = SHLMAXW; break;
		case ZSLLL:	aop = ZADDL; max = SHLMAXL; break;
		default:	continue;
		}
		d = &ip->i_af[0];
		s = &ip->i_af[1];
		if ((d->a_mode&A_AMOD) != A_WR && (d->a_mode&A_AMOD) != A_BR)
			continue;
		if ((s->a_mode&A_AMOD) != A_IMM || s->a_sp != NULL)
			continue;
		if (s->a_value < 1 || s->a_value > max)
			continue;		/* 0 is a no-op, negative is a right shift */
		if (!overflowdead(ip))
			continue;
		n = s->a_value;
		ip->i_op = aop;			/* the shift itself becomes the first doubling */
		s->a_mode = d->a_mode;
		s->a_value = 0;
		while (--n > 0) {
			np = newn(2);
			np->i_type = CODE;
			np->i_op = aop;
			np->i_naddr = 2;
			np->i_af[0] = *d;
			np->i_af[1] = *d;
			np->i_fp = ip->i_fp;
			np->i_bp = ip;
			ip->i_fp->i_bp = np;
			ip->i_fp = np;
			ip = np;
			d = &ip->i_af[0];
		}
		++changes;
	}
}

/* ----------------------------------------------------------------------------
 * Forward register-state copy propagation (transform logic ported from the Go n2's
 * peephole; structured on fixed arrays + a linear per-basic-block scan the way the donor
 * i8086 peep.c is, so it runs on the target).  For each register, holds[] names the stable
 * memory (a frame slot off(R13) or a global) that register currently contains, and copyOf[]
 * records a live register-pair copy.  State resets at every label/jump/barrier (no CFG or
 * alias analysis); a memory store forgets ALL held memory.  Transforms:
 *   - a redundant load  `LD Rd,M' where Rd already holds M           -> delete
 *   - a redundant copy  `LDL RRd,RRs' where RRd already equals RRs    -> delete
 *   - simpoper: `OP Rd,M' (ADD/SUB/AND/OR/XOR/CP) where a register holds M -> read the reg
 * ---------------------------------------------------------------------------- */
#define	NMREG	16

/* what register r currently holds: kind 0 = nothing, 1 = frame slot off(R13), 2 = global.
 * wid is the WIDTH the value was moved at (1 = byte, 2 = word, 4 = long); holdfits() below
 * says which widths may then serve a given operand. */
static struct hold { char kind; char wid; SYM *sp; long off; }	holds[NMREG];
static short	copyOf[NMREG];		/* r is a live copy of register copyOf[r] (-1 = none)	*/
static char	pairHold[NMREG];	/* holds[r] came from a long (pair) load			*/

static
resetstate()
{
	register int	r;

	for (r = 0; r < NMREG; ++r) {
		holds[r].kind = 0;
		copyOf[r] = -1;
		pairHold[r] = 0;
	}
}

/* a barrier op (in a CODE record) forgets all state: calls and block moves. */
static
barrierop(op)
{
	switch (op) {
	case ZCALL: case ZCALR: case ZLDIR: case ZLDIRB: case ZLDDR: case ZLDDRB:
		return (1);
	}
	return (0);
}

/* a compare/test op: reads its operands, writes no register. */
static
cmpop(op)
{
	switch (op) {
	case ZCP: case ZCPB: case ZCPL: case ZTEST: case ZTESTB: case ZTESTL:
	case ZBIT: case ZBITB:
		return (1);
	}
	return (0);
}

/* a two-operand word op whose source (op#1) is read-only, so a held memory source may be
 * substituted by the holding word register. */
static
simpoperop(op)
{
	switch (op) {
	case ZADD: case ZSUB: case ZAND: case ZOR: case ZXOR: case ZCP:
		return (1);
	}
	return (0);
}

/* a load whose destination is written without being read. */
static
loadop(op)
{
	return (op == ZLD || op == ZLDB || op == ZLDL);
}

static
isbyteop(op)
{
	return ((opinfo[op].op_flag & OP_BYTE) != 0);
}

static
isdword(op)
{
	return ((opinfo[op].op_flag & OP_DWORD) != 0);
}

/*
 * Does afp name a stable memory location -- a frame slot off(R13) or a global (direct)?
 * Fill *h and return 1; else 0.  Far derefs, non-FP indexes and immediates are not stable.
 */
static
memkey(afp, h)
register AFIELD		*afp;
register struct hold	*h;
{
	switch (afp->a_mode & A_AMOD) {
	case A_X:
		if ((afp->a_mode & A_REGM) == 13) {	/* off(R13): a frame slot */
			h->kind = 1;
			h->sp = NULL;
			h->off = afp->a_value;
			return (1);
		}
		break;
	case A_DIR:
		h->kind = 2;
		h->sp = afp->a_sp;
		h->off = afp->a_value;
		return (1);
	}
	return (0);
}

/*
 * May a register that holds its value at width h->wid serve an operand read at width wid?
 * Same width always.  A PAIR (wid 4) load or store also leaves the FIRST register holding
 * the WORD at that address -- the Z8000 is big-endian, so the high word comes first -- and
 * holds[] never claims the second register, so a word read of the same address may take it.
 * The converse does not hold: one word does not supply a whole pair.  A BYTE mixes with
 * NEITHER, and that is the case this predicate exists for: the byte at M is the HIGH half
 * of the word at M, whereas `LD Rd,M' leaves M's LOW byte (the byte at M+1) in RLd.  Without
 * this, `LD R0,M ; LDB RL0,M' had the LDB deleted as a redundant reload and RL0 kept the
 * wrong half of the word.
 */
static
holdfits(h, wid)
register struct hold	*h;
{
	if (h->wid == wid)
		return (1);
	return (wid == 2 && h->wid == 4);
}

static
samehold(a, b)
register struct hold	*a, *b;
{
	return (a->kind && a->kind == b->kind && holdfits(a, (int)b->wid)
		&& a->sp == b->sp && a->off == b->off);
}

/* the operand width an op moves/reads, in bytes: 1 = byte, 4 = long pair, else 2 = word. */
static
opwidth(op)
{
	if (isbyteop(op))
		return (1);
	if (isdword(op))
		return (4);
	return (2);
}

/* do afields a and b name the identical memory operand (same mode, register, offset, symbol)? */
static
sameaddr(a, b)
register AFIELD	*a, *b;
{
	if ((a->a_mode&A_AMOD) != (b->a_mode&A_AMOD))
		return (0);
	switch (a->a_mode & A_AMOD) {
	case A_X:
	case A_IR:
		return ((a->a_mode&A_REGM) == (b->a_mode&A_REGM)
			&& a->a_value == b->a_value && a->a_sp == b->a_sp);
	case A_DIR:
		return (a->a_sp == b->a_sp && a->a_value == b->a_value);
	}
	return (0);
}

/* a load that FULLY overwrites its word/pair destination (ZLDB writes only the low byte). */
static
purekill(op)
{
	return (op == ZLD || op == ZLDL || op == ZLDA || op == ZCLR);
}

/* a load whose destination register is WRITTEN without being read (so its dest is not a use
 * of the copy) -- unlike an RMW.  Includes the byte forms, which purekill excludes because a
 * byte load only redefines half the word (it does not fully KILL the pair). */
static
pureload(op)
{
	return (op == ZLD || op == ZLDB || op == ZLDL || op == ZLDA
	     || op == ZCLR || op == ZCLRB);
}

/* the word registers an operand reads (as a value, an index, or a far-pair base). */
static
opregs(afp, byteop, out)
register AFIELD	*afp;
register int	*out;
{
	register int	reg;

	reg = afp->a_mode & A_REGM;
	switch (afp->a_mode & A_AMOD) {
	case A_WR:	out[0] = reg; return (1);
	case A_BR:	out[0] = byteop ? (reg&7) : reg; return (1);
	case A_IR:	out[0] = reg; out[1] = reg+1; return (2);	/* far pair base */
	case A_X:	out[0] = reg; return (1);			/* index register */
	}
	return (0);
}

/* the redirect sites collected by deletable(): each is a @RRd operand to repoint to @RRs. */
#define	NREDIR	128
static struct { INS *ip; short k; }	redir[NREDIR];
static int	nredir;

/* find the LLABEL that a JUMP's label number names (linear -- functions are small). */
static INS *
labtarget(labno)
{
	register INS	*p;

	for (p = ins.i_fp; p != &ins; p = p->i_fp)
		if (p->i_type == LLABEL && p->i_labno == labno)
			return (p);
	return (NULL);
}

/* The four bytes of the pair (d,d+1) that operand `o' touches, as a mask: bit0 = Rd low byte,
 * bit1 = Rd high byte, bit2 = R(d+1) low, bit3 = R(d+1) high.  A byte register b addresses
 * word b&7, low byte if b>=8 (RLn = byte reg n+8), high byte if b<8 (RHn = byte reg n). */
static
pairmask(o, byteop, d)
register AFIELD	*o;
{
	register int	r, m;

	m = 0;
	r = o->a_mode & A_REGM;
	switch (o->a_mode & A_AMOD) {
	case A_BR:
		if (byteop) {
			register int	w, lo;
			w = r & 7;
			lo = (r & 8) != 0;
			if (w == d)   m |= lo ? 0x1 : 0x2;
			if (w == d+1) m |= lo ? 0x4 : 0x8;
			break;
		}
		/* a byte register in a word context: the whole word */
	case A_WR:
	case A_X:
		if (r == d)   m |= 0x3;
		if (r == d+1) m |= 0xC;
		break;
	case A_IR:				/* @RRr uses the pair (Rr, R(r+1)) */
		if (r == d || r+1 == d)     m |= 0x3;
		if (r == d+1 || r+1 == d+1) m |= 0xC;
		break;
	}
	return (m);
}

/* CFG-liveness DFS: is register pair (d,d+1) DEAD from `start' on EVERY reachable path -- no
 * read of a live BYTE before that byte is redefined?  Tracks a per-path 4-bit live mask (the
 * bytes of the pair still holding the copied value); a byte-load that rewrites a half before
 * it is read makes it dead (the byte-loop scratch case).  Follows JUMP targets (conditional
 * -> both successors); a CALL clobbers the caller-saved pairs (d<6) unless it reads @RRd;
 * running off the end reaches the epilogue RET; a block move is opaque -> NOT dead.  i_pc
 * carries a per-call visited stamp + the union of masks seen (mask-union worklist, so loops
 * terminate); genfunc rewrites i_pc afterward. */
#define	NDDSTK	256		/* was 1024 = 6 K of bss; see NLAB in emit1.c */
static INS	*ddstk[NDDSTK];
static int	ddmsk[NDDSTK];
static long	ddgen;

static
pairdeadcfg(start, d)
register INS	*start;
{
	register INS	*p, *tg;
	register int	sp, opj, op, m, w;
	register AFIELD	*o;
	int		seen, km;

	if (++ddgen > 0x07FFFFFFL)		/* leave room to pack the 4-bit mask into i_pc */
		ddgen = 1;
	sp = 0;
	ddstk[sp] = start; ddmsk[sp] = 0xF; ++sp;
	while (sp > 0) {
		--sp;
		p = ddstk[sp];
		m = ddmsk[sp];
		if (p == &ins)
			continue;			/* off the end -> reached the RET, dead */
		if (p->i_pc / 16 == ddgen) {		/* visited this query: union the masks */
			seen = (int)(p->i_pc % 16);
			if ((m & ~seen) == 0)
				continue;		/* nothing new live -> already covered */
			m |= seen;
		}
		p->i_pc = ddgen * 16 + m;
		if (p->i_type == JUMP) {
			tg = labtarget(p->i_labno);
			if (tg == NULL || sp >= NDDSTK-1)
				return (0);
			ddstk[sp] = tg; ddmsk[sp] = m; ++sp;
			if ((p->i_rel & 0xF) != 8) {	/* conditional: also the fall-through */
				ddstk[sp] = p->i_fp; ddmsk[sp] = m; ++sp;
			}
			continue;
		}
		if (p->i_type != CODE) {		/* label / line: pass through */
			if (sp >= NDDSTK) return (0);
			ddstk[sp] = p->i_fp; ddmsk[sp] = m; ++sp;
			continue;
		}
		op = p->i_op;
		if (op == ZCALL || op == ZCALR) {
			if ((p->i_af[0].a_mode&A_AMOD) == A_IR && (p->i_af[0].a_mode&A_REGM) == d)
				return (0);		/* the call reads its @RRd fn-pointer */
			if (d < 6)
				continue;		/* caller-saved -> clobbered by the call -> dead */
			if (sp >= NDDSTK) return (0);
			ddstk[sp] = p->i_fp; ddmsk[sp] = m; ++sp;	/* callee-saved -> preserved */
			continue;
		}
		if (barrierop(op))
			return (0);			/* block move: opaque pointer use */
		if (op == ZEXTS && p->i_naddr == 1 && (p->i_af[0].a_mode&A_AMOD) == A_WR) {
			/* EXTS RRr sign-extends the low word R(r+1) into the high word Rr: it READS
			 * R(r+1) and WRITES Rr, so it does not read Rr's copied value. */
			register int	r, rm, wm;
			r = p->i_af[0].a_mode & A_REGM;
			rm = (r+1 == d) ? 0x3 : (r+1 == d+1) ? 0xC : 0;
			wm = (r == d)   ? 0x3 : (r == d+1)   ? 0xC : 0;
			if (rm & m)
				return (0);
			m &= ~wm;
			if (m == 0)
				continue;
			if (sp >= NDDSTK) return (0);
			ddstk[sp] = p->i_fp; ddmsk[sp] = m; ++sp;
			continue;
		}
		w = isbyteop(op);
		for (opj = 0; opj < p->i_naddr; ++opj) {	/* reads: a touched live byte -> live */
			o = &p->i_af[opj];
			if (opj == 0 && !cmpop(op) && pureload(op)
			 && ((o->a_mode&A_AMOD) == A_WR || (o->a_mode&A_AMOD) == A_BR))
				continue;		/* write-only destination, not a read */
			if (pairmask(o, w, d) & m)
				return (0);
		}
		/* a register destination of a non-compare op REDEFINES its bytes -- clear them. */
		if (!cmpop(op) && p->i_naddr > 0
		 && ((p->i_af[0].a_mode&A_AMOD) == A_WR || (p->i_af[0].a_mode&A_AMOD) == A_BR)) {
			km = pairmask(&p->i_af[0], w, d);
			if (isdword(op)) {		/* a long op also writes the odd word of the pair */
				register int	rd;
				rd = p->i_af[0].a_mode & A_REGM;
				if (rd+1 == d)   km |= 0x3;
				if (rd+1 == d+1) km |= 0xC;
			}
			m &= ~km;
		}
		if (m == 0)
			continue;			/* fully redefined on this path -> dead */
		if (sp >= NDDSTK) return (0);
		ddstk[sp] = p->i_fp; ddmsk[sp] = m; ++sp;	/* fall through */
	}
	return (1);					/* no live byte read on any path -> dead */
}

/*
 * Is the reg-reg pair copy at `cp' (RRd <- RRs) safely removable?  Forward linear scan:
 * collect the @RRd deref sites to repoint to @RRs, requiring RRd to be fully overwritten
 * before any non-@RRd read of a live half, and RRs unchanged up to each redirect.  At a
 * label/jump/barrier the linear scan can go no further, so -- when every @RRd use was
 * already collected and RRs is still clean -- fall back to pairdeadcfg (CFG liveness) to
 * confirm RRd is dead past that boundary.  Fills redir[]/nredir; returns 1 if removable.
 */
static
deletable(cp, d, s)
register INS	*cp;
{
	register INS	*p;
	register int	i, opj;
	int		needlow, needhigh, sdirty, regs[2], nr, w;
	register AFIELD	*o;
	register int	op;

	needlow = needhigh = 1;
	sdirty = 0;
	nredir = 0;
	for (p = cp->i_fp; p != &ins; p = p->i_fp) {
		if (p->i_type != CODE) {		/* label/jump: the linear scan stops -- fall back
						 * to CFG liveness if every @RRd use was already
						 * collected and RRs is still clean. */
			if (nredir > 0 && needlow && needhigh && pairdeadcfg(p, d))
				return (1);
			return (0);
		}
		op = p->i_op;
		if (barrierop(op)) {
			if (nredir > 0 && needlow && needhigh && pairdeadcfg(p, d))
				return (1);
			return (0);
		}
		if ((op == ZMULTL || op == ZDIVL) && d <= 3)
			return (0);			/* pinned RQ0 quad consumes the copy */
		w = isbyteop(op);
		for (opj = 0; opj < p->i_naddr; ++opj) {
			o = &p->i_af[opj];
			if ((o->a_mode&A_AMOD) == A_IR && (o->a_mode&A_REGM) == d) {
				if (sdirty)
					return (0);	/* @RRd but RRs already changed */
				if (s == 0)
					return (0);	/* R0 cannot be an addressing base
							 * (register field 0 = "none"): a
							 * redirected @RR0 would encode as DA */
				if (nredir >= NREDIR)
					return (0);
				redir[nredir].ip = p;
				redir[nredir].k = opj;
				++nredir;
				continue;
			}
			if (opj == 0 && !cmpop(op) && pureload(op)
			 && ((o->a_mode&A_AMOD) == A_WR || (o->a_mode&A_AMOD) == A_BR))
				continue;		/* write-only destination, not a read */
			nr = opregs(o, w, regs);
			for (i = 0; i < nr; ++i)
				if ((regs[i] == d && needlow) || (regs[i] == d+1 && needhigh)) {
					/* a read of a live half: repoint it to RRs only if it is a plain
					 * word register in a pure-read position and RRs is clean. */
					if (!w && !sdirty
					 && ((o->a_mode&A_AMOD) == A_WR || (o->a_mode&A_AMOD) == A_BR)
					 && !(opj == 0 && !cmpop(op))) {
						/* a chained copy `LDL RRe,RRd' reads RRd as its source (op#1);
						 * repoint that read to RRs too -- forwarding RRs through the
						 * chain, which collapses copy-then-copy-then-use. */
						if (nredir >= NREDIR)
							return (0);
						redir[nredir].ip = p;
						redir[nredir].k = opj;
						++nredir;
						goto nextop;
					}
					return (0);
				}
			nextop: ;
		}
		/* kills: a full word/pair register destination overwrites a half of RRd. */
		if (!cmpop(op) && purekill(op) && p->i_naddr > 0
		 && ((p->i_af[0].a_mode&A_AMOD) == A_WR || (p->i_af[0].a_mode&A_AMOD) == A_BR)) {
			w = isbyteop(op) ? ((p->i_af[0].a_mode&A_REGM)&7) : (p->i_af[0].a_mode&A_REGM);
			if (w == d)   needlow = 0;
			if (w == d+1) needhigh = 0;
			if (isdword(op)) {
				if (w == d)   needhigh = 0;
				if (w+1 == d) needlow = 0;
			}
		}
		if (!needlow && !needhigh)
			return (1);
		/* the SOURCE pair RRs changed here?  mark dirty (a later @RRd redirect is then unsafe). */
		if (!cmpop(op) && p->i_naddr > 0
		 && ((p->i_af[0].a_mode&A_AMOD) == A_WR || (p->i_af[0].a_mode&A_AMOD) == A_BR)) {
			w = isbyteop(op) ? ((p->i_af[0].a_mode&A_REGM)&7) : (p->i_af[0].a_mode&A_REGM);
			if (w == s || w == s+1 || (isdword(op) && w+1 == s))
				sdirty = 1;
		}
	}
	return (0);				/* ran off the end -- RRd may be live-out */
}

/*
 * Is register pair (d,d+1) dead from instruction `p' onward -- no read of either half
 * before both are overwritten?  Linear forward scan: end-of-function or a RET ends the path
 * (the epilogue's RET is emitted outside the list, so falling off the end is a return);
 * a CALL clobbers the caller-saved pairs (d<6) unless it reads @RRd; a label/jump/block-move
 * is an unanalyzable boundary (conservatively NOT dead).
 */
static
pairdead(p, d)
register INS	*p;
{
	register int	opj, i, nr, w, needlow, needhigh;
	register AFIELD	*o;
	register int	op;
	int		regs[2];

	needlow = needhigh = 1;
	for (; p != &ins; p = p->i_fp) {
		if (p->i_type != CODE)
			return (0);			/* label/jump boundary */
		op = p->i_op;
		if (op == ZCALL || op == ZCALR) {
			if ((p->i_af[0].a_mode&A_AMOD) == A_IR
			 && (p->i_af[0].a_mode&A_REGM) == d && (needlow || needhigh))
				return (0);		/* an indirect call reads its @RRd base */
			if (d < 6)
				return (1);		/* caller-saved: clobbered by the call */
			continue;			/* callee-saved: preserved across */
		}
		if (barrierop(op))
			return (0);			/* block move: opaque pointer use */
		w = isbyteop(op);
		for (opj = 0; opj < p->i_naddr; ++opj) {
			o = &p->i_af[opj];
			if (opj == 0 && !cmpop(op) && pureload(op)
			 && ((o->a_mode&A_AMOD) == A_WR || (o->a_mode&A_AMOD) == A_BR))
				continue;		/* write-only destination */
			nr = opregs(o, w, regs);
			for (i = 0; i < nr; ++i)
				if ((regs[i] == d && needlow) || (regs[i] == d+1 && needhigh))
					return (0);	/* a live half is read */
		}
		if (!cmpop(op) && purekill(op) && p->i_naddr > 0
		 && ((p->i_af[0].a_mode&A_AMOD) == A_WR || (p->i_af[0].a_mode&A_AMOD) == A_BR)) {
			w = isbyteop(op) ? ((p->i_af[0].a_mode&A_REGM)&7) : (p->i_af[0].a_mode&A_REGM);
			if (w == d)   needlow = 0;
			if (w == d+1) needhigh = 0;
			if (isdword(op)) {
				if (w == d)   needhigh = 0;
				if (w+1 == d) needlow = 0;
			}
		}
		if (!needlow && !needhigh)
			return (1);			/* fully overwritten before any read */
	}
	return (1);				/* fell off the end (return) -- dead */
}

/* clear register r's tracked value and any copy that pointed at it. */
static
clrreg(r)
register int	r;
{
	register int	x;

	holds[r].kind = 0;
	pairHold[r] = 0;
	copyOf[r] = -1;
	for (x = 0; x < NMREG; ++x)
		if (copyOf[x] == r)
			copyOf[x] = -1;
}

/*
 * The register-state pass.
 */
static
cpstate()
{
	register INS	*ip;
	register int	op;
	int		byteop, r, dr;
	AFIELD		*d, *s;
	struct hold	k;

	resetstate();
	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type == LLABEL || ip->i_type == JUMP) {
			resetstate();
			continue;
		}
		if (ip->i_type != CODE)
			continue;
		op = ip->i_op;
		if (barrierop(op)) {
			resetstate();
			continue;
		}
		byteop = isbyteop(op);
		d = &ip->i_af[0];
		s = &ip->i_af[1];

		/* store-then-reload of the same register from the same slot: `LD M,Rd ; LD Rd,M'
		 * leaves Rd already holding M -> delete the reload (an adjacent pair; a label between
		 * would let the load be entered without the store, and i_bp then is not this store). */
		if (loadop(op) && ip->i_naddr == 2
		 && ((d->a_mode&A_AMOD) == A_WR || (d->a_mode&A_AMOD) == A_BR)) {
			register INS	*pv;
			pv = ip->i_bp;
			if (pv != &ins && pv->i_type == CODE && pv->i_op == op && pv->i_naddr == 2
			 && ((pv->i_af[1].a_mode&A_AMOD) == A_WR || (pv->i_af[1].a_mode&A_AMOD) == A_BR)
			 && (pv->i_af[1].a_mode&A_REGM) == (d->a_mode&A_REGM)
			 && sameaddr(&pv->i_af[0], s)) {
				ip = deleteins(ip, ip->i_fp);
				++changes;
				continue;
			}
		}

		/* redundant load: `LD Rd,M' where Rd already holds M -> delete. */
		if (loadop(op) && ip->i_naddr == 2
		 && ((d->a_mode&A_AMOD) == A_WR || (d->a_mode&A_AMOD) == A_BR)) {
			dr = byteop ? ((d->a_mode&A_REGM) & 7) : (d->a_mode&A_REGM);
			k.wid = opwidth(op);
			if (memkey(s, &k) && samehold(&holds[dr], &k)) {
				ip = deleteins(ip, ip->i_fp);
				++changes;
				continue;
			}
		}
		/* redundant reg-reg pair copy: `LDL RRd,RRs' where RRd already equals RRs. */
		if (op == ZLDL && ip->i_naddr == 2
		 && ((d->a_mode&A_AMOD) == A_WR || (d->a_mode&A_AMOD) == A_BR)
		 && ((s->a_mode&A_AMOD) == A_WR || (s->a_mode&A_AMOD) == A_BR)) {
			register int	dd, ss;
			dd = d->a_mode&A_REGM;
			ss = s->a_mode&A_REGM;
			if (dd != ss && dd < 15 && ss < 15
			 && ((copyOf[dd] == ss && copyOf[dd+1] == ss+1)
			  || (copyOf[ss] == dd && copyOf[ss+1] == dd+1))) {
				ip = deleteins(ip, ip->i_fp);
				++changes;
				continue;
			}
		}
		/* simpoper: `OP Rd,M' whose source M a register holds -> read the register. */
		k.wid = opwidth(op);
		if (simpoperop(op) && ip->i_naddr == 2
		 && ((d->a_mode&A_AMOD) == A_WR || (d->a_mode&A_AMOD) == A_BR)
		 && memkey(s, &k)) {
			for (r = 0; r < NMREG; ++r)
				if (samehold(&holds[r], &k)) {
					s->a_mode = A_WR | r;
					s->a_sp = NULL;
					s->a_value = 0;
					++changes;
					break;
				}
		}

		/* cmpmem: the memory-left compare `CP M,#k' (relop.t's ADR-left word rule, chosen
		 * because the in-place form is shorter than load-then-compare) where a register
		 * already holds M -- read the REGISTER instead.  `CP Rn,#k' is 4 bytes against the
		 * memory form's 6 (X) or 8 (DA), and the load that put M in Rn is still needed by
		 * whatever else reads it, so this is a pure saving on exactly the sites where the
		 * ADR-left rule would otherwise add a redundant memory read.  A compare writes no
		 * register, so operand#0 is a plain read and the substitution is the same one
		 * simpoper makes on operand#1. */
		if (op == ZCP && ip->i_naddr == 2
		 && (s->a_mode&A_AMOD) == A_IMM
		 && (d->a_mode&A_AMOD) != A_WR && (d->a_mode&A_AMOD) != A_BR
		 && memkey(d, &k)) {
			for (r = 0; r < NMREG; ++r)
				if (samehold(&holds[r], &k)) {
					d->a_mode = A_WR | r;
					d->a_sp = NULL;
					d->a_value = 0;
					++changes;
					break;
				}
		}

		/* ---- state update ---- */
		/* a store to memory forgets ALL tracked memory (no alias analysis).  A compare
		 * (CP, CPB, CPL, TEST, TESTL, BIT) with a memory operand#0 only READS it -- relop.t's
		 * in-place forms put memory there -- so it is not a store and forgets nothing. */
		if (ip->i_naddr > 0 && !cmpop(op)) {
			register int	dm;
			dm = d->a_mode & A_AMOD;
			if (dm == A_DIR || dm == A_X || dm == A_IR)
				for (r = 0; r < NMREG; ++r)
					holds[r].kind = 0;
		}
		/* clear the written destination register(s). */
		if (!cmpop(op) && ip->i_naddr > 0
		 && ((d->a_mode&A_AMOD) == A_WR || (d->a_mode&A_AMOD) == A_BR)) {
			r = byteop ? ((d->a_mode&A_REGM) & 7) : (d->a_mode&A_REGM);
			clrreg(r);
			if (isdword(op) && r < 15)
				clrreg(r+1);
			if (r & 1) {			/* a write to a pair's odd half breaks pair tracking */
				copyOf[r-1] = -1;
				if (pairHold[r-1]) {
					holds[r-1].kind = 0;
					pairHold[r-1] = 0;
				}
			}
		}
		/* MULTL/DIVL clobber the whole RQ0 quad (R0..R3), not just the declared pair. */
		if (op == ZMULTL || op == ZDIVL)
			for (r = 0; r <= 3; ++r)
				clrreg(r);
		/* record a load's value in holds[]. */
		k.wid = opwidth(op);
		if (loadop(op) && ip->i_naddr == 2
		 && ((d->a_mode&A_AMOD) == A_WR || (d->a_mode&A_AMOD) == A_BR)
		 && memkey(s, &k)) {
			dr = byteop ? ((d->a_mode&A_REGM) & 7) : (d->a_mode&A_REGM);
			holds[dr] = k;
			pairHold[dr] = (op == ZLDL);
			if (isdword(op) && dr < 15) {
				holds[dr+1].kind = 0;
				pairHold[dr+1] = 0;
			}
		}
		/* record a STORE's value in holds[]: right after `LD M,Rs' the register Rs and the
		 * memory M hold the same value, exactly as after `LD Rs,M'.  (The store above has
		 * already forgotten every OTHER register's memory, since M may alias it; only this
		 * one relation survives, and it is the one the store just established.)  This is
		 * what makes the memory-left compare free: cc1 stores a computed value to its frame
		 * slot and then compares the slot, so `CP M,#k' is rewritten back to `CP Rs,#k'. */
		if (loadop(op) && ip->i_naddr == 2
		 && ((s->a_mode&A_AMOD) == A_WR || (s->a_mode&A_AMOD) == A_BR)
		 && memkey(d, &k)) {
			register int	sr;
			sr = byteop ? ((s->a_mode&A_REGM) & 7) : (s->a_mode&A_REGM);
			holds[sr] = k;
			pairHold[sr] = (op == ZLDL);
			if (isdword(op) && sr < 15) {
				holds[sr+1].kind = 0;
				pairHold[sr+1] = 0;
			}
		}
		/* a reg-reg pair copy `LDL RRd,RRs': record it, and if RRd is provably dead after
		 * repointing its @RRd derefs to @RRs, do that and delete the copy. */
		if (op == ZLDL && ip->i_naddr == 2
		 && (d->a_mode&A_AMOD) == A_WR && (s->a_mode&A_AMOD) == A_WR) {
			register int	dd, ss;
			dd = d->a_mode&A_REGM;
			ss = s->a_mode&A_REGM;
			if (dd != ss && dd < 15 && ss < 15) {
				copyOf[dd] = ss;
				copyOf[dd+1] = ss+1;
				if (deletable(ip, dd, ss)) {
					for (r = 0; r < nredir; ++r) {
						register AFIELD	*ro;
						ro = &redir[r].ip->i_af[redir[r].k];
						ro->a_mode = (ro->a_mode & ~A_REGM)
							| (((ro->a_mode&A_REGM) == dd) ? ss : ss+1);
					}
					/* the copy is GONE, so RRd does not hold RRs: the
					 * entries recorded above must not survive, or the
					 * redundant-pair rule deletes a repointed chained
					 * copy (`LDL RRd,RRd' -> `LDL RRd,RRs') that is now
					 * the ONLY load of RRd. */
					copyOf[dd] = -1;
					copyOf[dd+1] = -1;
					ip = deleteins(ip, ip->i_fp);
					++changes;
					continue;
				}
			}
		}
	}
}

/*
 * Indirect call through a pointer whose address was just loaded into RR0: CALL @RR0 can
 * not use RR0 as its base, so the encoder would copy RR0 to RR2 first.  When the load
 * feeding the call put the address in RR0, retarget it to RR2 instead and call @RR2 --
 * dropping the copy.  RR2 is caller-saved and free at a call boundary (arguments are on
 * the stack, the return lands in RR0).
 */
static
callreloc()
{
	register INS	*ip, *pv;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_op != ZCALL)
			continue;
		if (ip->i_af[0].a_mode != (A_IR|0) || ip->i_af[0].a_value != 0)
			continue;
		pv = ip->i_bp;
		if (pv == &ins || pv->i_type != CODE || pv->i_op != ZLDL)
			continue;
		if (pv->i_af[0].a_mode != (A_WR|0))		/* LDL RR0,<mem> */
			continue;
		if ((pv->i_af[1].a_mode&A_AMOD) == A_WR || (pv->i_af[1].a_mode&A_AMOD) == A_BR)
			continue;				/* a register source is not a load */
		pv->i_af[0].a_mode = A_WR|2;			/* LDL RR0,mem -> LDL RR2,mem */
		ip->i_af[0].a_mode = A_IR|2;			/* CALL @RR0   -> CALL @RR2   */
		++changes;
	}
}

/*
 * Push-from-memory fold: `LDL RRd,@Rs ; PUSHL RRd' (load a far pointer through Rs, then push
 * it as a call argument) -> `PUSHL @Rs' in one word, when RRd is dead after the push (it was
 * only the deref scratch).  Rs must be an even, non-zero pair base (R0 cannot be an @RRn base).
 */
static
pushfold()
{
	register INS	*ip, *nx;
	register AFIELD	*d, *s;
	int		sreg, dreg;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_op != ZLDL || ip->i_naddr != 2)
			continue;
		d = &ip->i_af[0];
		s = &ip->i_af[1];
		if ((d->a_mode&A_AMOD) != A_WR)
			continue;
		if ((s->a_mode&A_AMOD) != A_IR || s->a_value != 0 || s->a_sp != NULL)
			continue;
		sreg = s->a_mode & A_REGM;
		if (sreg < 2 || sreg >= 15 || (sreg & 1))
			continue;
		dreg = d->a_mode & A_REGM;
		nx = ip->i_fp;
		if (nx == &ins || nx->i_type != CODE || nx->i_op != ZPUSHL || nx->i_naddr != 1)
			continue;
		if (nx->i_af[0].a_mode != (A_WR|dreg))
			continue;
		if (!pairdead(nx->i_fp, dreg))
			continue;
		nx->i_af[0].a_mode = A_IR | sreg;	/* PUSHL RRd -> PUSHL @Rs */
		nx->i_af[0].a_value = 0;
		nx->i_af[0].a_sp = NULL;
		ip = deleteins(ip, nx);			/* drop the load; resume at its predecessor */
		++changes;
	}
}

/*
 * Adjacent copy-forwarding (register coalescing the copy-prop delete could not do): for a
 * pair copy `LDL RRd,RRs' left behind by cpstate, forward-scan within the basic block for the
 * first instruction reading RRd (with RRs still unchanged); repoint that read to RRs and delete
 * the copy when RRd dies at the reader.  Stops at a label/jump/barrier/byte op, an RRs write, or
 * an RRd redefine-before-read.  Runs after cpstate, so it sees only copies deletable() left.
 */
static
copyfwd()
{
	register INS	*ip, *p, *nx;
	register int	i, nr, w;
	int		regs[2];
	int		d, s, opj, readsd, writess, writesd, dpos;
	register AFIELD	*o;
	register int	op;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_op != ZLDL || ip->i_naddr != 2)
			continue;
		if ((ip->i_af[0].a_mode&A_AMOD) != A_WR || (ip->i_af[1].a_mode&A_AMOD) != A_WR)
			continue;
		d = ip->i_af[0].a_mode & A_REGM;
		s = ip->i_af[1].a_mode & A_REGM;
		if (d == s || d >= 15 || s >= 15)
			continue;
		/* find the first reader of RRd, RRs unchanged up to it. */
		nx = NULL;
		for (p = ip->i_fp; p != &ins; p = p->i_fp) {
			if (p->i_type != CODE)
				break;
			op = p->i_op;
			if (barrierop(op) || isbyteop(op))
				break;
			readsd = writess = writesd = 0;
			w = isbyteop(op);
			for (opj = 0; opj < p->i_naddr; ++opj) {
				o = &p->i_af[opj];
				dpos = (opj == 0 && !cmpop(op) && op != ZPUSHL && op != ZPUSH
					&& ((o->a_mode&A_AMOD) == A_WR || (o->a_mode&A_AMOD) == A_BR));
				if (dpos) {
					if ((o->a_mode&A_REGM) == d || (o->a_mode&A_REGM) == d+1) {
						writesd = 1;
						if (!purekill(op))
							readsd = 1;	/* RMW reads d too */
					}
					if ((o->a_mode&A_REGM) == s || (o->a_mode&A_REGM) == s+1)
						writess = 1;
					continue;
				}
				nr = opregs(o, w, regs);
				for (i = 0; i < nr; ++i)
					if (regs[i] == d || regs[i] == d+1)
						readsd = 1;
			}
			if (writess)
				break;
			if (readsd) { nx = p; break; }
			if (writesd)
				break;
		}
		if (nx == NULL || !pairdead(nx->i_fp, d))
			continue;
		/* repoint nx's RRd read(s) to RRs (word halves d->s, d+1->s+1).  op#0 of a
		 * non-compare/non-push op is a destination or RMW target, NOT a forwardable read:
		 * leave it, and abort the whole forward if it is an RMW of RRd or a write of the
		 * still-live RRs (rewriting it would clobber the source). */
		{
			int	op0read, abort;

			op0read = cmpop(nx->i_op) || nx->i_op == ZPUSHL || nx->i_op == ZPUSH;
			abort = 0;
			for (opj = 0; opj < nx->i_naddr && !abort; ++opj) {
				o = &nx->i_af[opj];
				if (opj == 0 && !op0read
				 && ((o->a_mode&A_AMOD) == A_WR || (o->a_mode&A_AMOD) == A_BR)) {
					if ((o->a_mode&A_REGM) == s || (o->a_mode&A_REGM) == s+1)
						abort = 1;		/* would clobber the live RRs */
					else if (!purekill(nx->i_op)
					      && ((o->a_mode&A_REGM) == d || (o->a_mode&A_REGM) == d+1))
						abort = 1;		/* an RMW of RRd */
					continue;
				}
				switch (o->a_mode & A_AMOD) {
				case A_IR:
				case A_X:
					/* R0 cannot be an addressing base or index
					 * (register field 0 = "none"): rewriting the
					 * base of @RRd / disp(Rd) to RR0/R0 would
					 * silently encode as plain DA.  Keep the copy. */
					if ((o->a_mode&A_REGM) == d && s == 0) {
						abort = 1;
						break;
					}
					/* fall through */
				case A_WR:
				case A_BR:
					if ((o->a_mode&A_REGM) == d)
						o->a_mode = (o->a_mode & ~A_REGM) | s;
					else if ((o->a_mode&A_REGM) == d+1)
						o->a_mode = (o->a_mode & ~A_REGM) | (s+1);
					break;
				}
			}
			if (abort)
				continue;			/* leave the copy intact */
		}
		ip = deleteins(ip, ip->i_fp);
		++changes;
	}
}

/*
 * Delete a register-to-self move `LD/LDB/LDL Rd,Rd' -- a no-op the selector emits when the
 * source and destination register coincide (and one copy-forwarding can newly expose).  LD
 * touches no flags, so the deletion is always safe.
 */
static
selfmove()
{
	register INS	*ip;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_naddr != 2)
			continue;
		if (ip->i_op != ZLD && ip->i_op != ZLDB && ip->i_op != ZLDL)
			continue;
		if ((ip->i_af[0].a_mode&A_AMOD) != A_WR && (ip->i_af[0].a_mode&A_AMOD) != A_BR)
			continue;
		if ((ip->i_af[1].a_mode&A_AMOD) != A_WR && (ip->i_af[1].a_mode&A_AMOD) != A_BR)
			continue;
		if ((ip->i_af[0].a_mode&A_REGM) != (ip->i_af[1].a_mode&A_REGM))
			continue;
		ip = deleteins(ip, ip->i_fp);
		++changes;
	}
}

/*
 * Frame far-pointer materialization fix.  cc1 forms a far pointer to a frame object by
 * reading the FP register R13 as a PAIR -- but R13's hardware pair is RR12 = (R12,R13), and
 * R12 is an ordinary scratch, NOT the frame segment.  `LDL RRd,RR12' therefore captures
 * whatever R12 holds as the SEGMENT word, which is garbage once R12 is live (a silent
 * wrong-segment store -- memory corruption).  The correct frame far pointer is
 * (segment = R14 the stack segment, offset = R13 the FP).  R13 is the reserved FP, so RR12 is
 * never a genuine value pair -- `LDL RRd,RR12' is ALWAYS this bogus materialization.  Split
 * each into `LD Rd,R14 ; LD Rd+1,R13' here, before the copy-propagation passes, so no RR12
 * copy survives for them to forward to a later use.
 */
static
framefarptr()
{
	register INS	*ip, *np;
	register int	d, s;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE || ip->i_op != ZLDL || ip->i_naddr != 2)
			continue;
		if ((ip->i_af[0].a_mode&A_AMOD) != A_WR || (ip->i_af[1].a_mode&A_AMOD) != A_WR)
			continue;
		d = ip->i_af[0].a_mode & A_REGM;
		s = ip->i_af[1].a_mode & A_REGM;
		if (d >= 12 || (s != 12 && s != 13))
			continue;
		ip->i_op = ZLD;				/* LDL RRd,RR12 -> LD Rd,R14 (segment) */
		ip->i_af[1].a_mode = A_WR | 14;
		ip->i_af[1].a_sp = NULL;
		ip->i_af[1].a_value = 0;
		np = newn(2);				/* + LD Rd+1,R13 (offset) */
		np->i_type = CODE;
		np->i_op = ZLD;
		np->i_naddr = 2;
		np->i_af[0].a_mode = A_WR | (d+1);
		np->i_af[0].a_sp = NULL;
		np->i_af[0].a_value = 0;
		np->i_af[1].a_mode = A_WR | 13;
		np->i_af[1].a_sp = NULL;
		np->i_af[1].a_value = 0;
		np->i_fp = ip->i_fp;
		np->i_bp = ip;
		ip->i_fp->i_bp = np;
		ip->i_fp = np;
		ip = np;
		++changes;
	}
}

peephole()
{
	framefarptr();
	incexpand();
	subdec();
	shldouble();
	callreloc();
	cpstate();
	copyfwd();
	pushfold();
	selfmove();
}
