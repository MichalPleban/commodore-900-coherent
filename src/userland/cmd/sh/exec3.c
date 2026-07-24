/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Bourne shell.
 * Builtin commands.
 */
#include "sh.h"
#include <sys/times.h>
#include <const.h>		/* HZ defined here */
#define HOUR	(60L*60L*HZ)
#define MINUTE	(60L*HZ)
#define SECOND	HZ


extern	s_colon();
extern	s_dot();
extern	s_break();
#define s_continue	s_break
extern	s_cd();
extern	s_eval();
extern	s_exec();
extern	s_exit();
extern	s_export();
extern	s_login();
#define s_newgrp	s_login
extern	s_read();
#define s_readonly	s_export
extern	s_set();
extern	s_shift();
extern	s_times();
extern	s_trap();
extern	s_umask();
extern	s_wait();
#define SNULL	((int (*)())0)

typedef struct {
	int	i_hash;
	char	*i_name;
	int	(*i_func)();
} INLINE;

INLINE inls[] = {
	0,	":",		s_colon,
	0,	".",		s_dot,
	0,	"break",	s_break,
	0,	"continue",	s_continue,
	0,	"cd",		s_cd,
	0,	"eval",		s_eval,
	0,	"exec",		s_exec,
	0,	"exit",		s_exit,
	0,	"export",	s_export,
	0,	"login",	s_login,
	0,	"newgrp",	s_newgrp,
	0,	"read",		s_read,
	0,	"readonly",	s_readonly,
	0,	"set",		s_set,
	0,	"shift",	s_shift,
	0,	"times",	s_times,
	0,	"trap",		s_trap,
	0,	"umask",	s_umask,
	0,	"wait",		s_wait,
	0,	NULL,		SNULL,
};

inlfn()
{
	register int (*s_func)();
	register INLINE *ip;
	register int ahash;

	if (inls[0].i_hash==0)
		for (ip=inls; ip->i_name!=NULL; ip+=1)
			ip->i_hash = ihash(ip->i_name);
	ahash = ihash(nargv[0]);
	for (ip=inls; ip->i_name!=NULL; ip+=1)
		if (ip->i_hash==ahash && strcmp(nargv[0], ip->i_name)==0)
			break;
	if ((s_func=ip->i_func) == SNULL)
		return (0);
	if (*niovp != NULL && s_func != s_exec) {
		eredir();
		slret = 1;
	} else
		slret = (*s_func)();
	return (1);
}

ihash(cp)
register char *cp;
{
	register int i;
	for (i=0; *cp; i+=*cp++);
	return (i);
}

/*
 * Actual builtin functions.
 */
s_colon()
{
	return (0);
}
s_dot()
{
	if (nargc==2) {
		ffind(NULL);
		if (ffind(vpath, nargv[1], 4))
			return (session(SFILE, duplstr(strt, 0)));
		else {
			ecantfind(nargv[1]);
			return (1);
		}
	} else if (nargc==1)
		return (0);
	syntax();
	return (1);
}
s_break()
{
	register CON *cp;
	register int t;
	register int n;
	int ret;

	ret = nargv[0][0]=='b' ? 2 : 1;
	n = nargc>1 ? atoi(nargv[1]) : 1;
	for (cp = sesp->s_con; cp; cp = cp->c_next) {
		t = cp->c_node->n_type;
		if ((t==NWHILE || t==NFOR || t==NUNTIL) && --n == 0) {
			sesp->s_con = cp;
			longjmp(cp->c_envl, ret);
			break;
		}
		freebuf(cp->c_bpp);
	}
	printe("%s out of bounds", ret==1 ? "Continue" : "Break");
	reset(RBRKCON);
	NOTREACHED;
}
/* s_continue is overlaid with s_break */
s_cd()
{
	register char *cp;

	cp = nargc<2 ? vhome : nargv[1];
	if (chdir(cp) < 0) {
		printe("%s: bad directory", cp);
		return (1);
	}
	return (0);
}
s_eval()
{
	if (nargc>1)
		return (session(SARGV, ++nargv));
	else
		return (0);
}
s_exec()
{
	if (redirect(niovp) < 0) {
		if (nargc>1) {
			exit(1);
			NOTREACHED;
		}
		return (1);
	}
	if (nargc==1)
		return (0);
	if (no1flag)
		cleanup(2, NULL);
	dflttrp(ICMD);
	++nargv;
	--nargc;
	nenvp = envlvar(nenvp);
	flexec();
	exit(1);
	NOTREACHED;
}
s_exit()
{
	if (nargc > 1)
		slret = atoi(nargv[1]);
	reset(RUEXITS);
	NOTREACHED;
}
s_export()
{
	register int flag;
	register char **varv;

	flag = nargv[0][0]=='e' ? VEXP : VRDO;
	if (nargc < 2)
		tellvar(flag);
	else
		for (varv=++nargv; *varv; )
			if (namevar(*varv))
				flagvar(*varv++, flag);
			else
				eillvar(*varv++);
	return (0);
}
s_login()
{
	register char *cmd;

	cmd = nargv[0][0]=='l' ? "/bin/login" : "/bin/newgrp";
	execve(cmd, nargv, envlvar(nenvp));
	ecantfind(cmd);
	return (1);
}
/* s_newgrp is overlaid with s_login */
s_read()
{
	SES s;
	register int n, c;
	register char **vp;
	int eol;

	s.s_type = SSTR;
	s.s_flag = 0;
	s.s_ifp = stdin;
	s.s_next = sesp;
	sesp = &s;
	for (eol=0, vp=++nargv, n=--nargc; n; n-=1, vp+=1) {
		if (n==1 && ! eol) {
			strp = strt;
			c = collect('\n', 2);
			if (c == '\n')
				--strp;
			*strp = '\0';
		} else if (! eol)
			c = yylex();
		else
			*strt = '\0';
		if (namevar(*vp))
			assnvar(*vp, duplstr(strt, 0));
		else
			eillvar(*vp);
		eol = c=='\n' || c==EOF;
	}
	sesp = s.s_next;
	return (c==EOF);
}
/* s_readonly overlaid with s_export */
s_set()
{
	if (nargc < 2) {
		tellvar(0);
		return (0);
	}
	return (set(nargc, nargv, 0));
}
s_shift()
{
	register int n;

	n = nargc > 1 ? atoi(nargv[1]) : 1;
	while (n > 0 && sargc > 0) {
		n -= 1;
		sargc -= 1;
		sargp += 1;
	}
	return (n!=0);
}
s_times()
{
	struct tbuffer tb;

	times(&tb);
	ptime(tb.tb_cutime);
	ptime(tb.tb_cstime);
	prints("\n");
	return (0);
}
s_trap()
{
	register char **vp;
	register char *cp;
	register int err;

	err = 0;
	if (nargc==1)
		return (telltrp());
	vp = ++nargv;
	cp = *vp;
	if (class(cp[0], MDIGI)
	 && (cp[1]=='\0' || (class(cp[1], MDIGI) && cp[2]=='\0')))
		cp = NULL;
	else
		++vp;
	while (*vp) {
		if (class(vp[0][0], MDIGI))
			err |= setstrp(atoi(*vp++), cp);
		else {
			printe("Bad trap: %s", *vp++);
			err |= 1;
		}
	}
	return (err);
}
s_umask()
{
	if (nargc < 2)
		prints("%03o\n", ufmask);
	else
		umask(ufmask = atoi(nargv[1]));
	return (0);
}
s_wait()
{
	register int f;

	f = (nargc > 1) ? atoi(nargv[1]) : 0;
	if (f > 0)
		f = -f;
	waitc(f);
	return (slret);
}

/*
 * The set command.  This is also called from `main' to set
 * options from the command line.  In this case `flag' is
 * set.
 */
set(argc, argv, flag)
register char *argv[];
{
	int n;
	register char *cp;

	if (flag) {
		cflag = 0;
		iflag = 0;
		sflag = 0;
	}
	n = 0;
	if (argc > 0)
		n = 1;
	if (argc>1 && argv[1][0]=='-') {
		register char *fp;

		n++;
		for (cp = &argv[1][1]; *cp; cp++) {
			if ((fp=index(shfnams, *cp)) == NULL
			 || (fp+=shflags-shfnams) == NULL
			 || (fp != &lgnflag && fp > &xflag && flag == 0))
				printe("-%c: Bad option", *cp);
			else if (fp != &lgnflag)
				*fp = *cp;
		}
		if (cp == &argv[1][1]) {
			vflag = 0;
			xflag = 0;
		}
		if (flag == 0 && argc == 2)
			return (errflag);
	}
	if (errflag)
		return (1);
	if (sargv != NULL) {
		vfree(sargv);
	}
	sargv = vdupl(argv);
	sargc = argc - n;
	sargp = sargv + n;
	return (0);
}

/*
 * print the time as XmX.Xs
 * as fixed by Randall.
 */
ptime(t)
long t;
{
	register int ticks, tenths, seconds;

	prints("%Dm", t/MINUTE);
	ticks = t%MINUTE;
	seconds = ticks/SECOND;
	tenths = (ticks%SECOND + SECOND/20)/(SECOND/10);
	if (tenths == 10) {
		tenths = 0;
		seconds++;
	}
	prints("%d.%ds ", seconds, tenths);
}
