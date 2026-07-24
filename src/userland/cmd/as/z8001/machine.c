/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Machine op decode and
 * format. This code handles both
 * flavours of Zilog Z-8000.
 */
#include "asm.h"

static	char	segflag;
#if SEGCPU
static	char	jpdasz = 6;
static	char	callsz = 6;
#else
static	char	jpdasz = 4;
static	char	callsz = 4;
#endif

machine(sp)
register struct sym *sp;
{
	switch (sp->s_kind) {		/* determine which to call */
	case S_SEGM:
	case S_EVEN:
	case S_ODD:
	case S_HALT:
	case S_R:
	case S_RR:
	case S_RET:
	case S_SC:
	case S_TCC:
	case S_DI:
	case S_FLG:
	case S_IND:
	case S_INDR:
	case S_TRT:
	case S_TRTR:
	case S_CPD:
	case S_CPS:
	case S_RL:
	case S_SDA:
	case S_EX:
	case S_RSRC:
	case S_JP:
	case S_CLR:
	case S_CALL:
	case S_DEC:
	case S_DJNZ:
	case S_BIT:
	case S_SRA:
	case S_SLA:
	case S_CP:
		machine1(sp);
		break;

	default:
		machine2(sp);
	}
}

machine1(sp)
register struct sym *sp;
{
	register op;
	char id[NCPLN];
	struct expr cnt, dst, src;
	int c, cc, disp;
	int addr, flag, kind, mask;
	int mode, type;

	op = sp->s_addr;
	flag = sp->s_flag&0x03;
	switch (kind = sp->s_kind) {

	case S_SEGM:
		segflag = sp->s_addr;
		if (segflag)
			jpdasz = callsz = 6; else
			jpdasz = callsz = 4;
		lmode = SLIST;
		break;

	case S_EVEN:
	case S_ODD:
		if (((int)dot->s_addr&01) == (int)sp->s_addr) {
			if (inbss == 0)
				outab(0);
			else
				++dot->s_addr;
		}
		lmode = ALIST;
		laddr = dot->s_addr;
		break;

	case S_HALT:
		outaw(op);
		break;

	case S_R:
		getaddr(&dst);
		outxx(op, &dst, ROK|UP4|flag);
		break;

	case S_RR:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) != R)
			aerr();
		outxx(op|rof(dst), &src, ROK|UP4);
		break;

	case S_RET:
		if (more())
			op |= code();
		else
			op |= UN;
		outaw(op);
		break;

	case S_SC:
		getaddr(&src);
		if (mof(src) != DA)
			aerr();
		outab(SCHIGH);
		outrb(&src, 0);
		break;

	case S_TCC:
		op |= code();
		comma();
		getaddr(&dst);
		outxx(op, &dst, ROK|UP4);
		break;

	case S_DI:
	case S_FLG:
		do {
			getid(id, -1);
			if ((sp = lookup(id, 0)) == NULL)
				aerr();
			else {
				mask = sp->s_addr;
				if (kind == S_DI) {
					if (sp->s_kind != S_IVN)
						aerr();
					op &= ~mask;
				} else {
					if (sp->s_kind != S_FLGN)
						aerr();
					op |= mask;
				}
			}
		} while ((c = getnb()) == ',');
		unget(c);
		outaw(op);
		break;

	case S_IND:
	case S_INDR:
		getaddr(&dst);
		comma();
		getaddr(&src);
		comma();
		getaddr(&cnt);
		if (mof(dst)!=IR || mof(src)!=IR || mof(cnt)!=R)
			aerr();
		outaw(op | (rof(src)<<4));
		op = sp->s_kind==S_IND ? 0x08 : 0x00;
		outaw(op | (rof(cnt)<<8) | (rof(dst)<<4));
		break;

	case S_TRT:
	case S_TRTR:
		getaddr(&dst);
		comma();
		getaddr(&src);
		comma();
		getaddr(&cnt);
		if (mof(dst)!=IR || mof(src)!=IR || mof(cnt)!=R)
			aerr();
		outaw(op | (rof(dst)<<4));
		op = sp->s_kind==S_TRT ? 0x00 : 0x0E;
		outaw(op | (rof(cnt)<<8) | (rof(src)<<4));
		break;

	case S_CPD:
	case S_CPS:
		getaddr(&dst);
		comma();
		getaddr(&src);
		comma();
		getaddr(&cnt);
		comma();
		cc = code();
		mode = (kind == S_CPD) ? R : IR;
		if (mof(dst)!=mode || mof(src)!=IR || mof(cnt)!=R)
			aerr();
		outaw(op | (rof(src)<<4));
		outaw((rof(cnt)<<8) | (rof(dst)<<4) | cc);
		break;

	case S_RL:
		getaddr(&dst);
		if (mof(dst) != R)
			aerr();
		if (more()) {
			if ((addr = getaim()) == 2)
				op |= RLBY2;
			else if (addr != 1)
				aerr();
		}
		outaw(op | (rof(dst)<<4));
		break;

	case S_SDA:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(src) != R)
			aerr();
		outxx(op, &dst, ROK|UP4);
		outaw(rof(src) << 8);
		break;

	case S_EX:
	case S_RSRC:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) != R)
			aerr();
		checkreg(rof(dst), flag);
		flag = ROK|IROK|DAOK|XOK|UP4;
		if (kind == S_RSRC)
			flag |= IMOK;
		if ((sp->s_flag&S_L) != 0)
			flag |= LONG;
		outxx(op|rof(dst), &src, flag);
		break;

	case S_JP:
		op |= optcode();
	case S_CLR:
	case S_CALL:
		getaddr(&dst);
		flag |= IROK|DAOK|XOK|UP4;
		if (kind == S_CLR)
			flag |= ROK; else
			flag |= IREF;
		outxx(op, &dst, flag);
		break;

	case S_DEC:
		getaddr(&dst);
		if (more()) {
			if ((addr = getaim())<1 || addr>16)
				aerr();
			op |= addr-1;
		}
		outxx(op, &dst, ROK|IROK|DAOK|XOK|UP4);
		break;

	case S_DJNZ:
		getaddr(&cnt);
		comma();
		getaddr(&dst);
		if (mof(cnt) != R)
			aerr();
		/*
		 * djnz/dbjnz is backward-only with a 7-bit word
		 * displacement (target within -252..+2 bytes).  When the
		 * target is out of that range - or in another segment -
		 * relax to the two-instruction equivalent
		 *	dec/decb cnt,$1 ; jp nz,dst
		 * just as jr relaxes to jp.  The fallback clobbers the
		 * flags that a bare djnz leaves untouched, but nothing that
		 * reads a loop-closing branch's flags survives it, and the
		 * relaxation only fires for loop bodies over ~252 bytes.
		 * The instruction is therefore span-dependent (short form 2
		 * bytes, fallback 2+jpdasz): pass 0 assumes the long form,
		 * pass 1 measures and files one branch-map bit, pass 2
		 * replays it - the same protocol jr uses.
		 */
		if (pass == 0)
			dot->s_addr += 2 + jpdasz;
		else if (pass == 1) {
			if (dst.e_type != E_DIR ||
			    dst.e_base.e_lp != dot->s_base.s_lp) {
				/* other segment: must use dec;jp */
				dot->s_addr += 2 + jpdasz;
				flag = 1;
			} else {
				if (dst.e_addr >= dot->s_addr)
					dst.e_addr -= fuzz;
				dot->s_addr += 2;
				disp = dot->s_addr - dst.e_addr;
				disp >>= 1;
				flag = 0;
				if (disp < 0 || disp > 127) {
					flag = 1;
					dot->s_addr += jpdasz;
				}
			}
			setbit(flag);
		} else if (getbit()) {
			/* out of range: dec cnt,$1 ; jp nz,dst */
			outaw(((op & 0x0080) ? 0x2B00 : 0x2A00) |
			    (rof(cnt) << 8));
			outxx(JP | 0x000E, &dst, DAOK|UP4|IREF);
		} else {
			disp = dot->s_addr + 2 - dst.e_addr;
			disp >>= 1;
			outaw(op | (rof(cnt) << 8) | disp);
		}
		break;

	case S_BIT:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(src) == IM) {
			if (src.e_type != E_ACON)
				aerr();
			addr = src.e_addr;
			outxx(op|addr, &dst, ROK|IROK|DAOK|XOK|UP4);
			break;
		}
		if (mof(src)==R && mof(dst)==R) {
			outaw(op | rof(src));
			outaw(rof(dst) << 8);
			break;
		}
		aerr();
		break;

	case S_SRA:
	case S_SLA:
		getaddr(&dst);
		addr = 1;
		if (more())
			addr = getaim();
		if (mof(dst) != R)
			aerr();
		if (kind == S_SRA)
			addr = -addr;
		outaw(op | (rof(dst)<<4));
		outaw(addr);
		break;

	case S_CP:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) == R) {
			checkreg(rof(dst), flag);
			flag = ROK|IMOK|IROK|DAOK|XOK|UP4;
			if (op == CPL)
				flag |= LONG;
			outxx(op|rof(dst), &src, flag);
			break;
		}
		if (op!=CPL && mof(src)==IM) {
			op = makeop(op, CPI);
			outxx(op, &dst, IROK|DAOK|XOK|UP4);
			outiw(op, &src);
			break;
		}
		aerr();
		break;

	default:
		err('?');
	}
}

machine2(sp)
register struct sym *sp;
{
	register op;
	char id[NCPLN];
	struct expr cnt, dst, src;
	int c, cc, disp;
	int addr, flag, kind, mask;
	int mode, type;


	op = sp->s_addr;
	flag = sp->s_flag&0x03;
	switch (kind = sp->s_kind) {
	case S_CALR:
		mchcalr();
		break;

	case S_IN:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) == R) {
			if (mof(src) == IR) {
				outaw(op | (rof(src)<<4) | rof(dst));
				break;
			}
			if (mof(src) == DA) {
				outaw(makeop(op, INDA) | (rof(dst)<<4));
				outrw(&src, 0);
				break;
			}
		}
		aerr();
		break;

	case S_SIN:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(src)!=DA || mof(dst)!=R)
			aerr();
		outaw(op | (rof(dst)<<4));
		outrw(&src, 0);
		break;

	case S_OUT:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(src) == R) {
			if (mof(dst) == IR) {
				outaw(op | (rof(dst)<<4) | rof(src));
				break;
			}
			if (mof(dst) == DA) {
				outaw(makeop(op, OUTDA) | (rof(src)<<4));
				outrw(&dst, 0);
				break;
			}
		}
		aerr();
		break;

	case S_SOUT:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst)!=DA || mof(src)!=R)
			aerr();
		outaw(op | (rof(src)<<4));
		outrw(&dst, 0);
		break;

	case S_LONG:
		getaddr(&src);
		if (mof(src) != DA)
			aerr();
		type = src.e_type;
		if (type==E_ACON || type==E_AREG) {
			outal(src.e_addr);
			break;
		}
		if (!segflag) {
			aerr();
			break;
		}
		if (type == E_ASEG) {
			outaw((src.e_base.e_segn&0177) << 8);
			outaw((unsigned)src.e_addr);
			break;
		}
		if (type == E_SEG) {
			dst = src;
			dst.e_addr = 0;
			outrw(&dst, 0);
			outaw((unsigned)src.e_addr);
			break;
		}
		outrl(&src, 0, 0x00);
		break;

	case S_POP:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(src) != IR)
			aerr();
		flag |= ROK|IROK|DAOK|XOK;
		outxx(op|(rof(src)<<4), &dst, flag);
		break;

	case S_PUSH:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) != IR)
			aerr();
		if (op == PUSHW && mof(src) == IM) {
			outaw(PUSHI | (rof(dst)<<4));
			outrw(&src, 0);
			break;
		}
		flag |= ROK|IROK|DAOK|XOK;
		outxx(op|(rof(dst)<<4), &src, flag);
		break;

	case S_LDM:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if ((addr = getaim() - 1) < 0 || addr > 15) {
			aerr();
			break;
		}
		if (mof(dst) == R)
			outldm(LDMLD, rof(dst), &src, addr);
		else if (mof(src) == R)
			outldm(LDMST, rof(src), &dst, addr);
		else
			aerr();
		break;

	case S_LDR:
	case S_LDAR:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) != R)
			aerr();
		notxseg(&src);
		disp = src.e_addr - dot->s_addr - 4;
		outaw(op | rof(dst));
		outaw(disp);
		break;
		
	case S_JR:
		op |= optcode() << 8;
		getaddr(&dst);
		notxseg(&dst);
		if (pass == 0)
			dot->s_addr += jpdasz;
		else if (pass == 1) {
			if (dst.e_type != E_DIR
			 || dst.e_base.e_lp != dot->s_base.s_lp) {
				/* Doesn't reach with jr */
				dot->s_addr += jpdasz;
				flag = 1;
			} else {
				if (dst.e_addr >= dot->s_addr)
					dst.e_addr -= fuzz;
				dot->s_addr += JRSZ;
				disp = dst.e_addr - dot->s_addr;
				disp >>= 1;
				flag = 0;
				if (disp<-128 || disp>127) {
					flag = 1;
					dot->s_addr += jpdasz-JRSZ;
				}
			}
			setbit(flag);
		} else if (getbit()) {
			/* Use jp */
			op = JP | ((op>>8)&0x0F);
			outxx(op, &dst, DAOK|UP4|IREF);
		} else {
			disp = dst.e_addr - dot->s_addr - 2;
			disp >>= 1;
			outaw(op | (disp&0377));
		}
		break;

	case S_LDA:
		getaddr(&dst);
		comma();
		getaddr(&src);
		if (mof(dst) != R)
			aerr();
		if (mof(src) == BX) {
			outbx(LDABX, &dst, &src);
			break;
		}
		if (mof(src) == BA) {
			outba(LDABA, &dst, &src);
			break;
		}
		outxx(LDA|rof(dst), &src, DAOK|XOK|UP4);
		break;

	case S_LDK:
		getaddr(&dst);
		if ((addr = getaim())>=0 && addr<=15) {
			outxx(op|addr, &dst, ROK|UP4);
			break;
		}
		aerr();
		break;

	case S_LD:
		mchld(op, flag);
		break;

	case S_CTL:
		mchctl(op);
		break;

	default:
		err('o');
	}
}

/*
 * Format calr instructions.
 */
mchcalr()
{
	struct expr dst;
	register flag, disp;

	getaddr(&dst);
	if (mof(dst) != DA)
		aerr();
	if (pass == 0)
		dot->s_addr += callsz;
	else if (pass == 1) {
		if (dst.e_type != E_DIR
		||  dst.e_base.e_lp != dot->s_base.s_lp) {
			/* hard */
			dot->s_addr += callsz;
			flag = 1;
		} else {
			if (dst.e_addr >= dot->s_addr)
				dst.e_addr -= fuzz;
			dot->s_addr += CALRSZ;
			disp = dot->s_addr + 2 - dst.e_addr;
			disp >>= 1;
			flag = 0;
			if (disp<-2048 || disp>2047) {
				/* doesn't reach */
				++flag;
				dot->s_addr += callsz-CALRSZ;
			}
		}
		setbit(flag);
	} else if (getbit())
		outxx(CALL, &dst, DAOK|UP4|IREF);
	else {
		disp = dot->s_addr + 2 - dst.e_addr;
		disp >>= 1;
		outaw(CALR | (disp&07777));
	}
}

/*
 * Format ld instructions.
 */
mchld(op, flag)
register op;
register flag;
{
	struct expr src, dst;
	register addr;

	getaddr(&dst);
	comma();
	getaddr(&src);
	if (op!=LDL && mof(dst)==R && isaim(src)) {
		addr = src.e_addr;
		if (op == LDB) {
			outaw(LDBI | (rof(dst)<<8) | (addr&0377));
			return;
		}
		if (op==LDW && isk(addr)) {
			outaw(LDK | (rof(dst)<<4) | addr);
			return;
		}
	}
	if (mof(dst) == R) {
		checkreg(rof(dst), flag);
		flag = ROK|IROK|IMOK|DAOK|XOK|UP4;
		if (op==LDL && mof(src)==IM)
			flag |= LONG;
		if (mof(src) == BA) {
			op = (op == LDL) ? LDLBA : makeop(op, LDBA);
			outba(op, &dst, &src);
			return;
		}
		if (mof(src) == BX) {
			op = (op == LDL) ? LDLBX : makeop(op, LDBX);
			outbx(op, &dst, &src);
			return;
		}
		outxx(op|rof(dst), &src, flag);
		return;
	}
	if (mof(src) == R) {
		checkreg(rof(src), flag);
		if (mof(dst) == BA) {
			op = (op == LDL) ? STLBA : makeop(op, STBA);
			outba(op, &src, &dst);
			return;
		}
		if (mof(dst) == BX) {
			op = (op == LDL) ? STLBX : makeop(op, STBX);
			outbx(op, &src, &dst);
			return;
		}
		op = (op == LDL) ? STL : makeop(op, ST);
		outxx(op|rof(src), &dst, ROK|DAOK|IROK|XOK|UP4);
		return;
	}
	if (op!=LDL && mof(src)==IM) {
		op = makeop(op, LDI);
		outxx(op, &dst, IROK|DAOK|XOK|UP4);
		outiw(op, &src);
		return;
	}
	aerr();
}

/*
 * Format ldctl instructions.
 */
mchctl(op)
register op;
{
	struct expr src, dst;
	register addr, ctlr;

	getaddr(&dst);
	comma();
	getaddr(&src);
	if (mof(dst)==R && mof(src)==DA) {
		ctlr = rof(dst);
		addr = src.e_addr;
	} else if (mof(dst)==DA && mof(src)==R) {
		addr = dst.e_addr;
		ctlr = rof(src);
	} else
		aerr();
	if (op == LDCTLB) {
		if (addr != FLAGS)
			aerr();
	} else {
		if (addr == FLAGS)
			aerr();
	}
	if (mof(dst) == DA)
		addr |= TOCTLR;
	outaw(op | (ctlr<<4) | addr);
}

/*
 * Output ldm instructions.
 * These have their own, somewhat unique
 * format. The second opcode word is placed
 * before the DA or X address word.
 */
outldm(op, reg, mesp, nreg)
register op;
struct expr *mesp;
{
	register mode;

	mode = mofp(mesp);
	if (mode!=IR && mode!=DA && mode!=X)
		aerr();
	if (mode==DA || mode==X)
		op |= mbits(0, 1);
	if (mode==IR || mode==X)
		op |= rofp(mesp) << 4;
	outaw(op);
	outaw((reg<<8) | nreg);
	if (mode==DA || mode==X)
		outsof(mesp, 1);
}

/*
 * Output an immediate word.
 * Replicate bytes if the op is a
 * byte instruction.
 */
outiw(op, esp)
register struct expr *esp;
{
	if ((op&W) != 0)
		outrw(esp, 0);
	else {
		outrb(esp, 0);
		outrb(esp, 0);
	}
}

/*
 * Get absolute immediate.
 * Used by shifts, by `inc' and `dec',
 * et al.
 */
getaim()
{
	struct expr src;

	comma();
	getaddr(&src);
	if (!isaim(src))
		aerr();
	return (src.e_addr);
}

/*
 * The next input character
 * must be a comma or it is a dreadful
 * error.
 */
comma()
{
	if (getnb() != ',')
		qerr();
}

/*
 * Read a condition code item.
 */
code()
{
	register struct sym *sp;
	int id[NCPLN];

	getid(id, -1);
	if ((sp=lookup(id, 0))==NULL || sp->s_kind!=S_CC) {
		aerr();
		return (0);
	}
	return ((int) sp->s_addr);
}

/*
 * Read an optional condition code.
 * If there, check for the comma. If not, do
 * nothing except return UN.
 */
optcode()
{
	register struct sym *sp;
	register char *sip;
	register c;
	char id[NCPLN];

	sip = ip;
	if (ctype[c = getnb()] == LETTER) {
		getid(id, c);
		if ((sp=lookup(id, 0))!=NULL && sp->s_kind==S_CC) {
			comma();
			return (sp->s_addr);
		}
	}
	ip = sip;
	return (UN);
}

/*
 * Read an address expression.
 * Pack its value into the supplied
 * expression structure. Return the
 * address mode to the caller.
 */
getaddr(esp)
register struct expr *esp;
{
	register c, t;
	struct expr reg;

	if ((c=getnb()) == '$') {
		expr(esp, 0);
		esp->e_mode = IM;
		return;
	}
	if (c=='(' || c=='@') {
		expr(esp, 0);
		if (c=='(' && getnb()!=')')
			qerr();
		if (esp->e_type!=E_AREG || esp->e_addr==0)
			aerr();
		esp->e_mode = IR + (int)esp->e_addr;
		return;
	}
	unget(c);
	expr(esp, 0);
	if ((c = getnb()) == '(') {
		expr(&reg, 0);
		if (getnb() != ')')
			qerr();
		if (esp->e_type==E_AREG && reg.e_type!=E_AREG) {
			t = BA + (int)esp->e_addr;
			*esp = reg;
			esp->e_mode = t;
			return;
		}
		if (reg.e_type!=E_AREG || reg.e_addr==0)
			aerr();
		t = esp->e_type;
		if (t == E_AREG) {
			esp->e_mode = BX + (int)reg.e_addr;
			return;
		}
		esp->e_mode = X + (int)reg.e_addr;
		return;
	}
	unget(c);
	if ((t = esp->e_type) == E_AREG) {
		esp->e_mode = R + (int)esp->e_addr;
		return;
	}
	esp->e_mode = DA;
}

/*
 * Output a BA format instruction.
 */
outba(op, dp, sp)
register struct expr *dp, *sp;
{
	outaw(op | (dp->e_mode&017) | ((sp->e_mode&017)<<4));
	outrw(sp, 0);
}

/*
 * Output a BX format instruction.
 */
outbx(op, dp, sp)
register struct expr *dp, *sp;
{
	outaw(op | (dp->e_mode&017) | ((sp->e_mode&017)<<4));
	outaw((int)sp->e_addr << 8);
}

/*
 * General op formatting.
 * Handles immediates, two sizes of
 * segment offsets, etc.
 * BA and BX modes must be handled
 * elsewhere so should never come here.
 */
outxx(op, esp, f)
register struct expr *esp;
register f;
{
	register r;
	int m;

	r = esp->e_mode&017;
	m = esp->e_mode&~017;
	switch (m) {

	case R:
		if ((f&ROK) == 0)
			aerr();
		checkreg(r, f);
		op |= mbits(1, 0);
		break;

	case IR:
		if ((f&IROK) == 0)
			aerr();
		if (segflag)
			checkreg(r, S_2);
		op |= mbits(0, 0);
		break;

	case IM:
		if ((f&IMOK) == 0)
			aerr();
		op |= mbits(0, 0);
		r = 0;
		break;

	case X:
		if ((f&XOK) == 0)
			aerr();
		op |= mbits(0, 1);
		break;

	case DA:
		if ((f&DAOK) == 0)
			aerr();
		op |= mbits(0, 1);
		r = 0;
		break;

	case BA:
	case BX:
		aerr();

	default:
		fprintf(stderr, "Bad mode!\n");
	}
	if ((f&UP4) != 0)
		r <<= 4;
	outaw(op | r);
	if (m == IM) {
		if ((f&LONG) != 0) {
			if (esp->e_type == E_ACON)
				outal(esp->e_addr); else
				outrl(esp, 0, 0x00);
			return;
		}
		/*
		 * Replicate bytes for byte
		 * immediate ops.
		 */
		if ((op&W) == 0) {
			outrb(esp, 0);
			outrb(esp, 0);
			return;
		}
		outrw(esp, 0);
		return;
	}
	if (m==DA || m==X)
		outsof(esp, (f&IREF)==0);
}

/* 
 * Put out a segment and offset,
 * adjusting for segmented/nonsegmented
 * modes of the machine.
 * `df' is the D space flag.
 */
outsof(esp, df)
register struct expr *esp;
int df;
{
	static struct expr tesp;
	register int of, sn;

	if (segflag) {
		if (esp->e_type == E_ASEG) {
			sn = esp->e_base.e_segn;
			of = esp->e_addr;
			if (of>=0 && of<=255) {
				outaw(((sn&0x7F)<<8) | of);
				return;
			}
			outaw((1<<15) | (sn<<8));
			outaw(of);
			return;
		}
		if (esp->e_type == E_SEG) {
			tesp = *esp;
			tesp.e_addr = 0;
			of = esp->e_addr;
			if (of>=0 && of<=255) {
				outrb(&tesp, 0);
				outab(of);
				return;
			}
			/*
			 * Long form: like the E_ASEG case above, the first
			 * word must carry the long-address flag and the
			 * segment in its HIGH byte (0x80|segment, low byte
			 * reserved), then the 16-bit offset follows.  The
			 * segment number arrives by byte relocation, so put
			 * the 0x80 flag in the relocated byte's addend.
			 * (The original code set the flag on the caller's
			 * expr and then emitted the zeroed copy as a word,
			 * losing the flag entirely; the CPU then decoded
			 * the short one-word form and executed the offset
			 * word as an instruction.)
			 */
			tesp.e_addr = 0x80;
			outrb(&tesp, 0);
			outab(0);
			outaw(of);
			return;
		}
		outrl(esp, 0, 0x80);
	} else
		outrw(esp, 0);
}

/*
 * Branch map.
 */
#define	NB	128

int	bb[NB];
int	*bp;
int	bm;

minit()
{
	segflag = SEGCPU;
	bp = bb;
	bm = 01;
}

setbit(b)
{
	if (bp < &bb[NB]) {
		if (b)
			*bp |= bm;
		bm <<= 1;
		if (bm == 0) {
			++bp;
			bm = 01;
		}
	}
}

getbit()
{
	register f;

	if (bp >= &bb[NB])
		return (1);
	f = 0;
	if (*bp & bm)
		++f;
	if ((bm <<= 1) == 0) {
		++bp;
		bm = 01;
	}
	return (f);
}

/*
 * Give an `a' error if the specified
 * address is not in the same segment as
 * the current location. Used to check
 * the addresses of relative branches.
 */
notxseg(esp)
register struct expr *esp;
{
	if ((esp->e_mode&~017)!=DA ||
	esp->e_type!=E_DIR ||
	esp->e_base.e_lp!=dot->s_base.s_lp)
		aerr();
}

locinit()
{
	struct loc *locdef();

	defloc = locdef(".shri", L_SHRI);
	locdef(".prvi", L_PRVI);
	locdef(".bssi", L_BSSI);
	locdef(".shrd", L_SHRD);
	locdef(".prvd", L_PRVD);
	locdef(".bssd", L_BSSD);
	locdef(".strn", L_PRVD);
	locdef(".symt",	L_DEBUG);
	nloc = NLSEG;
}

struct loc *
locdef(cp, t)
char *cp;
{
	register char *cp1, *cp2;
	register struct loc *lp;
	struct sym *sp;
	struct loc *lp1, *lp2;
	int c;
	char id[NCPLN];

	lp = (struct loc *) new(sizeof(struct loc));
	lp->l_seg = t;
	lp->l_lp = NULL;
	lp->l_fuzz = 0;
	lp->l_break = 0;
	lp->l_offset = 0;
	lp1 = NULL;
	lp2 = loc[t];
	while (lp2 != NULL) {
		lp1 = lp2;
		lp2 = lp2->l_lp;
	}
	if (lp1 == NULL)
		loc[t] = lp; else
		lp1->l_lp = lp;
	cp1 = cp;
	cp2 = id;
	while (c = *cp1++)
		if (cp2 < &id[NCPLN])
			*cp2++ = c;
	while (cp2 < &id[NCPLN])
		*cp2++ = 0;
	sp = lookup(id, 1);
	sp->s_kind = S_LOC;
	sp->s_flag = 0;
	sp->s_addr = (address) lp;	/* Hide pointer in address (ugh) */
	return (lp);
}

checkreg(r, f)
register int r, f;
{
	if ((r & f & 03) != 0)
		aerr();
}
