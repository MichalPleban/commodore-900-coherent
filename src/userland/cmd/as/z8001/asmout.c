/*
 * Output routines.
 * 16 bit.
 * Non segmented.
 */
#include "asm.h"
#include <canon.h>

#define NTXT	128
#define NREL	128

#define outlst(b)	{ if (cp < &cb[NCODE]) *cp++ = (b); }

address	txtla;
long	crbase;
long	crseek;
struct	ldheader ldh;
struct	loc *txtlp;
char	txt[NTXT];
char	rel[NREL];
char	*txtp	= { txt };
char	*relp	= { rel };

outab(b)
{
	if (pass == 2) {
		if (inbss)
			err('s');
		else {
			outlst(b);
			outchk(1, 0);
			*txtp++ = b;
		}
	}
	++dot->s_addr;
}

outaw(w)
{
	if (pass == 2) {
		if (inbss)
			err('s');
		else {
			outlst(fbyte(w));
			outlst(sbyte(w));
			outchk(2, 0);
			*txtp++ = fbyte(w);
			*txtp++ = sbyte(w);
		}
	}
	dot->s_addr += 2;
}

outal(l)
long	l;
{
	if (pass==2) {
		if (inbss)
			err('s');
		else {
			outlst(fbyte(fword(l)));
			outlst(sbyte(fword(l)));
			outlst(fbyte(sword(l)));
			outlst(sbyte(sword(l)));
			outchk(4, 0);
			*txtp++ = fbyte(fword(l));
			*txtp++ = sbyte(fword(l));
			*txtp++ = fbyte(sword(l));
			*txtp++ = sbyte(sword(l));
		}
	}
	dot->s_addr += 4;
}

outrb(esp, pcrf)
register struct expr *esp;
{
	register address a;
	register n;
	int t;

	if (pass == 2) {
		if (inbss)
			err('s');
		else {
			t = esp->e_type;
			if (t==E_AREG || t==E_ASEG)
				t = E_ACON;
			if (t == E_SEG)
				t = E_SYM;
			a = esp->e_addr;
			if (pcrf)
				a -= dot->s_addr+1;
			outlst(a);
			if (t == E_ACON) {
				n = 0;
				if (pcrf)
					n = 5;
				outchk(1, n);
				*txtp++ = a;
				if (pcrf)
					outrel(LR_BYTE|LR_PCR|L_ABS,
					    dot->s_addr);
			} else {
				n = 5;
				if (t == E_SYM)
					n = 7;
				outchk(1, n);
				*txtp++ = a;
				n = LR_BYTE;
				if (pcrf)
					n |= LR_PCR;
				if (t == E_SYM)
					n |= L_SYM;
				else
					n |= esp->e_base.e_lp->l_seg;
				outrel(n, dot->s_addr);
				if (t == E_SYM) {
					n = esp->e_base.e_sp->s_ref;
					*relp++ = sbyte(n);
					*relp++ = fbyte(n);
				}
			}
		}
	}
	++dot->s_addr;
}

outrw(esp, pcrf)
register struct expr *esp;
{
	register address a;
	register n;
	int t;

	if (pass == 2) {
		if (inbss)
			err('s');
		else {
			t = esp->e_type;
			if (t==E_AREG || t==E_ASEG)
				t = E_ACON;
			if (t == E_SEG)
				t = E_SYM;
			a = esp->e_addr;
			if (pcrf)
				a -= dot->s_addr+2;
			outlst(fbyte(a));
			outlst(sbyte(a));
			if (t == E_ACON) {
				n = 0;
				if (pcrf)
					n = 5;
				outchk(2, n);
				*txtp++ = fbyte(a);
				*txtp++ = sbyte(a);
				if (pcrf)
					outrel(LR_WORD|LR_PCR|L_ABS,
					    dot->s_addr);
			} else {
				n = 3;
				if (t == E_SYM)
					n = 5;
				outchk(2, n);
				*txtp++ = fbyte(a);
				*txtp++ = sbyte(a);
				n = LR_WORD;
				if (pcrf)
					n |= LR_PCR;
				if (t == E_SYM)
					n |= L_SYM;
				else
					n |= esp->e_base.e_lp->l_seg;
				outrel(n, dot->s_addr);
				if (t == E_SYM) {
					n = esp->e_base.e_sp->s_ref;
					*relp++ = sbyte(n);
					*relp++ = fbyte(n);
				}
			}
		}
	}
	dot->s_addr += 2;
}

/*
 * Output a relocatable long.
 * Bits should be 0x80 if long segment form
 * much be generated, 0x00 otherwise.
 */
outrl(esp, pcrf, bits)
register struct expr *esp;
int pcrf, bits;
{
	register address a;
	register n;
	int t;

	if (pass == 2) {
		if (inbss)
			err('s');
		else {
			t = esp->e_type;
			if (t==E_AREG || t==E_ASEG)
				t = E_ACON;
			if (t == E_SEG)
				t = E_SYM;
			a = esp->e_addr;
			if (pcrf)
				a -= dot->s_addr+2;
			a += (long)bits<<24;
			outlst(fbyte(fword(a)));
			outlst(sbyte(fword(a)));
			outlst(fbyte(sword(a)));
			outlst(sbyte(sword(a)));
			if (t == E_ACON) {
				n = 0;
				if (pcrf)
					n = 5;
				outchk(2, n);
				*txtp++ = fbyte(fword(a));
				*txtp++ = sbyte(fword(a));
				*txtp++ = fbyte(sword(a));
				*txtp++ = sbyte(sword(a));
				if (pcrf)
					outrel(LR_LONG|LR_PCR|L_ABS,
					    dot->s_addr);
			} else {
				n = 3;
				if (t == E_SYM)
					n = 5;
				outchk(2, n);
				*txtp++ = fbyte(fword(a));
				*txtp++ = sbyte(fword(a));
				*txtp++ = fbyte(sword(a));
				*txtp++ = sbyte(sword(a));
				n = LR_LONG;
				if (pcrf)
					n |= LR_PCR;
				if (t == E_SYM)
					n |= L_SYM;
				else
					n |= esp->e_base.e_lp->l_seg;
				outrel(n, dot->s_addr);
				if (t == E_SYM) {
					n = esp->e_base.e_sp->s_ref;
					*relp++ = sbyte(n);
					*relp++ = fbyte(n);
				}
			}
		}
	}
	dot->s_addr += 4;
}

/*
 * Make sure there is room in
 * the buffers for `nt' bytes worth
 * text and `nr' bytes of rel.
 * If not, write the buffers out
 * to the file.
 */
outchk(nt, nr)
{
	register unsigned n;
	register tn;
	long ts;

	if (txtp+nt>&txt[NTXT] || relp+nr>&rel[NREL]) {
		if ((n = txtp-txt) != 0) {
			ts = txtla + sizeof(ldh);
			tn = txtlp->l_seg;
			if (tn > L_BSSI) {
				ts -= ldh.l_ssize[L_BSSI];
				if (tn > L_BSSD)
					ts -= ldh.l_ssize[L_BSSD];
			}
			fseek(ofp, ts, 0);
			xwrite(txt, sizeof(char), n);
			if ((n = relp-rel) != 0) {
				fseek(ofp, crseek, 0);
				xwrite(rel, sizeof(char), n);
				crseek += n;
				relp = rel;
			}
		}
		txtp = txt;
	}
	if (txtp == txt) {
		txtla = dot->s_addr;
		txtlp = dot->s_base.s_lp;
	}
}

/*
 * Output a relocation record.
 */
outrel(op, addr)
int op;
address addr;
{
	*relp++ = op;
	*relp++ = sbyte(fword(addr));
	*relp++ = fbyte(fword(addr));
	*relp++ = sbyte(sword(addr));
	*relp++ = fbyte(sword(addr));
}

outinit()
{
	register struct loc *lp;
	register struct sym *sp;
	register i;
	struct ldsym lds;
	int rn;
	long sb, ss;

	ldh.l_magic = L_MAGIC;
#if LADDR
	ldh.l_flag = LF_32;
	ldh.l_tbase = sizeof(ldh);
#else
	ldh.l_flag = 0;
#endif
	ldh.l_machine = M_MACHINE;
	ldh.l_entry = 0;
	sb = sizeof(ldh);
	for (i=0; i<nloc; ++i) {
		ss = 0;
		lp = loc[i];
		while (lp != NULL) {
			ss += locrup(lp->l_break);
			lp = lp->l_lp;
		}
		ldh.l_ssize[i] = ss;
		if (i!=L_BSSI && i!=L_BSSD)
			sb += ss;
	}
	fseek(ofp, sb, 0);
	ss = 0;
	rn = 0;
	for (i=0; i<NHASH; ++i) {
		sp = symhash[i];
		while (sp != NULL) {
			if ((sp->s_flag&S_SYMT) != 0
			&& (xflag == 0
			|| (sp->s_flag&S_GBL) != 0
			||  sp->s_id[0] != 'L')) {
				sp->s_ref = rn++;
				symcopy(lds.ls_id, sp->s_id);
				if (sp->s_kind == S_NEW)
					lds.ls_type = L_REF;
				else if (sp->s_type != E_DIR)
					lds.ls_type = L_ABS;
				else
					lds.ls_type = sp->s_base.s_lp->l_seg;
				if ((sp->s_flag&S_GBL) != 0)
					lds.ls_type |= L_GLOBAL;
				lds.ls_addr = sp->s_addr;
				canint(lds.ls_type);
#if LADDR
				canlong(lds.ls_addr);
#else
				canvaddr(lds.ls_addr);
#endif
				xwrite(&lds, sizeof(lds), 1);
				ss += sizeof(lds);
			}
			sp = sp->s_sp;
		}
	}
	ldh.l_ssize[L_SYM] = ss;
	crseek = sb+ss;
	crbase = crseek;
}

/*
 * Finish up l.out
 */
outfinish()
{
	int	i;

	ldh.l_ssize[L_REL] = crseek-crbase;
	canshort(ldh.l_magic);
	canshort(ldh.l_flag);
	canshort(ldh.l_machine);
#if LADDR
	canshort(ldh.l_tbase);
	canlong(ldh.l_entry);
#else
	canvaddr(ldh.l_entry);
#endif
	for (i=0; i<NLSEG; i++)
		cansize(ldh.l_ssize[i]);
	fseek(ofp, (long)0, 0);
	xwrite(&ldh, sizeof(ldh), 1);
	fclose(ofp);
}

/*
 * Write code file.
 * Check for any errors.
 */
xwrite(p, s, n)
char *p;
{
	if (fwrite(p, s, n, ofp) != n) {
		fprintf(stderr, "%s: I/O error.\n", ofn);
		exit(1);
	}
}
