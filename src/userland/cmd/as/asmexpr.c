#include "asm.h"

expr(esp, n)
register struct expr *esp;
{
	register c, p;
	address lv, rv;
	int lt, rt;
	struct expr re;

	term(esp);
	for (;;) {
		c = getnb();
		if (ctype[c] != BINOP)
			break;
		if (c == '|')
			p = 10;
		else if (c == '*')
			p = 9;
		else
			p = 8;
		if (p <= n)
			break;
		expr(&re, p);
		lt = esp->e_type;
		lv = esp->e_addr;
		rt = re.e_type;
		rv = re.e_addr;
		if (c == '|') {
			if (rt != E_ACON)
				aerr();
			switch (lt) {
			case E_ACON:
				esp->e_type = E_ASEG;
				esp->e_base.e_segn = lv;
				break;

			case E_SYM:
				esp->e_type = E_SEG;
				break;

			default:
				aerr();
			}
			esp->e_addr = rv;
			continue;
		}
		if (c == '^') {
			if ((esp->e_type = rt) == E_DIR)
				esp->e_base.e_lp = re.e_base.e_lp;
			else if (rt==E_SYM || rt==E_SEG)
				esp->e_base.e_sp = re.e_base.e_sp;
			else if (rt == E_ASEG)
				esp->e_base.e_segn = re.e_base.e_segn;
			continue;
		}
		if (c == '+') {
			if (lt == E_ACON) {
				esp->e_type = rt;
				if (rt == E_ASEG)
					esp->e_base.e_segn = re.e_base.e_segn;
				else if (rt == E_DIR)
					esp->e_base.e_lp = re.e_base.e_lp;
				else if (rt==E_SYM || rt==E_SEG)
					esp->e_base.e_sp = re.e_base.e_sp;
			} else if (rt != E_ACON)
				rerr();
			esp->e_addr += rv;
			continue;
		}
		if (c == '-') {
			if (lt == E_ACON) {
				if (rt != E_ACON)
					rerr();
			} else if (rt != E_ACON) {
				if (lt!=E_DIR || rt!=E_DIR || 
				    esp->e_base.e_lp!=re.e_base.e_lp)
					rerr();
				esp->e_type = E_ACON;
			}
			esp->e_addr -= rv;
			continue;
		}
		if (c == '*') {
			if (lt!=E_ACON || rt!=E_ACON)
				aerr();
			esp->e_addr *= rv;
			continue;
		}
		fprintf(stderr, "Internal error, c=%d in expr.\n", c);
		exit( 1);
	}
	unget(c);
}

address
absexpr()
{
	struct expr e;

	expr(&e, 0);
	abscheck(&e);
	return (e.e_addr);
}

term(esp)
register struct expr *esp;
{
	register c;
	address  n;
	char id[NCPLN];
	struct sym  *sp;
	struct tsym *tp;
	int r, v;

	if ((c=getnb()) == '[') {
		expr(esp, 0);
		if (getnb() != ']')
			qerr();
		return;
	}
	if (c == '-') {
		expr(esp, 100);
		abscheck(esp);
		esp->e_addr = -esp->e_addr;
		return;
	}
	if (c == '~') {
		expr(esp, 100);
		abscheck(esp);
		esp->e_addr = ~esp->e_addr;
		return;
	}
	if (c == '\'') {
		esp->e_type = E_ACON;
		esp->e_addr = getmap(-1);
		return;
	}
	esp->e_type = E_ACON;
	esp->e_addr = 0;
	if (ctype[c] == DIGIT) {
		r = 10;
		if (c == '0') {
			r = 8;
			if ((c = *ip) != 0)
				++ip;
			if (c == 'x') {
				r = 16;
				if ((c = *ip) != 0)
					++ip;
			}
		}
		n = 0;
		while ((v=digit(c, r)) >= 0) {
			n = r*n + v;
			if ((c = *ip) != 0)
				++ip;
		}
		if (c=='f' || c=='b') {
			if (n < 10) {
				if (c == 'f')
					tp = tsymp[n].tp_fp; else
					tp = tsymp[n].tp_bp;
				if (tp != NULL) {
					esp->e_type = E_DIR;
					esp->e_base.e_lp = tp->t_lp;
					esp->e_addr = tp->t_addr;
					return;
				}
			}
			err('u');
			return;
		}
		if (c != 0)
			--ip;
		esp->e_addr = n;
		return;
	}
	if (ctype[c] == LETTER) {
		getid(id, c);
		if ((sp=lookup(id, gflag)) != NULL) {
			if (sp->s_kind == S_NEW) {
				if ((sp->s_flag&S_GBL) != 0) {
					esp->e_type = E_SYM;
					esp->e_base.e_sp = sp;
					return;
				}
				uerr(id);
				return;
			}
			esp->e_type = sp->s_type;
			esp->e_addr = sp->s_addr;
			if (sp->s_type == E_ASEG)
				esp->e_base.e_segn = sp->s_base.s_segn;
			else if (sp->s_type == E_DIR)
				esp->e_base.e_lp = sp->s_base.s_lp;
			else if (sp->s_type==E_SYM || sp->s_type==E_SEG)
				esp->e_base.e_sp = sp->s_base.s_sp;
			return;
		}
		uerr(id);
		return;
	}
	qerr();
}

digit(c, r)
register c, r;
{
	if (r == 16) {
		if (c>='A' && c<='F')
			return (c-'A'+10);
		if (c>='a' && c<='f')
			return (c-'a'+10);
	}
	if (c>='0' && c<='9')
		return (c - '0');
	return (-1);
}

abscheck(esp)
register struct expr *esp;
{
	if (esp->e_type != E_ACON) {
		rerr();
		esp->e_type = E_ACON;
	}
}
