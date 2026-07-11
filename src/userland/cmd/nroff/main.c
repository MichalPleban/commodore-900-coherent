/*
 * Nroff/Troff.
 * Main programme and initialisation.
 */
#include <stdio.h>
#include <ctype.h>
#include <types.h>
#include <time.h>
#include "roff.h"
#include "code.h"
#include "env.h"
#include "esc.h"
#include "div.h"
#include "reg.h"
#include "str.h"

main(argc, argv)
register char *argv[];
{
	char name[2];
	int filflag, tinflag;
	register REG *rp;
	register int i;

	devparm();
	initialise();
	filflag = 0;
	tinflag = 0;
	for (i=1; i<argc; i++) {
		if (argv[i][0] != '-') {
			filflag = 1;
			if (adsfile(argv[i]) != 0)
				process();
			continue;
		}
		switch (argv[i][1]) {
		case 'a':
			antflag = 1;
			continue;
		case 'd':
			debflag++;
			continue;
		case 'i':
			tinflag++;
			continue;
		case 'm':
			sprintf(miscbuf, "%stmac.%s", TMACDIR, &argv[i][2]);
			adsfile(miscbuf);
			process();
			continue;
		case 'n':
			pno = atoi(&argv[i][2]);
			npn = 1 + pno;
			continue;
		case 'r':
			if ((name[0]=argv[i][2]) == '\0')
				continue;
			name[1] = '\0';
			rp = getnreg(name);
			rp->r_nval = atoi(&argv[i][3]);
			continue;
		case 'T':
			if (ntroff == NROFF)
				tntflag = 1;
			continue;
		case 'x':
			ntrflag++;
			continue;
		default:
			fprintf(stderr, "Bad option: %s\n", argv[i]);
			exit(1);
		}
	}
	if (filflag==0 || tinflag!=0) {
		adsunit(stdin);
		process();
	}
	leave();
}

/*
 * Open temp file, set up registers and general initialisation.
 */
initialise()
{
	char *np;
	register REG *rp;
	register REQ *qp;
	register int i;
	char	*mktemp();

#if RSX
	if ((tmp=fopen("rofftmp", "r+w")) == NULL)
		panic("Cannot open temp file");
	fmkdl(tmp);
#else
	np = mktemp("/tmp/rofXXXXXX");
	if ((tmp=fopen(np, "w")) == NULL)
		panic("Cannot create temp file");
	else if (freopen(np, "r+w", tmp) == NULL)
		panic("Cannot reopen temp file");
	unlink(np);
#endif
	tmpseek = ENVSIZE * sizeof (ENV);
	tmpseek = (tmpseek+DBFSIZE+DBFSIZE-1) & ~(DBFSIZE-1);

	for (i=0; i<RHTSIZE; i++)
		regt[i] = NULL;
	for (qp=reqtab; qp->q_name[0]; qp++) {
		rp = makereg(qp->q_name, RTEXT);
		rp->r_macd.m_next = NULL;
		rp->r_macd.m_type = MREQS;
		rp->r_macd.m_func = qp->q_func;
	}
	nrpnreg = getnreg("%");
	nrctreg = getnreg("ct");
	nrdlreg = getnreg("dl");
	nrdnreg = getnreg("dn");
	nrdwreg = getnreg("dw");
	nrdyreg = getnreg("dy");
	nrhpreg = getnreg("hp");
	nrlnreg = getnreg("ln");
	nrmoreg = getnreg("mo");
	nrnlreg = getnreg("nl");
	nrsbreg = getnreg("sb");
	nrstreg = getnreg("st");
	nryrreg = getnreg("yr");
	setnreg();

	for (i=0; i<ENVSIZE; i++)
		envinit[i] = 0;
	envs = 0;
	envstak[envs] = 0;
	setenvr();
	envinit[0] = 1;

	cdivp = NULL;
	newdivn("\0\0");
	mdivp = cdivp;

	endtrap[0] = '\0';
	outflag = 0;
	strp = NULL;
	pgl = unit(11*SMINCH, SDINCH);
	pct = 0;
	pno = 1;
	nrpnreg->r_nval = 1;
	npn = 2;
	esc = '\\';
	svs = 0;
	nbrflag = 0;
	byeflag = 0;
	ifeflag = 0;
	ntrflag = 0;
	debflag = 0;
	antflag = ntroff==NROFF;
	tntflag = 0;
	nrorval = 0;
}

/*
 * Initialise pre-defined number registers.
 */
setnreg()
{
#if RSX
	int timebuf[8];

	time(timebuf);
	nryrreg->r_nval = timebuf[0];
	nrmoreg->r_nval = timebuf[1];
	nrdyreg->r_nval = timebuf[2];
#else
	time_t time();
	time_t curtime;
	register struct tm *tmp;

	curtime = time((time_t *)0);
	tmp = localtime(&curtime);
	nryrreg->r_nval = tmp->tm_year%100;
	nrmoreg->r_nval = tmp->tm_mon+1;
	nrdyreg->r_nval = tmp->tm_mday;
	nrdwreg->r_nval = tmp->tm_wday + 1;
#endif
}

/*
 * Leave.
 */
leave()
{
	char name[2];

	if (endtrap[0] != '\0') {
		name[0] = endtrap[0];
		name[1] = endtrap[1];
		endtrap[0] = '\0';
		execute(name);
	}
	setbreak();
	if (ntrflag == 0) {
		byeflag = 1;
		pspace(0);
	}
	exit(0);
}
