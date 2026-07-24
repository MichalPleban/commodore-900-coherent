/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Input handling.
 */
#include <stdio.h>
#include <ctype.h>
#include "roff.h"
#include "code.h"
#include "env.h"
#include "esc.h"
#include "div.h"
#include "reg.h"
#include "str.h"

/*
 * Get a character handling escapes.
 */
getf(flags)
{
	register REG *rp;
	register int c, n;
	char charbuf[CBFSIZE], name[2];

	escflag = 0;
	for (;;) {
		if ((c=getl(flags&1)) == esc) {
			if ((c=getl(flags&1)) == EOF)
				return (c);
			if (esctab[c] == 0) {
				escflag = 1;
				return (c);
			}
			c = esctab[c] & 0377;
		}
		switch (c) {
		case EIGN:
			continue;
		case ECOM:
			while ((c=getl(0)) != '\n')
				;
			return (c);
		case EARG:
			c = getf(0);
			if (!isdigit(c)) {
				printe("Bad argument reference");
				break;
			}
			if (strp->s_argp[c-='0'])
				adscore(strp->s_argp[c]);
			continue;
		case ESTR:
			if ((c=getf(0)) != '(') {
				name[0] = c;
				name[1] = '\0';
			} else {
				name[0] = getf(0);
				name[1] = getf(0);
			}
			if ((rp=findreg(name, RTEXT)))
				adstreg(rp);
			continue;
		case ENUM:
			n = 0;
			if ((c=getf(0)) == '+') {
				n = 1;
				c = getf(0);
			} else if (c == '-') {
				n = -1;
				c = getf(0);
			}
			if (c != '(') {
				name[0] = c;
				name[1] = '\0';
			} else {
				name[0] = getf(0);
				name[1] = getf(0);
			}
			if ((rp=findreg(name, RNUMR)) == NULL)
				spcnreg(name);
			else {
				rp->r_nval += n*rp->r_incr;
				adsnval(rp->r_nval, rp->r_form);
			}
			continue;
		case EWID:
			if (flags&2)		/* Copy mode */
				return (c);
			if (scandel(charbuf, CBFSIZE) == 0) {
				adsnval(0, '1');
			} else {
				adsnval(getwidth(charbuf), '1');
			}
			continue;
		case EEND:
			if (ifeflag == 0)
				continue;
			return (c);
		default:
			return (c);
		}
	}
}

/*
 * Get width of string by processing into temporary environment.
 */
int
getwidth(cp)
char	*cp;
{
	ENV	savenv;
	int	n;

	savenv = env;		/* gigantic block copy */
	setline();
	ind = 0;
	tif = 0;
	fil = 0;
	adscore(cp);
	ccc = 0;
	strp->s_eoff = 1;
	process();
	wordbreak(DNULL);
	n = nlinsiz;
	env = savenv;
	return (n);
}

/*
 * Given a buffer pointer, `bp', and the size of the buffer, `n',
 * get an argument surrounded by delimeters and store it in the
 * buffer.
 */
scandel(cp, n)
register char *cp;
{
	int r;
	register int c;
	register int endc;

	r = 1;
	endc = getf(0);
	while ((c=getf(0)) != endc) {
		if (c == '\n') {
			printe("Syntax error");
			return (0);
		}
		if (--n == 0) {
			printe("Delimeter argument too large");
			r = 0;
		} else if (n > 0)
			*cp++ = c;
	}
	*cp++ = '\0';
	return (r);
}

/*
 * Get a character from the current input source.
 */
getl(eofflag)
{
	register STR *sp;
	int n;
	register int c;
	register char *cp;

	sp = strp;
	if (sp != NULL)
		sp->s_clnc = sp->s_nlnc;
	for (;;) {
		if (sp == NULL) {
			if (eofflag == 0)
				panic("Unexpected end of file");
			return (EOF);
		}
		switch (sp->s_type) {
		case SSINP:
			if ((c=getc(stdin)) == EOF)
				break;
			if (c == '\0')
				break;
			if (c == '\n') {
				if ((c=getc(stdin)) == '\n')
					c = '\0';
				ungetc(c, stdin);
				c = '\n';
			}
			goto ret;
		case SFILE:
			if ((c=getc(sp->s_fp)) == EOF) {
				fclose(sp->s_fp);
				break;
			}
			goto ret;
		case SCORE:
			if ((c=*sp->s_cp++) == '\0') {
				if (sp->s_srel)
					nfree(sp->s_srel);
				break;
			}
			goto ret;
		case SCTEX:
			if (sp->s_disk  &&  sp->s_sp >= sp->s_bufend) {
				nread(sp->s_seek, sp->s_bufp);
				sp->s_seek += DBFSIZE;
				sp->s_sp = sp->s_bufp;
			}
			if ((c=*sp->s_sp++) == '\0') {
				if (strnext())
					continue;
				nfree(sp->s_bufp);
				break;
			}
			goto ret;
		case SCDIV:
			if (sp->s_n-- == 0) {
				if (strnext())
					continue;
				nfree(sp->s_bufp);
				break;
			}
			n = sizeof (CODE);
			cp = (char *) &codeval;
			while (n--)
				*cp++ = geth();
			return (ECOD);
		}
		if (sp->s_abuf)
			nfree(sp->s_abuf);
		n = sp->s_eoff;
		strp = strp->s_next;
		nfree(sp);
		sp = strp;
		if (n) {
			if (eofflag == 0)
				panic("Incomplete macro in trap");
			return (EOF);
		}
	}
ret:
	if ((c&=0377) == '\n')
		sp->s_nlnc++;
	return (c);
}

/*
 * In a chained macro, set up input stack to point to next
 * element in chain.
 */
strnext()
{
	register STR *sp;
	register MAC *mp;

	sp = strp;
	if ((mp=sp->s_macp) == NULL)
		return (0);
	sp->s_macp = mp->m_next;
	switch (mp->m_type) {
	case MTEXT:
		sp->s_type = SCTEX;
		break;
	case MDIVN:
		sp->s_type = SCDIV;
		if ((sp->s_n=mp->m_size) == 0)
			return (strnext());
		break;
	}
	if (mp->m_core) {
		sp->s_disk = 0;
		sp->s_sp = mp->m_core;
	} else {
		sp->s_disk = 1;
		sp->s_seek = mp->m_seek/DBFSIZE * DBFSIZE;
		sp->s_sp = &(sp->s_bufp[mp->m_seek%DBFSIZE]);
		nread(sp->s_seek, sp->s_bufp);
		sp->s_seek += DBFSIZE;
	}
	return (1);
}

/*
 * Get a single character assuming the stack has been set up
 * for chained I/O.
 */
geth()
{
	if (strp->s_disk  &&  strp->s_sp >= strp->s_bufend) {
		nread(strp->s_seek, strp->s_bufp);
		strp->s_seek += DBFSIZE;
		strp->s_sp = strp->s_bufp;
	}
	return (*strp->s_sp++);
}

/*
 * Push down input stack and add the given file on top.
 */
adsfile(file)
char *file;
{
	register STR *sp;
	register FILE *fp;

	if ((fp=fopen(file, "r")) == NULL) {
		printe("Cannot open %s", file);
		return (0);
	}
	sp = allstr(SFILE);
	sp->s_next = strp;
	strp = sp;
	sp->s_fp = fp;
	return (1);
}

/*
 * Push down input stack and add the given unit.
 */
adsunit(u)
FILE *u;
{
	register STR *sp;

	sp = allstr(SFILE);
	sp->s_next = strp;
	strp = sp;
	sp->s_fp = u;
}

/*
 * Push down input stack and add the given register.
 */
adstreg(rp)
register REG *rp;
{
	register STR *sp;

	if (rp->r_macd.m_type == MREQS)
		return;
	sp = allstr(0);
	sp->s_next = strp;
	strp = sp;
	sp->s_bufp = nalloc(DBFSIZE);
	sp->s_bufend = &sp->s_bufp[DBFSIZE];
	sp->s_macp = &rp->r_macd;
	if (strnext() == 0) {
		strp = strp->s_next;
		nfree(sp->s_bufp);
		nfree(sp);
		return (0);
	}
	return (1);
}

/*
 * Given the name of a special number register, expand it and
 * add it onto the input stack.
 */
spcnreg(name)
char name[2];
{
	register STR *sp;
	register int n;
	register char *cp;

	n = 0;
	if (name[0] == '.') {
		switch (name[1]) {
		case '$':
			if ((n=strp->s_argc) > 0)
				--n;
			break;
		case 'A':
			n = antflag;
			break;
		case 'H':
			n = unit(SMHRES, SDHRES);
			break;
		case 'T':
			n = tntflag;
			break;
		case 'V':
			n = unit(SMVRES, SDVRES);
			break;
		case 'a':
			n = arorval;
			break;
		case 'c':
			n = 0;
			for (sp=strp; sp; sp=sp->s_next) {
				if (sp->s_type == SFILE) {
					n = sp->s_clnc;
					break;
				}
			}
			break;
		case 'd':
			n = cdivp->d_rpos;
			break;
		case 'f':
			n = 0;
			break;
		case 'h':
			n = cdivp->d_maxh;
			break;
		case 'i':
			n = ind;
			break;
		case 'l':
			n = lln;
			break;
		case 'n':
			n = nrorval;
			break;
		case 'o':
			n = pof;
			break;
		case 'p':
			n = pgl;
			break;
		case 's':
			n = unit(psz*SDPOIN, SMPOIN);
			break;
		case 't':
			n = cdivp->d_ctpp ? cdivp->d_ctpp->t_apos : pgl;
			if (n > pgl)
				n = pgl;
			n -= cdivp->d_rpos;
			break;
		case 'u':
			n = fil;
			break;
		case 'v':
			n = vls;
			break;
		case 'w':
			n = 0;
			break;
		case 'x':
			n = 0;
			break;
		case 'y':
			n = 0;
			break;
		case 'z':
			cp = nalloc(3);
			cp[0] = cdivp->d_name[0];
			cp[1] = cdivp->d_name[1];
			cp[2] = '\0';
			adscore(cp);
			strp->s_srel = cp;
			return;
		}
	}
	adsnval(n, '1');
}

/*
 * Given a value and a format, expand out the value to the
 * particular format and add it onto the top of the stack.
 */
adsnval(val, f)
char f;
{
	register ROM *op;
	int w;
	register int n;
	char charbuf[CBFSIZE], *str, *cp2;
	register char *cp1;

	n = val;
	if (n <= 0) {
		if ((n=-n) <= 0) {
			n = 0;
			if (!isdigit(f))
				f = '0';
		}
	}
	cp1 = charbuf;
	switch (f) {
	case 'a':
	case 'A':
		while (n) {
			n--;
			*cp1++ = 'a' + n%26;
			n /= 26;
		}
		break;
	case 'i':
	case 'I':
		for (cp2="ivxlcdm"; *cp2!='m'; cp2+=2) {
			op = &romtab[n%10];
			n /= 10;
			while (op != &romtab[0]) {
				*cp1++ = cp2[op->o_digit];
				op = &romtab[op->o_state];
			}
		}
		if (cp1+n >= &charbuf[CBFSIZE]) {
			*cp1++ = '*';
			break;
		}
		while (n--)
			*cp1++ = 'm';
		break;
	default:
		w = f - '0';
		while (n) {
			--w;
			*cp1++ = n%10 + '0';
			n /= 10;
		}
		while (w-- > 0)
			*cp1++ = '0';
		if (val < 0)
			*cp1++ = '-';
	}
	cp2 = str = nalloc(cp1-charbuf + 1);
	if (isupper(f)) {
		while (cp1 > charbuf)
			*cp2++ = islower(*--cp1) ? toupper(*cp1) : *cp1;
	} else {
		while (cp1 > charbuf)
			*cp2++ = *--cp1;
	}
	*cp2 = '\0';
	adscore(str);
	strp->s_srel = str;
}

/*
 * Divert input to the given string.
 */
adscore(cp)
char *cp;
{
	register STR *sp;

	sp = allstr(SCORE);
	sp->s_next = strp;
	strp = sp;
	sp->s_cp = cp;
	sp->s_srel = NULL;
}

/*
 * Allocate an entry to add to the input stack.
 */
char	*
allstr(type)
{
	register STR *sp;
	register int i;

	sp = (STR *) nalloc(sizeof *sp);
	sp->s_type = type;
	sp->s_eoff = 0;
	sp->s_clnc = 1;
	sp->s_nlnc = 1;
	sp->s_argc = 0;
	for (i=0; i<ARGSIZE; i++)
		sp->s_argp[i] = null;
	sp->s_abuf = NULL;
	return (sp);
}
