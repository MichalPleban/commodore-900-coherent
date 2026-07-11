/*
 * Nroff/Troff.
 * Requests (n-z).
 */
#include <stdio.h>
#include <ctype.h>
#include "roff.h"
#include "code.h"
#include "env.h"
#include "div.h"
#include "reg.h"
#include "str.h"
#include "esc.h"

/*
 * Turn adjust mode off.
 */
req_na()
{
	adm = 0;
}

/*
 * Need.
 */
req_ne(argc, argv)
char *argv[];
{
}

/*
 * Go into no fill mode.
 */
req_nf()
{
	setbreak();
	fil = 0;
}

/*
 * Turn off hyphenation
 */
req_nh()
{
	hyp = 0	/* turn the durn thing off */;
}
/*
 * Set number mode.
 */
req_nm(argc, argv)
char *argv[];
{
/*
	if ((lnn=number(argv[1], SMUNIT, SDUNIT, lnn, 0, 0)) <= 0) {
		lnn = 0;
		return;
	}
	if (argv[2][0] != '\0')
		lnm = number(argv[2], SMUNIT, SDUNIT, 0, 0, 1);
	if (argv[3][0] != '\0')
		lns = number(argv[3], SMDIGW, SDDIGW, 0, 0, 1);
	if (argv[4][0] != '\0')
		lni = number(argv[4], SMDIGW, SDDIGW, 0, 0, 1);
*/
}

/*
 * Turn off line number mode for n lines.
 */
req_nn(argc, argv)
char *argv[];
{
/*
	register int n;

	lnc = number(argv[1], SMUNIT, SDUNIT, 0, 0, 1);
*/
}

/*
 * Define a number register.
 */
req_nr(argc, argv)
char *argv[];
{
	register REG *rp;
	char name[2];

	argname(argv[1], name);
	rp = getnreg(name, RNUMR);
	rp->r_nval = number(argv[2], SMUNIT, SDUNIT, rp->r_nval, 0, 0);
	if (argc >= 4)
		rp->r_incr = number(argv[3], SMUNIT, SDUNIT, 0, 0, 0);
}

/*
 * Turn no-space mode on.
 */
req_ns()
{
	nsm = 1;
}

/*
 * End input from current file and switch to given file.
 */
req_nx(argc, argv)
char *argv[];
{
	register FILE *fp;
	register STR *sp;

	/*
	 * Bug fix (July 2026): the do-while never advanced sp, so
	 * .nx from inside a macro looped forever.
	 */
	sp = strp;
	while (sp != NULL && sp->s_type != SFILE)
		sp = sp->s_next;
	if (sp == NULL) {
		printe("Cannot find current file");
		return;
	}
	if ((fp=fopen(argv[1], "r")) == NULL) {
		printe("Cannot open %s", argv[1]);
		return;
	}
	fclose(sp->s_fp);
	sp->s_fp = fp;
}

/*
 * Output saved space.
 */
req_os()
{
	sspace(svs);
}

/*
 * Set page number character in title.
 */
req_pc(argc, argv)
char *argv[];
{
	tpc = argv[1][0];
}

/*
 * Set page length.
 */
req_pl(argc, argv)
char *argv[];
{
	pgl = number(argv[1], SMVLSP, SDVLSP, pgl, 0, unit(11*SMINCH, SDINCH));
	if (pgl <= 0)
		pgl = unit(SMVLSP, SDVLSP);
}

/*
 * Print sizes of macros.
 */
req_pm(argc, argv)
{
	register REG **rpp, *rp;
	register MAC *mp;
	unsigned asize, csize, dsize;

	asize = 0;
	csize = 0;
	dsize = 0;
	for (rpp = &regt[0];  rpp < &regt[RHTSIZE];  rpp++) {
		for (rp = *rpp;  rp;  rp = rp->r_next) {
			asize++;
			csize += sizeof *rp;
			if (rp->r_type != RTEXT)
				continue;
			csize -= sizeof rp->r_macd;
			--asize;
			/*
			 * Bug fix (July 2026): the third clause was a
			 * bare `mp->m_next', so .pm hung forever.
			 */
			for (mp = &rp->r_macd;  mp;  mp = mp->m_next) {
				asize++;
				csize += sizeof *mp;
			}
		}
	}
	fprintf(stderr, "%u allocations\n", asize);
	fprintf(stderr, "Core size = %u\n", csize);
/*
	fprintf(stderr, "Disk size = %u\n", dsize);
*/
}

/*
 * Set page number.
 */
req_pn(argc, argv)
char *argv[];
{
	if (argc >= 2)
		pno = number(argv[1], SMUNIT, SDUNIT, pno, 0, pno);
}

/*
 * Set page offset.
 */
req_po(argc, argv)
char *argv[];
{
	register int n;

	n = pof;
	pof = number(argv[1], SMEMSP, SDEMSP, pof, 0, oldpof);
	oldpof = n;
}

/*
 * Set pointsize.
 */
req_ps(argc, argv)
char *argv[];
{
	register int n;

	n = psz;
	setpsze(number(argv[1], SMPOIN, SDPOIN, psz, 0, oldpsz));
	oldpsz = n;
}

/*
 * Read an insertion from the standard input.
 */
req_rd(argc, argv)
char *argv[];
{
	register STR *sp;

	if (argc >= 2)
		fprintf(stderr, "%s", argv[1]);
	else
		putc(BEL, stderr);
	sp = allstr(SSINP);
	sp->s_next = strp;
	strp = sp;
}

/*
 * Remove text register or request.
 */
req_rm(argc, argv)
char *argv[];
{
	char name[2];
	register int i;

	for (i=1; i<argc; i++) {
		argname(argv[i], name);
		if (reltreg(name))
			continue;
		printe("Cannot remove %s", argv[i]);
	}
}

/*
 * Rename the given request or macro.
 */
req_rn(argc, argv)
char *argv[];
{
	register REG *rp, **lrp;
	char name[2];

	argname(argv[1], name);
	if ((rp=findreg(name, RTEXT)) == NULL) {
		printe("Cannot find register %s", argv[1]);
		return;
	}
	/*
	 * Bug fix (July 2026): the rename left the register in the
	 * old name's hash bucket, so the new name could not be
	 * found unless both names hashed to the same bucket.  Move
	 * it to the new name's bucket.
	 */
	for (lrp = &regt[hash(name)]; *lrp != rp; lrp = &(*lrp)->r_next)
		;
	*lrp = rp->r_next;
	argname(argv[2], name);
	rp->r_name[0] = name[0];
	rp->r_name[1] = name[1];
	rp->r_next = regt[hash(name)];
	regt[hash(name)] = rp;
}

/*
 * Remove number register.
 */
req_rr(argc, argv)
char *argv[];
{
	char name[2];
	register int i;

	for (i=1; i<argc; i++) {
		argname(argv[i], name);
		if (relnreg(name))
			continue;
		printe("Cannot remove %s", argv[i]);
	}
}

/*
 * Turn no space mode off.
 */
req_rs()
{
	nsm = 0;
}

/*
 * Return to marked vertical position.
 */
req_rt(argc, argv)
char *argv[];
{
	register int n;

	if (argc == 1)
		n = cdivp->d_mk;
	else
		n = number(argv[1], SMVLSP, SDVLSP, 0, 0, 0);
	if (n >= cdivp->d_rpos)
		return;
	sspace(n - cdivp->d_rpos);
}

/*
 * Stack input and redirect from given file.
 */
req_so(argc, argv)
char *argv[];
{
	adsfile(argv[1]);
}

/*
 * Space vertically.
 */
req_sp(argc, argv)
char *argv[];
{
	register int n;

	if (nsm != 0)
		return;
	setbreak();
	sspace(number(argv[1], SMVLSP, SDVLSP, 0, 1, 1));
}

/*
 * Save vertical distance.
 */
req_sv(argc, argv)
char *argv[];
{
	register int n;

	n = number(argv[1], SMVLSP, SDVLSP, 0, 0, 1);
	if (mdivp->d_rpos+n > mdivp->d_ctpp->t_rpos)
		svs = n;
	else
		sspace(n);
}

/*
 * Set tab stops.
 */
req_ta(argc, argv)
char *argv[];
{
	register TAB *tp;
	register int i;
	register char *p;
	int	prevpos = 0;

	tp = &tab[1];
	for (i=1; i<argc; i++) {
		if (i > TABSIZE-2) {
			printe("Too many tab stops");
			break;
		}
		p = argv[i];
		if (*p == '\0') {
			printe("Bad tab stop");
			break;
		}
		while (*p)
			p++;
		tp->t_jus = LJUS;
		switch (*--p) {
		case 'L':
			tp->t_jus = LJUS;
			*p = '\0';
			break;
		case 'C':
			tp->t_jus = CJUS;
			*p = '\0';
			break;
		case 'R':
			tp->t_jus = RJUS;
			*p = '\0';
			break;
		}
		tp->t_pos = number(argv[i], SMEMSP, SDEMSP, prevpos, 0, 0);
		if (tp->t_pos < prevpos) {
			printe("Bad tab stop");
			break;
		}
		prevpos = tp->t_pos;
		tp++;
	}
	tp->t_jus = '\0';
}

/*
 * Set tab character.
 */
req_tc(argc, argv)
char *argv[];
{
	tbc = argv[1][0];
}

/*
 * Temporary indent.
 */
req_ti(argc, argv)
char *argv[];
{
	register int n;

	setbreak();
	tin = number(argv[1], SMEMSP, SDEMSP, ind, 0, 0);
	tif = 1;
}

/*
 * Three part title.
 */
req_tl(argc, argv)
char *argv[];
{
	CODE *cp;
	int i;
	register int n;
	char charbuf[CBFSIZE], endc, c;
	register char *bp, *lp;
	ENV	savenv;

	savenv = env;			/* massive block copy */
	setline();
	ind = 0;
	lln = tln;
	fil = 0;
	bp = nextarg(miscbuf, NULL, 0);
	if ((endc=*bp) != '\0')
		bp++;
	for (i=0; i<3; i++) {
		lp = charbuf;
		for (;;) {
			if (*bp == '\0')
				break;
			if ((c=*bp++) == endc)
				break;
			if (c == tpc) {
				char	digits[5];
				register char	*dp = &digits[0];
				register int	pn = pno;

				for(;  pn > 0;  pn /= 10)
					*dp++ = pn%10 + '0';
				while (--dp > &digits[0])
					*lp++ = *dp;
				c = *dp;
			}
			if (lp >= &charbuf[CBFSIZE-2]) {
				printe("Section %d of title too large", i+1);
				break;
			}
			*lp++ = c;
		}
		if (lp != charbuf) {
			*lp++ = '\0';
			adscore(charbuf);
			strp->s_eoff = 1;
			process();
			wordbreak(DHMOV);
		}
		switch (i) {
		case 0:
			n = llinsiz;
			cp = llinptr;
			break;
		case 1:
			n = (lln - llinsiz - n)/2;
			nlinsiz = llinsiz += cp->c_iarg = n;
			cp = llinptr;
			break;
		case 2:
			n = lln - llinsiz;
			cp->c_iarg += n;
			llinsiz += n;
			break;
		}
	}
	linebreak();
	env = savenv;
}

/*
 * Print a message on the terminal.
 */
req_tm(argc, argv)
char *argv[];
{
	fprintf(stderr, "%s\n", nextarg(miscbuf, NULL, 0));
}

/*
 * Translate on output.
 */
req_tr(argc, argv)
char *argv[];
{
	register char c1, c2, *p;

	p = argv[1];
	while (c1=*p++) {
		if ((c2=*p++) == '\0') {
			--p;
			c2 = ' ';
		}
		if (!isascii(c1) || !isascii(c2))
			continue;
		trantab[c1] = c2;
	}
}

/*
 * Set underline font.
 */
req_uf(argc, argv)
char *argv[];
{
}

/*
 * Underline.
 */
req_ul(argc, argv)
char *argv[];
{
	ufp[0] = fon[0];
	ufp[1] = fon[1];
	setfont(uft, 0);
	ulc = number(argv[1], SMUNIT, SDUNIT, 0, 0, 1);
}

/*
 * Set vertical base line spacing.
 */
req_vs(argc, argv)
char *argv[];
{
	register int n;

	n = vls;
	vls = number(argv[1], SMPOIN, SDPOIN, vls, 0, oldvls);
	oldvls = n;
}

/*
 * Set a trap.
 */
req_wh(argc, argv)
char *argv[];
{
	register TPL **tpp, *tp;
	register DIV *dp;
	int rpos, apos;

	rpos = number(argv[1], SMVLSP, SDVLSP, 0, 0, 0);
	apos = rpos>=0 ? rpos : pgl+rpos;
	dp = mdivp;
	for (tpp=&dp->d_stpl; tp=*tpp; tpp=&tp->t_next) {
		if (apos == tp->t_apos) {
			if (dp->d_trap == tp)
				dp->d_trap = tp->t_next;
			if (dp->d_ctpp == tp)
				dp->d_ctpp = tp->t_next;
			*tpp = tp->t_next;
			nfree(tp);
			if (argc < 3)	/* .wh NN xx !replaces all at NN */
				break;	/* .wh NN !only removes 1st */
		 } else if (apos < tp->t_apos)
			break;
	}
	if (argc >= 3) {
		tp = nalloc(sizeof (TPL));
		tp->t_rpos = rpos;
		tp->t_apos = apos;
		argname(argv[2], tp->t_name);
		tp->t_next = *tpp;
		*tpp = tp;
		if (dp->d_trap==tp->t_next && apos>=0)
			dp->d_trap = tp;
		if (dp->d_ctpp==tp->t_next && apos>=dp->d_rpos)
			dp->d_ctpp = tp;
	}
}

/*
 * Debugging command.
 */
req_zz()
{
	d00flag = 1;
}
