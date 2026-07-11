/*
 * Nroff/Troff.
 * Requests (a-m).
 */
#include <stdio.h>
#include <ctype.h>
#include "roff.h"
#include "code.h"
#include "div.h"
#include "env.h"
#include "esc.h"
#include "reg.h"
#include "str.h"

/*
 * User abort.
 */
req_ab()
{
	register char *bp;

	bp = nextarg(miscbuf, NULL, 0);
	if (*bp != '\0')
		fprintf(stderr, "%s\n", bp);
	else
		fprintf(stderr, "User Abort\n");
	abort();
}

/*
 * Turn adjust mode on and set adjust type.
 */
req_ad(argc, argv)
char *argv[];
{
	adm = 1;
	if (argc < 2)
		return;
	switch (argv[1][0]) {
	case 'l':
		adj = LJUS;
		return;
	case 'c':
		adj = CJUS;
		return;
	case 'r':
		adj = RJUS;
		return;
	case 'b':
	case 'u':
		adj = FJUS;
		return;
	default:
		printe("Bad adjustment type");
		return;
	}
}

/*
 * Assign format.
 */
req_af(argc, argv)
char *argv[];
{
	register REG *rp;
	int n;
	char name[2];
	register char *p, c;

	argname(argv[1], name);
	rp = getnreg(name);
	if (index("iIaA", c=argv[2][0])) {
		rp->r_form = c;
		return;
	}
	if (isascii(c) && isdigit(c)) {
		n = '0';
		p = &argv[2][1];
		while (isascii(c=*p++) && isdigit(*p))
			;
		if (p-argv[2] > 9) {
			printe("Field with too large");
			return;
		}
		rp->r_form = '0' + p-argv[2];
	}
}

/*
 * Append to macro.
 */
req_am(argc, argv)
char *argv[];
{
	register REG *rp;
	register MAC *mp;
	char name[2];

	argname(argv[1], name);
	if ((rp=findreg(name, RTEXT)) == NULL) {
		rp = makereg(name, RTEXT);
		mp = &rp->r_macd;
	} else {
		for (mp=&rp->r_macd; mp->m_next; mp=mp->m_next)
			;
		mp->m_next = nalloc(sizeof (*mp));
		mp = mp->m_next;
	}
	deftext(mp, argv[2]);
}

/*
 * Append to string.
 */
req_as(argc, argv)
char *argv[];
{
	register REG *rp;
	register MAC *mp;
	char name[2];
	register char *cp;

	argname(argv[1], name);
	if ((rp=findreg(name, RTEXT)) == NULL) {
		rp = makereg(name, RTEXT);
		mp = &rp->r_macd;
	} else {
		for (mp=&rp->r_macd; mp->m_next; mp=mp->m_next)
			;
		mp->m_next = nalloc(sizeof *mp);
		mp = mp->m_next;
	}
	cp = nalloc(strlen(argv[2]) + 1);
	strcpy(cp, argv[2]);
	mp->m_next = NULL;
	mp->m_type = MTEXT;
	mp->m_core = cp;
}

/*
 * Begin page.
 */
req_bp(argc, argv)
char *argv[];
{
	setbreak();
	if (argc >= 2) {
		npn = number(argv[1], SMUNIT, SDUNIT, 0, 0, 0);
		pspace(1);
		return;
	}
	if (nsm == 0)
		pspace(1);
}

/*
 * Break.
 */
req_br()
{
	setbreak();
}

/*
 * Set nobreak control character.
 */
req_c2(argc, argv)
char *argv[];
{
	nbc = argc>1 ? argv[1][0] : '\'';
}

/*
 * Set control character.
 */
req_cc(argc, argv)
char *argv[];
{
	ccc = argc>1 ? argv[1][0] : '.';
}

/*
 * Centre all text.
 */
req_ce(argc, argv)
char *argv[];
{
	setbreak();
	cec = number(argv[1], SMUNIT, SDUNIT, 0, 0, 1);
}

/*
 * Change trap location.
 */
req_ch(argc, argv)
char *argv[];
{
	register TPL **tpp, *tp;
	register DIV *dp;
	int rpos, apos;
	char name[2];

	dp = mdivp;
	argname(argv[1], name);
	for (tpp=&dp->d_stpl; tp=*tpp; tpp=&tp->t_next) {
		if (name[0]==tp->t_name[0] && name[1]==tp->t_name[1]) {
			if (dp->d_trap == tp)
				dp->d_trap = tp->t_next;
			if (dp->d_ctpp == tp)
				dp->d_ctpp = tp->t_next;
			*tpp = tp->t_next;
			nfree(tp);
			break;
		}
	}
	if (argc >= 3) {
		rpos = number(argv[2], SMVLSP, SDVLSP, 0, 0, 0);
		apos = rpos>=0 ? rpos : pgl+rpos;
		for (tpp=&dp->d_stpl; tp=*tpp; tpp=&tp->t_next) {
			if (apos <= tp->t_apos)
				break;
		}
		tp = nalloc(sizeof (TPL));
		tp->t_rpos = rpos;
		tp->t_apos = apos;
		tp->t_name[0] = name[0];
		tp->t_name[1] = name[1];
		tp->t_next = *tpp;
		*tpp = tp;
		if (dp->d_trap==tp->t_next && apos>=0)
			dp->d_trap = tp;
		if (dp->d_ctpp==tp->t_next && apos>=dp->d_rpos)
			dp->d_ctpp = tp;
	}
}

/*
 * Set constant character space mode.
 * Note that the second argument (font) is ignored.
 */
req_cs(argc, argv)
{
/*
	register int ems;

	ems = number(argv[3], SMPOIN, SDPOIN, 0, 0, unit(SMEMSP, SDEMSP));
	csz = number(argv[2], ems, 1, 0, 0, 0);
*/
}

/*
 * Continous underline.
 */
req_cu(argc, argv)
char *argv[];
{
	ulc = INFINITY;
}

/*
 * Divert and append output to macro.
 */
req_da(argc, argv)
char *argv[];
{
	register REG *rp;
	register MAC *mp;
	char name[2];

	if (argc < 2) {
		enddivn();
		return;
	}
	argname(argv[1], name);
	newdivn(name);
	if ((rp=findreg(name, RTEXT)) == NULL) {
		rp = makereg(name, RTEXT);
		mp = &rp->r_macd;
		mp->m_next = NULL;
		mp->m_type = MDIVN;
		mp->m_size = 0;
		mp->m_core = NULL;
		mp->m_seek = tmpseek;
	} else {
		cdivp->d_maxh = rp->r_maxh;
		cdivp->d_maxw = rp->r_maxw;
		for (mp=&rp->r_macd; mp->m_next; mp=mp->m_next)
			;
		/*
		 * Bug fix (July 2026): force flushd to start a fresh
		 * MAC record.  The old tail's temp-file data is not
		 * contiguous with the current end of the temp file
		 * (every flush pads to a 512 boundary), but flushd
		 * saw d_seek == tmpseek and grew the old record's
		 * m_size across the gap, so replaying an appended
		 * diversion read padding as CODEs and panicked.
		 */
		cdivp->d_seek = -1;
	}
	cdivp->d_macp = mp;
}

/*
 * Define a macro.
 */
req_de(argc, argv)
char *argv[];
{
	register REG *rp;
	char name[2];

	argname(argv[1], name);
	rp = makereg(name, RTEXT);
	deftext(&rp->r_macd, argv[2]);
}

/*
 * Divert output to macro.
 */
req_di(argc, argv)
char *argv[];
{
	register REG *rp;
	char name[2];

	if (argc < 2) {
		enddivn();
		return;
	}
	argname(argv[1], name);
	newdivn(name);
	rp = makereg(name, RTEXT);
	cdivp->d_macp = &rp->r_macd;
	rp->r_macd.m_next = NULL;
	rp->r_macd.m_type = MDIVN;
	rp->r_macd.m_size = 0;
	rp->r_macd.m_core = NULL;
	rp->r_macd.m_seek = tmpseek;
}

/*
 * Define a string.
 */
req_ds(argc, argv)
char *argv[];
{
	register REG *rp;
	char name[2];
	register char *cp;

	argname(argv[1], name);
	rp = makereg(name, RTEXT);
	cp = nalloc(strlen(argv[2]) + 1);
	strcpy(cp, argv[2]);
	rp->r_macd.m_next = NULL;
	rp->r_macd.m_type = MTEXT;
	rp->r_macd.m_core = cp;
}

/*
 * Set a diversion trap.
 */
req_dt(argc, argv)
char *argv[];
{
}

/*
 * Set escape character.
 */
req_ec(argc, argv)
char *argv[];
{
	esc = argc>1 ? argv[1][0] : '\\';
}

/*
 * Else part of if-else.
 * Bug fix (July 2026): bp was a plain (signed) char *, so the
 * EBEG (0243) compares never matched -- .el neither stripped a
 * leading block marker nor skipped a multi-line else-block after
 * a true .ie.  bp is unsigned char * now, as in req_if, and the
 * skip path gains req_if's same-line EEND scan.
 */
req_el(argc, argv)
char *argv[];
{
	char charbuf[CBFSIZE], c;
	register unsigned char *bp;
	register char *cp;

	bp = nextarg(miscbuf, NULL, 0);
	if (!lastcon) {
		cp = charbuf;
		if (*bp == EBEG)
			bp++;
		while (c=*bp++) {
			if (cp < &charbuf[CBFSIZE-2])
				*cp++ = c;
		}
		*cp++ = '\n';
		*cp++ = '\0';
		cp = duplstr(charbuf);
		adscore(cp);
		strp->s_srel = cp;
	} else {
		if (*bp++ == EBEG) {
			while (*bp != '\0')
				if (*bp++ == EEND)
					return;
			ifeflag = 1;
			while (getf(0) != EEND)
				;
			ifeflag = 0;
			while (getf(0) != '\n')
				;
		}
	}
}

/*
 * Set end macro.
 */
req_em(argc, argv)
char *argv[];
{
	argname(argv[1], endtrap);
}

/*
 * Turn off escape mechanism.
 */
req_eo()
{
	esc = '\0';
}

/*
 * Change enviroments.
 */
req_ev(argc, argv)
char *argv[];
{
	register int old, new;

	if (argc < 2) {
		if (envs == 0) {
			printe("Cannot pop enviroment");
			return;
		}
		old = envstak[envs];
		new = envstak[--envs];
	} else {
		new = number(argv[1], SMUNIT, SDUNIT, 0, 0, 0);
		if (new<0 || new>=ENVSIZE) {
			printe("Enviroment does not exist");
			return;
		}
		if (envs >= EVSSIZE) {
			printe("Enviroments stacked too deeply");
			return;
		}
		old = envstak[envs];
		envstak[++envs] = new;
	}
	lseek(fileno(tmp), (long) old * sizeof (ENV), 0);
	if (write(fileno(tmp), &env, sizeof (env)) != sizeof (env))
		panic("Cannot write enviroment");
	if (envinit[new] == 0) {
		setenvr();
		envinit[new] = 1;
	} else {
		lseek(fileno(tmp), (long) new * sizeof (ENV), 0);
		if (read(fileno(tmp), &env, sizeof (env)) != sizeof (env))
			panic("Cannot read enviroment");
	}
}

/*
 * Exit from nroff.
 */
req_ex()
{
	leave();
}

/*
 * Turn on fill mode.
 */
req_fi()
{
	setbreak();
	fil = 1;
}

/*
 * Flush.
 */
req_fl()
{
	setbreak();
}

/*
 * Define font at position
 */
req_fp(argc, argv)
char *argv[];
{
	register n;

	n = argv[1][0] - '0';

	if ((n <= 8) && (n >= 1))
		mapfont[n] = argv[2][0];
	else
		printe("Font position out of range");

}
/*
 * Set current font.
 */
req_ft(argc, argv)
char *argv[];
{
	char name[2];

	argname(argv[1], name);
	setfont(name, 1);
}

/*
 * If part of if-else.
 */
req_ie(argc, argv)
char *argv[];
{
	lastcon = req_if(argc, argv);
}

/*
 * If (conditional execution of command).
 * This returns the condition and is called from `req_ie'.
 */
req_if()
{
	int not, con;
	char charbuf[CBFSIZE], endc;
	register int c;
	register unsigned char *bp;
	register char *cp;

	bp = nextarg(miscbuf, NULL, 0);
	not = 0;
	if (*bp == '!') {
		bp++;
		not = 1;
	}
	switch (*bp++) {
	case 'e':
		con = 1;
		break;
	case 'n':
		con = 1;
		break;
	case 'o':
		con = 1;
		break;
	case 't':
		con = 1;
		break;
	default:
		--bp;
		if (isascii(*bp) && isdigit(*bp)) {
			bp = nextarg(bp, charbuf, CBFSIZE);
			con = number(charbuf, SMUNIT, SDUNIT, 0, 0, 0) > 0;
			break;
		}
		if ((endc=*bp) == '\0') {
			con = 1;
			break;
		}
		bp++;
		cp = charbuf;
		while ((c=*bp++) != endc) {
			if (c == '\0') {
				--bp;
				break;
			}
			if (cp < &charbuf[CBFSIZE-1])
				*cp++ = c;
		}
		*cp++ = '\0';
		cp = charbuf;
		con = 1;
		while ((c=*bp++) != endc) {
			if (c == '\0') {
				--bp;
				break;
			}
			if (c == *cp)
				cp++;
			else
				con = 0;
		}
	}
	while (isascii(*bp) && isspace(*bp))
		bp++;
	if (not)
		con = !con;
	if (con) {
		cp = charbuf;
		if (*bp == EBEG)
			bp++;
		while (c=*bp++) {
			if (cp < &charbuf[CBFSIZE-2])
				*cp++ = c;
		}
		*cp++ = '\n';
		*cp++ = '\0';
		cp = duplstr(charbuf);
		adscore(cp);
		strp->s_srel = cp;
	} else {
		if (*bp++ == EBEG) {
			while (*bp != '\0') if (*bp++ == EEND)
				return (con);
			ifeflag = 1;
			while (getf(0) != EEND)
				;
			ifeflag = 0;
			while (getf(0) != '\n')
				;
		}
	}
	return (con);
}

/*
 * Ignore.
 */
req_ig(argc, argv)
char *argv[];
{
	deftext(NULL, argv[1]);
}

/*
 * Set indent.
 */
req_in(argc, argv)
char *argv[];
{
	register int n;

	n = ind;
	ind = number(argv[1], SMEMSP, SDEMSP, ind, 0, oldind);
	oldind = n;
	setbreak();
}


/*
 * Set an input line count trap.
 */
req_it(argc, argv)
char *argv[];
{
	if (argc < 3) {
		inpltrc = 0;
		return;
	}
	inpltrc = number(argv[1], SMUNIT, SDUNIT, 0, 0, 0);
	argname(argv[2], inptrap);
}

/*
 * Set ligature mode.
 */
req_lg(argc, argv)
char *argv[];
{
	lgm = number(argv[1], SMUNIT, SDUNIT, 0, 0, 0) > 0;
}

/*
 * Set line length.
 */
req_ll(argc, argv)
char *argv[];
{
	register int n;

	n = lln;
	lln = number(argv[1], SMEMSP, SDEMSP, lln, 0, oldlln);
	oldlln = n;
}

/*
 * Set line spacing.
 */
req_ls(argc, argv)
char *argv[];
{
	register int n;

	n = lsp;
	lsp = number(argv[1], SMUNIT, SDUNIT, lsp, 0, oldlsp);
	oldlsp = n;
}

/*
 * Set title length.
 */
req_lt(argc, argv)
char *argv[];
{
	register int n;

	n = tln;
	tln = number(argv[1], SMEMSP, SDEMSP, tln, 0, oldtln);
	oldtln = n;
}

/*
 * Set margin character.
 */
req_mc(argc, argv)
char *argv[];
{
	mrc = argv[1][0];
	mar = number(argv[2], SMEMSP, SDEMSP, mar, 0, 0);
}

/*
 * Mark vertical place internally in diversion or in number register.
 */
req_mk(argc, argv)
char *argv[];
{
	register REG *rp;
	char name[2];

	if (argc == 1) {
		cdivp->d_mk = cdivp->d_rpos;
		return;
	}
	argname(argv[1], name);
	if ((rp=findreg(name, RNUMR)) == NULL) {
		rp = makereg(name, RNUMR);
		rp->r_incr = 1;
		rp->r_form = '1';
	}
	rp->r_nval = cdivp->d_rpos;
}
