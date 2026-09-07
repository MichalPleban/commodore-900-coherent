/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The Z8001 instruction encoder (genins), function frame (genprolog/genepilog), the
 * per-function emit driver (genfunc), and the data pseudo-ops.  genins() forms each
 * instruction from the opcode-table base opcode and the addressing hi-nibble: 0x8000 for
 * a register-direct operand, 0x4000 for a direct/indexed address, plus the index and
 * destination register nibbles and any address words.
 */
#ifdef   vax
#include "INC$LIB:cc2.h"
#else
#include "cc2.h"
#endif

int	framesize;
int	framemask;		/* callee-saved register-variable mask (from AUTOS) */
extern long	outhere();

/* the callee-save plan for the current function, computed at genprolog, reused at
 * genepilog (the INS list is unchanged between them). */
static int	sv_rlo, sv_cnt;	/* save range: low register, count (ends at R13)	*/

/* Distance in bytes from the frame pointer to the top of the frame, i.e. the size the
 * prologue reserves.  The frame pointer addresses the BOTTOM of the frame, so cc1's
 * frame displacements -- which are measured from the top, autos negative and parameters
 * positive -- are emitted with this added.  Set by genprolog before genfunc runs. */
static int	framebase;

/* Set by genprolog when the function needs no frame at all, so genepilog knows to
 * emit nothing but the RET.  Every function goes through genprolog before its
 * genepilog, so this always describes the function being emitted. */
static int	noframe;

static int	frameseg();
static int	isframe();
static int	fpvalop();

/*
 * Emit the segmented address word(s) for a memory operand (DA / X / IR-with-offset).
 * A label/global reference emits a placeholder DA word pair and a relocation; a plain
 * offset uses the short (one-word, seg 0) form when it fits in a byte, else the long
 * (two-word) form
 */
static
emitaddr(afp)
register AFIELD	*afp;
{
	register int	off, fr;

	if (afp->a_sp != NULL) {
		outw(0x8000);			/* DA, segment-present; offset patched at outdone */
		outw(0);
		outfix(afp->a_sp, (long)afp->a_value);
		return;
	}
	off = afp->a_value;
	fr = isframe(afp);
	if (fr && (afp->a_mode&A_REGM) == FPREG)
		off += framebase;	/* cc1 measures from the top of the frame */
	if (off >= 0 && off <= 0xFF)
		outw((unsigned short)(frameseg(fr)<<8 | off));	/* SS: short */
	else {
		outw((unsigned short)(0x8000 | frameseg(fr)<<8));	/* SL: long */
		if (fr && notvariant(VTPA))
			outfixss();		/* the word just written holds the segment byte */
		outw((unsigned short)off);
		return;
	}
	if (fr && notvariant(VTPA))
		outfixss();
}

/* Is this a stack-frame reference -- a symbol-less address indexed by the frame
 * pointer or by the stack pointer's offset half?  A symbol-less address with no
 * index is an absolute machine address (a device register), which is in no
 * segment of this program's and must not take the stack segment. */
static int
isframe(afp)
register AFIELD	*afp;
{
	register int	rn;

	if (afp->a_sp != NULL || (afp->a_mode&A_AMOD) != A_X)
		return (0);
	rn = afp->a_mode & A_REGM;
	return (rn == FPREG || rn == SPREG);
}

/* The segment byte a frame/auto address word is emitted with.
 *
 * Under Coherent it is 0 and carries a byte relocation against the external
 * symbol SS (outfixss), so the LINK states which segment the stack is in: 0 for
 * a program, whose frames are in segment 0 (csu/crts0.s), and 0x3F for the
 * kernel, whose frames are in the system stack segment (md.s).  A kernel
 * compiled with a literal 0 reads and writes every local in segment 0.
 *
 * A CP/M-8000 transient program (VTPA) runs on a stack inside the TPA segment
 * and links against no such symbol, so there the segment is the literal 0x32. */
static int
frameseg(fr)
{
	if (fr && isvariant(VTPA))
		return (0x32);
	return (0);
}

/* @RRn+disp for a form with no BA encoding (single-operand ops, EA-immediate
 * stores): step the pair's offset (odd) register to the operand across the
 * operation and step it back.  Segment-correct, where an X-mode index by the odd
 * register against a segment-0 base would drop the pair's SEGMENT half once code
 * left the flat model.  The displacement
 * can be NEGATIVE -- a store through a register far pointer at a negative index
 * (`app[-1] = p', sh's makargl) offsets the odd register DOWN -- so step both
 * ways.  irunstep reverses irstep exactly.
 *
 * A displacement of 1..16 either way is one INC/DEC #n: one word, 4 cycles, and
 * carry untouched.  Wider is one ADD of the whole 16-bit displacement -- two
 * words and 7 cycles against the two or more INC/DEC words a 4-bit count would
 * need (2n bytes, 4n cycles), so it wins on size from a count of three and on
 * time from a count of two.  The step words are the only flag effect that
 * survives the operation they bracket: they set S/Z/V whichever form is used, so
 * nothing downstream may read the bracketed operation's flags, and the tables
 * accordingly route every compare through a pointer with its displacement
 * already folded into the pair rather than into an @RRn+disp operand. */
static
irstep(rn, disp)
{
	register int	r;

	r = (rn|1) & 0xF;
	if (disp >= 1 && disp <= 16)
		outw(0xA900 | r<<4 | (disp-1));		/* INC Rodd,#n */
	else if (disp >= -16 && disp <= -1)
		outw(0xAB00 | r<<4 | (-disp-1));	/* DEC Rodd,#n */
	else if (disp != 0) {
		outw(0x0100 | r);			/* ADD Rodd,#disp */
		outw((unsigned short)disp);
	}
}

static
irunstep(rn, disp)
{
	register int	r;

	r = (rn|1) & 0xF;
	if (disp >= 1 && disp <= 16)
		outw(0xAB00 | r<<4 | (disp-1));		/* DEC Rodd,#n (undo INC) */
	else if (disp >= -16 && disp <= -1)
		outw(0xA900 | r<<4 | (-disp-1));	/* INC Rodd,#n (undo DEC) */
	else if (disp != 0) {
		outw(0x0100 | r);			/* ADD Rodd,#-disp */
		outw((unsigned short)(0 - disp));
	}
}

/*
 * Does this ZLD encode as the one-word LDK Rd,#k?  A 0..15 immediate with no relocation
 * into a register does; anything else takes the two-word LD Rd,#imm.  The substitution is
 * made in genins, below the INS list, so it is invisible to cc3 and to the peephole --
 * listing.c asks here rather than restating the rule.
 */
ldkform(ip)
register INS	*ip;
{
	register AFIELD	*s;

	if (ip->i_type != CODE || ip->i_op != ZLD || ip->i_naddr != 2)
		return (0);
	if ((ip->i_af[0].a_mode&A_AMOD) != A_WR
	 && (ip->i_af[0].a_mode&A_AMOD) != A_BR)
		return (0);
	s = &ip->i_af[1];
	return ((s->a_mode&A_AMOD) == A_IMM && s->a_sp == NULL
	     && s->a_value >= 0 && s->a_value <= 15);
}

/* byte-immediate replicate: a Z8000 byte op (even opcode high byte) puts the data byte
 * in BOTH halves of the immediate word (the HW reads the high half). */
static
immword(base, v)
{
	register int	b;

	if (((base>>8)&1) == 0) {
		b = v & 0xFF;
		return ((b<<8) | b);
	}
	return (v & 0xFFFF);
}

/* "OP EA,#imm" sub-op nibble: LD=5, CP=1. */
static
eanib(op)
{
	if (op == ZLD || op == ZLDB)	return (5);
	if (op == ZCP || op == ZCPB)	return (1);
	cbotch("genins: no EA-immediate form");
	return (0);
}

/* INC/DEC of a memory operand: 0 if none. */
static
incdecmem(op)
{
	if (op == ZINCB)	return (0x6800);
	if (op == ZINC)		return (0x6900);
	if (op == ZDECB)	return (0x6A00);
	if (op == ZDEC)		return (0x6B00);
	return (0);
}

/* store-to-memory base opcode: 0 if none. */
static
storebase(op)
{
	if (op == ZLD)		return (0x6F00);
	if (op == ZLDB)		return (0x6E00);
	if (op == ZLDL)		return (0x5D00);
	return (0);
}

/*
 * Direct call to a named function: CALL addr -> 0x5F (segmented DA), seg-prefix word,
 * offset word (relocated to the callee).  Indirect call through a pair: CALL @RRn (0x1F).
 */
static
gencall(ip)
register INS	*ip;
{
	register AFIELD	*t;

	t = &ip->i_af[0];
	if (t->a_sp != NULL) {			/* CALL fn  (direct, named) */
		outw(0x5F00);
		outw(0x8000);
		outw(0);
		outfix(t->a_sp, 0L);		/* patch the offset word to the callee */
		return;
	}
	if ((t->a_mode&A_AMOD) == A_WR || (t->a_mode&A_AMOD) == A_BR) {
		outw(0x1F00 | (t->a_mode&A_REGM)<<4);	/* CALL @RRn (the pointer is in the pair) */
		return;
	}
	/* the pointer lives in memory: load its value into RR0, then CALL @R0. */
	switch (t->a_mode & A_AMOD) {
	case A_X:				/* off(Rn) */
		outw(0x5400 | (t->a_mode&A_REGM)<<4);
		emitaddr(t);
		break;
	case A_DIR:				/* addr */
		outw(0x5400);
		emitaddr(t);
		break;
	case A_IR:				/* @RRn (the pointer's address is in the pair) */
		if (t->a_value != 0) {		/* @RRn+disp: base-address (BA) long load */
			if ((t->a_mode&A_REGM) == 0) {	/* R0 cannot be a BA base -> go via RR2 */
				outw(0x9402);		/* LDL RR2,RR0 */
				outw(0x3520);		/* LDL RR0,disp(@RR2) */
				outw((unsigned short)t->a_value);
			} else {
				outw(0x3500 | (t->a_mode&A_REGM)<<4);
				outw((unsigned short)t->a_value);
			}
		} else if ((t->a_mode&A_REGM) == 0) {
			outw(0x9402);		/* LDL RR2,RR0  (R0 cannot be an @RRn base) */
			outw(0x1420);		/* LDL RR0,@RR2 */
		} else
			outw(0x1400 | (t->a_mode&A_REGM)<<4);	/* LDL RR0,@RRn */
		break;
	default:
		cbotch("gencall: indirect-call addressing");
	}
	outw(0x1F00);				/* CALL @R0 */
}

genins(ip)
register INS	*ip;
{
	register OPINFO	*opp;
	register AFIELD	*d, *s;
	register int	base, dam, sam, dn, sn, sty;

	if (ip->i_type != CODE) {		/* a pool record routed straight here by getfunc */
		register SYM	*lp;
		switch (ip->i_type) {
		case ENTER:			/* segment switch (a far-pointer pool literal) */
			genseg(ip->i_seg);
			return;
		case LLABEL:			/* a data-segment label: fix its address */
			lp = (ip->i_sp != NULL) ? ip->i_sp : llookup(ip->i_labno, 1);
			lp->s_seg = dotseg;
			lp->s_value = dot;
			return;
		case BLOCK:
			genblock(ip->i_len);
			return;
		case ALIGN:
			genalign(ip->i_align);
			return;
		default:
			return;			/* LINE etc: nothing to emit */
		}
	}
	opp = &opinfo[ip->i_op];
	base = opp->op_opcode & 0xFFFF;
	sty = opp->op_style;

	/* data pseudo-ops: lay down initialized data rather than an instruction. */
	if (sty == OF_BYTE_) { outab((int)(ip->i_af[0].a_value & 0xFF)); return; }
	if (sty == OF_WORD)  { outw((unsigned short)ip->i_af[0].a_value); return; }
	if (sty == OF_LPTR || sty == OF_GPTR) {	/* far pointer datum: segment 0 : offset */
		outw(0);
		if (ip->i_af[0].a_sp != NULL) {
			outw(0);
			outfix(ip->i_af[0].a_sp, (long)ip->i_af[0].a_value);
		} else
			outw((unsigned short)ip->i_af[0].a_value);
		return;
	}

	if (ip->i_op == ZCALL) { gencall(ip); return; }

	/* block move: LDIRB/LDIR/LDDRB/LDDR @RRd,@RRs,Rcount.  word1 = base | src<<4;
	 * word2 = count<<8 | dst<<4. */
	if (ip->i_op == ZLDIRB || ip->i_op == ZLDIR || ip->i_op == ZLDDRB || ip->i_op == ZLDDR) {
		outw(base | (ip->i_af[1].a_mode&A_REGM)<<4);
		outw((ip->i_af[2].a_mode&A_REGM)<<8 | (ip->i_af[0].a_mode&A_REGM)<<4);
		return;
	}

	if (ip->i_naddr == 1) {
		s = &ip->i_af[0];
		sam = s->a_mode & A_AMOD;
		sn = s->a_mode & A_REGM;
		if (sty == OF_EXTS)			/* EXTS RRd: base | reg<<4 */
			{ outw(base | sn<<4); return; }
		if (sty == OF_FLAG)			/* SETFLG/RESFLG/COMFLG #mask */
			{ outw(base | (int)(s->a_value&0xF)<<4); return; }
		if (ip->i_op == ZTESTL && sam != A_X && sam != A_DIR && sam != A_IR)
			{ outw(0x9C08 | sn<<4); return; }	/* TESTL RRd (register pair) */
		if (sty == OF_PUSH) {			/* PUSH/PUSHL @R15,Rs */
			if (sam == A_WR || sam == A_BR) {
				outw((ip->i_op==ZPUSHL ? 0x91F0 : 0x93F0) | sn);
				return;
			}
			if (sam == A_IR && ip->i_op == ZPUSHL) {	/* PUSHL @R15,@Rs (far pointer from memory) */
				outw(0x11F0 | sn);
				return;
			}
			if (sam == A_IMM && ip->i_op == ZPUSH) {	/* PUSH @R15,#imm16 */
				outw(0x0DF9);
				outw((unsigned short)s->a_value);
				if (s->a_sp != NULL)
					outfix(s->a_sp, (long)s->a_value);
				return;
			}
			cbotch("genins: PUSH source");
		}
		if (sty == OF_POP) {			/* POP/POPL Rd,@R15 (dst is the operand) */
			if (sam == A_WR || sam == A_BR) {
				outw((ip->i_op==ZPOPL ? 0x95F0 : 0x97F0) | sn);
				return;
			}
			cbotch("genins: POP dest");
		}
		switch (sam) {			/* single-operand (CLR/COM/NEG/TEST/...) */
		case A_WR:
		case A_BR:
			outw((base|0x8000) | sn<<4);
			return;
		case A_IR:
			if (s->a_value != 0) {	/* @RRn+disp: step the offset register */
				irstep(sn, (int)s->a_value);
				outw(base | sn<<4);
				irunstep(sn, (int)s->a_value);
				return;
			}
			outw(base | sn<<4);
			return;
		case A_X:
			outw((base|0x4000) | sn<<4);
			emitaddr(s);
			return;
		case A_DIR:
			outw(base|0x4000);
			emitaddr(s);
			return;
		}
		cbotch("genins: single-operand mode");
	}

	if (ip->i_naddr == 2) {
		d = &ip->i_af[0];
		s = &ip->i_af[1];
		dam = d->a_mode & A_AMOD;
		sam = s->a_mode & A_AMOD;
		dn = d->a_mode & A_REGM;
		sn = s->a_mode & A_REGM;

		if (sty == OF_SHIFT) {		/* SLL/SLA Rd,#count -- 2-word, signed count */
			outw(base | dn<<4);
			outw((unsigned short)s->a_value);
			return;
		}
		if (sty == OF_SHIFTD) {		/* SDL/SDA Rd,Rs -- dynamic (count in Rs) */
			outw((base|0x8000) | dn<<4);
			outw(sn<<8);
			return;
		}
		if (sty == OF_INCDEC) {		/* INC/DEC reg/mem,#count: count-1 in low nibble */
			register int	n;
			n = (s->a_value - 1) & 0xF;
			switch (dam) {
			case A_WR: case A_BR:	outw((base|0x8000) | dn<<4 | n); return;
			case A_IR:
				if (d->a_value != 0) {	/* INC/DEC @RRd+disp */
					irstep(dn, (int)d->a_value);
					outw(base | dn<<4 | n);
					irunstep(dn, (int)d->a_value);
					return;
				}
				outw(base | dn<<4 | n); return;
			case A_X:		outw((base|0x4000) | dn<<4 | n); emitaddr(d); return;
			case A_DIR:		outw((base|0x4000) | n); emitaddr(d); return;	/* INC/DEC addr,#n (global) */
			}
			cbotch("genins: INC/DEC mode");
		}
		if (sty == OF_LDA) {		/* LDA Rd,addr(Rs): effective address (0x76) */
			switch (sam) {
			case A_X:	outw(0x7600 | sn<<4 | dn); emitaddr(s); return;
			case A_DIR:	outw(0x7600 | dn); emitaddr(s); return;
			}
			cbotch("genins: LDA source");
		}

		if (dam == A_WR || dam == A_BR) {	/* register destination */
			switch (sam) {
			case A_WR:
			case A_BR:			/* OP Rd,Rs */
				outw((base|0x8000) | sn<<4 | dn);
				return;
			case A_IMML:			/* OP RRd,#imm32 (high word, then low) */
				outw(base | dn);
				outw((unsigned short)(s->a_value >> 16));
				outw((unsigned short)s->a_value);
				return;
			case A_IMM:			/* OP Rd,#imm */
				if (ldkform(ip)) {
					outw(0xBD00 | dn<<4 | (int)(s->a_value&0xF));	/* LDK Rd,#k */
					return;
				}
				outw(base | dn);
				if (s->a_sp != NULL) {	/* &fn / &global immediate -> relocate */
					outw(0);
					outfix(s->a_sp, (long)s->a_value);
				} else
					outw(immword(base, (int)s->a_value));
				return;
			case A_IR:			/* OP Rd,@RRs [+disp] */
				if (s->a_value != 0) {
					/* LD/LDB/LDL have a direct BA (@RRn+disp) form;
					 * other ALU ops (ADD/SUB/CP/AND/OR/...) do not,
					 * so step the source pair's offset register across
					 * the base-form @RRn op (segment-correct; the same
					 * INC/DEC-Rodd idiom the single-operand path uses).
					 * A far-pointer field read as an ALU operand -- e.g.
					 * bc/yacc `n = p->field + x'. */
					if (ip->i_op == ZLD || ip->i_op == ZLDB
					 || ip->i_op == ZLDL) {
						register int	ba;
						ba = (ip->i_op==ZLDB) ? 0x3000
						   : (ip->i_op==ZLDL) ? 0x3500 : 0x3100;
						outw(ba | sn<<4 | dn);
						outw((unsigned short)s->a_value);
						return;
					}
					irstep(sn, (int)s->a_value);
					outw(base | sn<<4 | dn);
					irunstep(sn, (int)s->a_value);
					return;
				}
				outw(base | sn<<4 | dn);
				return;
			case A_X:			/* OP Rd,off(Rs)  indexed (seg 0) */
				outw((base|0x4000) | sn<<4 | dn);
				emitaddr(s);
				return;
			case A_DIR:			/* OP Rd,addr */
				outw((base|0x4000) | dn);
				emitaddr(s);
				return;
			}
			cbotch("genins: reg-dest source mode");
		}

		/* memory destination (store / op-to-memory) */
		if (dam == A_IR) {		/* store THROUGH a pair: @RRd [+disp -> BA] */
			if (sam == A_WR || sam == A_BR) {
				if (d->a_value != 0) {
					register int	ba;
					ba = (ip->i_op==ZLDB) ? 0x3200 : (ip->i_op==ZLDL) ? 0x3700 : 0x3300;
					outw(ba | dn<<4 | sn);
					outw((unsigned short)d->a_value);
					return;
				}
				outw(((ip->i_op==ZLDB) ? 0x2E00 : (ip->i_op==ZLDL) ? 0x1D00 : 0x2F00)
					| dn<<4 | sn);
				return;
			}
			if (sam == A_IMM && d->a_value == 0) {	/* LD @Rd,#imm (IR EA-immediate) */
				register int	hi;
				hi = (ip->i_op==ZLDB || ip->i_op==ZCPB) ? 0x0C00 : 0x0D00;
				outw(hi | dn<<4 | eanib(ip->i_op));
				outw(immword(hi, (int)s->a_value));
				return;
			}
			if (sam == A_IMM) {		/* LD @RRd+disp,#imm: the Z8000 has no BA-immediate
							 * store, so step the pair's offset register across the
							 * IR EA-immediate form (segment-correct; a seg-0
							 * X-mode idiom would lose the pair's segment half). */
				register int	hi;
				hi = (ip->i_op==ZLDB || ip->i_op==ZCPB) ? 0x0C00 : 0x0D00;
				irstep(dn, (int)d->a_value);
				outw(hi | dn<<4 | eanib(ip->i_op));
				if (s->a_sp != NULL) {
					outw(0);
					outfix(s->a_sp, (long)s->a_value);
				} else
					outw(immword(hi, (int)s->a_value));
				irunstep(dn, (int)d->a_value);
				return;
			}
			cbotch("genins: @RRd store source");
		}
		if (dam == A_X || dam == A_DIR) {
			register int	mb;
			if ((mb = incdecmem(ip->i_op)) != 0 && sam == A_IMM) {	/* INC/DEC mem */
				outw(mb | (dam==A_X ? dn<<4 : 0) | (int)((s->a_value-1)&0xF));
				emitaddr(d);
				return;
			}
			if (sam == A_WR || sam == A_BR) {	/* store reg -> mem */
				register int	sb;
				sb = storebase(ip->i_op);
				if (sb == 0) cbotch("genins: no store form");
				outw(sb | dn<<4 | sn);
				emitaddr(d);
				return;
			}
			if (sam == A_IMM) {		/* OP mem,#imm (EA-immediate) */
				register int	hi;
				hi = (ip->i_op==ZLDB || ip->i_op==ZCPB) ? 0x0C00 : 0x0D00;
				outw(hi | 0x4000 | dn<<4 | eanib(ip->i_op));
				emitaddr(d);
				if (s->a_sp != NULL) {	/* storing &fn/&global: relocate the imm word */
					outw(0);
					outfix(s->a_sp, (long)s->a_value);
				} else
					outw(immword(hi, (int)s->a_value));
				return;
			}
			cbotch("genins: mem-dest source mode");
		}
	}
	cbotch("genins: unhandled op/mode");
}

/*
 * Walk the buffered function (the doubly-linked INS list) emitting each item.  Records
 * each instruction's PC for branch resolution.  add() and other leaf bodies have no
 * JUMPs; branch emission is the next piece.
 */
/*
 * Walk the buffered function emitting each item.  A branch is a span-dependent JUMP: it
 * starts as a one-word `JR cc,disp' (signed 8-bit word displacement, +-128 words).  If the
 * target is out of reach the branch is PROMOTED to a three-word `JP cc,addr' (opcode, SL
 * segment prefix, absolute offset), which reaches the whole segment.  Promotion grows the
 * code and shifts every later PC -- possibly pushing another JR out of range -- so re-emit
 * the whole function and iterate to a fixed point; promotion is monotonic (i_long only
 * 0->1), so this converges.  Once stable, patch each JR displacement and each JP target
 * (an in-text address carrying an L_SHRI relocation so ld adjusts it by the final base).
 */
#define	NLAB	256		/* per function; was 1024 = 14 K of bss, which starved the heap on a 64 K data segment */
static struct { int labno; long pc; }	labv[NLAB];
static struct { long pc; INS *ip; }	brv[NLAB];
genfunc()
{
	register INS	*ip;
	register int	i, j;
	int		nlab, nbr, grew, cc;
	long		tgt, fstart, fdot;
	int		fmark, disp;

	fstart = outhere();
	fdot = dot;
	fmark = outfixmark();

	for (;;) {
		nlab = nbr = 0;
		for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
			ip->i_pc = outhere();
			switch (ip->i_type) {
			case CODE:
				genins(ip);
				break;
			case LLABEL:
				if (nlab >= NLAB) cbotch("genfunc: too many labels");
				labv[nlab].labno = ip->i_labno;
				labv[nlab].pc = outhere();
				++nlab;
				break;
			case JUMP:
				if (nbr >= NLAB) cbotch("genfunc: too many branches");
				brv[nbr].pc = outhere();
				brv[nbr].ip = ip;
				++nbr;
				cc = ip->i_rel & 0xF;
				if (ip->i_long) {			/* JP cc,addr (three words) */
					outw(0x5E00 | cc);
					outw(0x8000);			/* SL segment prefix, segment 0 */
					outw(0);			/* offset patched below */
				} else
					outw(0xE000 | cc<<8);		/* JR cc, disp patched below */
				break;
			default:
				break;
			}
		}
		/* promote any short branch now out of reach, then re-emit with the new sizes. */
		grew = 0;
		for (i = 0; i < nbr; ++i) {
			if (brv[i].ip->i_long)
				continue;
			tgt = -1;
			for (j = 0; j < nlab; ++j)
				if (labv[j].labno == brv[i].ip->i_labno) {
					tgt = labv[j].pc;
					break;
				}
			if (tgt < 0)
				cbotch("genfunc: unresolved branch label");
			disp = (tgt - (brv[i].pc + 2)) / 2;
			if (disp < -128 || disp > 127) {
				brv[i].ip->i_long = 1;
				grew = 1;
			}
		}
		if (!grew)
			break;
		outrewind(fstart, fmark);	/* discard this pass; re-emit with the promotions */
		dot = fdot;
	}

	for (i = 0; i < nbr; ++i) {		/* stable layout: patch JR disps and JP targets */
		tgt = -1;
		for (j = 0; j < nlab; ++j)
			if (labv[j].labno == brv[i].ip->i_labno) {
				tgt = labv[j].pc;
				break;
			}
		if (tgt < 0)
			cbotch("genfunc: unresolved branch label");
		if (brv[i].ip->i_long)
			outjptext(brv[i].pc + 4, tgt);		/* JP offset word + L_SHRI reloc */
		else {
			disp = (tgt - (brv[i].pc + 2)) / 2;	/* signed 8-bit word displacement */
			outpatchb(brv[i].pc + 1, disp);
		}
	}
}

/* mark a callee-saved word register (R6..R12) touched by an operand. */
static
markreg(r, maskp)
int	*maskp;
{
	if (r >= 6 && r <= 12)
		*maskp |= 1 << r;
}

/* record every callee-saved register an operand touches.  `dword' says the
 * instruction operates on a PAIR (OP_DWORD), in which case a register operand
 * names RRn and both halves are touched -- missing the odd half is how a
 * function ends up saving R11 and then clobbering R10 with an LDL. */
static
markop(afp, byteop, dword, maskp)
register AFIELD	*afp;
int		*maskp;
{
	register int	reg;

	reg = afp->a_mode & A_REGM;
	switch (afp->a_mode & A_AMOD) {
	case A_WR:
	case A_BR:
		if (byteop)
			markreg(reg&7, maskp);
		else {
			markreg(reg, maskp);
			if (dword)
				markreg(reg+1, maskp);
		}
		break;
	case A_X:				/* addr(Rn): the index is a WORD */
		markreg(reg, maskp);
		break;
	case A_IR:				/* @RRn: both halves of the pair */
		markreg(reg, maskp);
		markreg(reg+1, maskp);
		break;
	}
}

/*
 * Compute the callee-save plan for the current function: the registers cc0 reserved as
 * register variables (framemask) UNION every callee-saved register the body actually
 * writes (scanned from the buffered INS list).  The save area sits at the bottom of the
 * frame and holds one contiguous run of registers ending at R13, so the caller's frame
 * pointer is saved by the same instruction; the run starts at the lowest register that
 * has to be saved, and covers any unused register between that one and R13.
 */
static
plansaves()
{
	register INS	*ip;
	register int	rg, i;
	int		sv_mask;	/* registers R6..R12 the body needs saved */

	sv_mask = framemask;
	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		register OPINFO	*opp;
		int		byteop;
		if (ip->i_type != CODE || ip->i_op == ZCALL)
			continue;
		opp = &opinfo[ip->i_op];
		if (opp->op_style == OF_BYTE_ || opp->op_style == OF_WORD)
			continue;
		/* Byte-ness comes from the table's OP_BYTE flag.  Deriving it from
		 * the opcode -- ((op_opcode>>8)&1)==0 -- only works for the LD/LDB
		 * pair and misreads every other op: ZLDL is 0x1400, so it read as a
		 * BYTE op and its register operand was masked &7, turning RR10 into
		 * R2 and marking nothing.  A scratch pair then went unsaved. */
		byteop = (opp->op_flag & OP_BYTE) != 0;
		for (i = 0; i < ip->i_naddr; ++i)
			markop(&ip->i_af[i], byteop,
			       (opp->op_flag & OP_DWORD) != 0, &sv_mask);
	}
	sv_rlo = 13;
	for (rg = 6; rg <= 12; ++rg)
		if (sv_mask&(1<<rg)) {
			sv_rlo = rg;
			break;
		}
	sv_cnt = 14 - sv_rlo;
}

/*
 * Whole size of the frame in bytes: the locals, rounded up to an even size because the
 * Z8000 stack must stay word-aligned (an odd SP misaligns every pushed word arg -- and
 * every syscall arg the kernel reads), plus the register save area at the bottom.  The
 * save area always holds at least R13, so this is never zero.  plansaves() reads only
 * the finished INS list, so this may be asked before genprolog runs.
 */
framereserve()
{
	plansaves();
	return (((framesize + 1) & ~1) + sv_cnt*2);
}

/*
 * The census framerebase() works from: the index of the operand that names R13 as a
 * register VALUE, or -1 if the instruction has none.  cc1 materializes the frame
 * pointer this way -- with the displacement as a separate immediate -- so such an
 * operand never reaches emitaddr(), and it is the one form of frame reference that a
 * scan of address operands cannot see.  frameless() asks the same question, so the
 * two agree on what counts as a use of the frame register by construction.
 */
static int
fpvalop(ip)
register INS	*ip;
{
	register int	i;

	for (i = 0; i < ip->i_naddr; ++i)
		if ((ip->i_af[i].a_mode&A_AMOD) == A_WR
		 && (ip->i_af[i].a_mode&A_REGM) == FPREG)
			return (i);
	return (-1);
}

/*
 * Rewrite the frame register's remaining uses for a bottom-based frame pointer.
 *
 * emitaddr() rebases a displacement indexed by R13, which covers every frame
 * address operand.  What it cannot reach is R13 used as a VALUE: cc1 materializes
 * the frame pointer into a register (`&x' as a far pointer, a local array's base)
 * and then does arithmetic on it with the displacement as an IMMEDIATE.  Such a
 * register has to hold the TOP of the frame, which is R13 plus the frame size.
 *
 *	LD Rd,R13	-> LDA Rd,frame-top(R13), folding a following constant
 *			   add/subtract on Rd into the same displacement
 *	ADD Rd,R13	-> the same add, plus the frame size
 *
 * Any other use of R13 as a register operand has no rebasing rule and is a botch
 * rather than a silently wrong address.
 */
static
framerebase()
{
	register INS	*ip, *np;
	register int	i, d, k;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE)
			continue;
		if ((i = fpvalop(ip)) < 0)
			continue;
		if (i != 1 || ip->i_naddr != 2
		 || (ip->i_af[0].a_mode&A_AMOD) != A_WR
		 || (ip->i_af[0].a_mode&A_REGM) == FPREG)
			cbotch("frame register operand, op=%d", ip->i_op);
		d = ip->i_af[0].a_mode & A_REGM;
		if (ip->i_op == ZADD) {			/* index + frame base */
			np = newn(2);
			np->i_type = CODE;
			np->i_naddr = 2;
			np->i_af[0].a_mode = A_WR | d;
			np->i_af[0].a_sp = NULL;
			np->i_af[0].a_value = 0;
			np->i_af[1].a_mode = A_IMM;
			np->i_af[1].a_sp = NULL;
			np->i_af[1].a_value = framebase;
			np->i_op = (framebase <= 16) ? ZINC : ZADD;
			np->i_fp = ip->i_fp;
			np->i_bp = ip;
			ip->i_fp->i_bp = np;
			ip->i_fp = np;
			ip = np;
			continue;
		}
		if (ip->i_op != ZLD)
			cbotch("frame register operand, op=%d", ip->i_op);
		k = 0;
		np = ip->i_fp;				/* fold a constant step on Rd */
		if (np != &ins && np->i_type == CODE && np->i_naddr == 2
		 && np->i_af[0].a_mode == (unsigned)(A_WR|d)
		 && (np->i_af[1].a_mode&A_AMOD) == A_IMM
		 && np->i_af[1].a_sp == NULL) {
			if (np->i_op == ZADD || np->i_op == ZINC)
				k = (int)np->i_af[1].a_value;
			else if (np->i_op == ZSUB || np->i_op == ZDEC)
				k = -(int)np->i_af[1].a_value;
			if (k != 0) {
				np->i_bp->i_fp = np->i_fp;
				np->i_fp->i_bp = np->i_bp;
			}
		}
		ip->i_op = ZLDA;
		ip->i_af[1].a_mode = A_X | FPREG;
		ip->i_af[1].a_sp = NULL;
		ip->i_af[1].a_value = k;
	}
}

/*
 * Does this operand name R13?  Mirrors markop()'s register accounting: a BYTE operand
 * names one of RH0..RL7 whatever its register code, a PAIR operand names both halves,
 * and an indexed address names its index register.  A_X indexed by R13 is reported
 * separately (through *xfp) because it is the one form that survives frame elision:
 * it addresses the parameter region, which sits at the same place off R15.
 */
static int
fpuse(afp, byteop, dword, xfp)
register AFIELD	*afp;
int		*xfp;
{
	register int	reg;

	reg = afp->a_mode & A_REGM;
	switch (afp->a_mode & A_AMOD) {
	case A_IMM:
	case A_IMML:
	case A_DIR:
		return (0);
	case A_WR:
	case A_BR:
		if (byteop)
			return (0);		/* a byte register code, not R13 */
		return (reg == FPREG || (dword && reg == FPREG-1));
	case A_IR:				/* @RRn: both halves of the pair */
		return (reg == FPREG || reg == FPREG-1);
	case A_X:				/* addr(Rn): the index is a WORD */
		if (reg != FPREG)
			return (0);
		*xfp = 1;
		return (0);
	}
	return (1);				/* an unknown mode: assume the worst */
}

/*
 * Can this function do without a frame?  Only when it needs no part of one.  The
 * frame region holds the locals AND the compiler temporaries (framesize is cc1's
 * maxtemp, which covers maxauto), the save area holds the callee-saved registers, and
 * R13 addresses all of it -- so `needs no part of one' means framesize is zero, no
 * register has to be saved, and R13 is named nowhere except as the index of a frame
 * ADDRESS.  Such an address is a parameter: with no frame the stack pointer still
 * stands where the CALL left it, so the four-byte segmented return address is all that
 * lies below the parameters, and cc1's displacement -- measured from the top of the
 * frame, so +4 for the first parameter -- reaches it unchanged off R15.
 *
 * That last equality holds only while the stack pointer does not move, which rules out
 * any function that calls another: cc1 pushes arguments one at a time and loads the
 * later ones after the earlier pushes have already dropped R15.  A PUSH or POP outside
 * a call sequence would move it the same way, so those are refused too.
 *
 * Anything not recognised is refused.  Eliding a frame a function actually uses does
 * not cost cycles, it reads the wrong memory.
 */
static int
frameless()
{
	register INS	*ip;
	register int	i;
	int		xfp;

	if (framesize != 0 || sv_cnt != 1)
		return (0);
	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		register OPINFO	*opp;
		if (ip->i_type != CODE)
			continue;
		if (ip->i_op == ZCALL)
			return (0);
		opp = &opinfo[ip->i_op];
		if (opp->op_style == OF_BYTE_ || opp->op_style == OF_WORD
		 || opp->op_style == OF_LPTR || opp->op_style == OF_GPTR)
			continue;		/* a data pseudo-op, no registers */
		if (opp->op_style == OF_PUSH || opp->op_style == OF_POP)
			return (0);
		if (fpvalop(ip) >= 0)		/* R13 as a value: framerebase()'s census */
			return (0);
		for (i = 0; i < ip->i_naddr; ++i) {
			xfp = 0;
			if (fpuse(&ip->i_af[i], (opp->op_flag & OP_BYTE) != 0,
				  (opp->op_flag & OP_DWORD) != 0, &xfp))
				return (0);
			if (xfp && (ip->i_af[i].a_sp != NULL
				 || ip->i_af[i].a_value < 0))
				return (0);	/* not a plain parameter displacement */
		}
	}
	return (1);
}

/*
 * Re-index every frame address on the stack pointer, for a function whose frame has
 * been elided.  frameless() has already established that these are all parameters and
 * that R15 does not move, so each one reaches the same byte it would have off R13.
 */
static
frametosp()
{
	register INS	*ip;
	register int	i;

	for (ip = ins.i_fp; ip != &ins; ip = ip->i_fp) {
		if (ip->i_type != CODE)
			continue;
		for (i = 0; i < ip->i_naddr; ++i)
			if ((ip->i_af[i].a_mode&A_AMOD) == A_X
			 && (ip->i_af[i].a_mode&A_REGM) == FPREG)
				ip->i_af[i].a_mode = A_X | SPREG;
	}
}

/*
 * Function prologue: reserve the whole frame, save the callee-saved registers and the
 * caller's frame pointer at its bottom, and point the frame pointer there.  Locals then
 * lie at small POSITIVE displacements off R13, which the Z8000 addresses with a
 * one-word operand.  The saved registers and the saved frame pointer are inside the
 * frame, so only the four-byte segmented return address separates the top of the frame
 * from the first parameter.
 *
 * A function with nothing to put in a frame gets none: frameless() decides, and the
 * body then addresses its parameters off the stack pointer instead.  R13 is left
 * holding the CALLER's frame pointer, so the frame chain through such a function is
 * the caller's own.
 */
genprolog()
{
	register int	res;

	res = framereserve();
	if ((noframe = frameless()) != 0) {
		framebase = 0;
		frametosp();
		if (listwanted())
			listing();
		return;
	}
	framebase = res;
	framerebase();
	if (listwanted())		/* the INS list as the encoder is about to see it */
		listing();
	if (res <= 16)
		outw(0xABF0 | (res-1));		/* DEC R15,#res				*/
	else {
		outw(0x030F);			/* SUB R15,#res				*/
		outw((unsigned short)res);
	}
	if (sv_cnt > 1) {
		outw(0x1CE9);			/* STM @RR14,Rrlo,#count		*/
		outw((sv_rlo<<8) | (sv_cnt-1));
	} else
		outw(0x2FED);			/* LD  @RR14,R13   save caller FP	*/
	outw(0xA1FD);				/* LD  R13,R15     FP = SP		*/
}

/*
 * Function epilogue: reload the callee-saved registers and the caller's frame pointer
 * from the bottom of the frame, release the frame, and return.  The stack pointer is
 * already at the bottom of the frame: every call pops its own arguments.
 *
 * A function whose frame was elided has nothing to undo: the stack pointer still
 * stands where the CALL left it and no register was saved.
 */
genepilog()
{
	if (noframe) {
		outw(0x9E08);			/* RET T				*/
		return;
	}
	if (sv_cnt > 1) {
		outw(0x1CE1);			/* LDM Rrlo,@RR14,#count		*/
		outw((sv_rlo<<8) | (sv_cnt-1));
	} else
		outw(0x21ED);			/* LD  R13,@RR14   restore caller FP	*/
	if (framebase <= 16)
		outw(0xA9F0 | (framebase-1));	/* INC R15,#framebase			*/
	else {
		outw(0x010F);			/* ADD R15,#framebase			*/
		outw((unsigned short)framebase);
	}
	outw(0x9E08);				/* RET T				*/
}

/*
 * The number of bytes the prologue reserves, for the listing.  genprolog settles it --
 * zero for a function whose frame is elided -- before listing() runs.
 */
framebytes()
{
	return (framebase);
}

/*
 * ALIGN: bring the location counter to a word boundary.
 */
genalign(align)
{
	if ((dot&01) == 0)
		return;
	if (dotseg == SBSS)
		++dot;
	else
		outab(0);
}

/*
 * BLOCK: reserve n bytes of zeroed storage.
 */
genblock(n)
register sizeof_t n;
{
	if (dotseg == SBSS)
		dot += n;
	else
		while (n--)
			outab(0);
}

/*
 * Reverse a Z8000 condition code (used by the branch optimizer): the complement of a
 * cc is cc ^ 8 (F<->T, LT<->GE, EQ<->NE, ULT<->UGE, ...).
 */
revrel(cc)
{
	return (cc ^ 8);
}

/*
 * Second pass (object mode).  work2()/outdone() wrote the finished l.out object to the
 * scratch file (ofp = argv[4]); the driver has reopened ifp = that scratch and ofp =
 * the real object (argv[3]).  Copy it across.
 */
copycode()
{
	register int	c;

	while ((c = getc(ifp)) != EOF)
		putc(c, ofp);
}
