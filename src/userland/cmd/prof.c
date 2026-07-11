/*
 * Prof interprets the mon.out files produced by monitor(3) and hence by
 * the cc(1) option -p.  The usage is as follows:
 *	prof [-a] [-b] [-c] [-s] [l.out_file [mon.out_file]]
 * The -a option indicates that all symbols should be used (as opposed to
 * only external symbols).  The -b option indicates that we should print
 * all bin information (to detect hot spots).  The -c option indicates
 * that all call information should be printed.  The -s option asks for stack
 * depth information.
 */
#include <stdio.h>
#include <l.out.h>
#include <const.h>
#include <canon.h>
#include "mon.h"


/*
 * Note that in setting PSCALE, one must guard against overflow.
 * Examine putdata carefully before changeing these constants.
 * Also note that certain divisibility properties are assumed
 * there reguarding PSCALE and HZ.
 */
#define PSCALE	((long)100)		/* pc count scale factor */
#define	SWIDTH	10			/* default symbol widht */
#define	EOS	'\0'			/* string terminator */
#define	REL	1			/* relative position for fseek */
#define	TRUE	(0 == 0)
#define	FALSE	(0 != 0)


typedef struct	symbol {
	char		name[NCPLN];
	vaddr_t		addr;
	long		pcount,		/* pc count, scaled by PSCALE */
			ccount;		/* number of times routine called */
}	symbol;

char		*lout	= "l.out",
		*monout	= "mon.out";
symbol		**dict;			/* NULL terminated list of symbols */
int		dsize,			/* number of symbols */
		allsyms	= FALSE,	/* iff we use all symbols */
		bindmp	= FALSE,	/* iff we should dump bin info */
		calldmp	= FALSE;	/* iff we should dump call info */
int		stkdmp = FALSE;		/* iff we should dump low stack mark */
long		tticks,			/* total number of clock ticks */
		tcalls;			/* total number of calls */
vaddr_t		lowpc;			/* lowest pc profiled */
unsigned	scale;			/* mon.out scale factor over 2 */
vaddr_t		stksz;			/* Stack size */


main(argc, argv)
int		argc;
register char	*argv[];
{
	register char	*cp,
			ch;

	for (cp=*++argv; cp != NULL  &&  *cp++ == '-'; cp=*++argv)
		while ((ch=*cp++) != EOS)
			switch (ch) {
				case 'a':
					allsyms = TRUE;
					break;
				case 'b':
					bindmp = TRUE;
					break;
				case 'c':
					calldmp = TRUE;
					break;
				case 's':
					stkdmp = TRUE;
					break;
				default:
					usage();
			}
	if (*argv != NULL)
		lout = *argv++;
	if (*argv != NULL)
		monout = *argv++;
	if (*argv != NULL)
		usage();
	getsyms();
	getdata();
	if (stkdmp)
		printf("%u bytes stack used\n", stksz);
	if (! bindmp  &&  ! calldmp)
		putdata();
	return(0);
}


die(str)
char	*str;
{
	fprintf(stderr, "%r\n", &str);
	exit(1);
}


usage()
{
	die("Usage: prof [-a] [-b] [-c] [-s] [l.out_file [mon.out_file]]");
}


/*
 * Getsyms reads in the symbols from an l.out file and sets dict to
 * an array of them, in sorted order.
 */
getsyms()
{
	FILE		*fp;
	long		skip;
	struct ldheader	hdr;
	int		cmpsym();

	fp = fopen(lout, "r");
	if (fp == NULL)
		die("Can't open %s", lout);
	if (fread(&hdr, sizeof hdr, 1, fp) != 1 || hdr.l_magic != L_MAGIC)
		die("%s not l.out file", lout);
	cansize(hdr.l_ssize[L_SHRI]);
	cansize(hdr.l_ssize[L_PRVI]);
	cansize(hdr.l_ssize[L_SHRD]);
	cansize(hdr.l_ssize[L_PRVD]);
	cansize(hdr.l_ssize[L_DEBUG]);
	cansize(hdr.l_ssize[L_SYM]);
	skip = hdr.l_ssize[L_SHRI] + hdr.l_ssize[L_PRVI]
		+ hdr.l_ssize[L_SHRD] + hdr.l_ssize[L_PRVD]
		+ hdr.l_ssize[L_DEBUG];
	fseek(fp, skip, REL);
	readsy((int)(hdr.l_ssize[L_SYM]/sizeof (struct ldsym)), fp);
	qsort(dict, dsize, sizeof *dict, cmpsym);
	fclose(fp);
}


/*
 * Cmpsym compares the two symbols `sp1' and `sp2', and returns an
 * int corresponding to the relative order of the address fields.
 */
cmpsym(sp1, sp2)
symbol	**sp1,
	**sp2;
{
	register vaddr_t	adr1,
				adr2;

	adr1 = (*sp1)->addr;
	adr2 = (*sp2)->addr;
	if (adr1 > adr2)
		return (1);
	else if (adr1 == adr2)
		return (0);
	else
		return (-1);
}


/*
 * Readsy reads in nsyms ldsyms from the FILE fp.  It sets dict to
 * an array of the resulting symbols.
 */
readsy(nsyms, fp)
register int	nsyms;
FILE		*fp;
{
	register symbol	**dpp,
			*dp;
	struct ldsym	lsym;
	char		*alloc();

	dict = dpp = (symbol **)alloc((nsyms + 1) * sizeof *dpp);
	while (--nsyms >= 0) {
		if (fread(&lsym, sizeof lsym, 1, fp) != 1)
			die("Unexpected end of file on %s", lout);
		if ((lsym.ls_type & ~L_GLOBAL) > L_BSSI)
			continue;
		if ((lsym.ls_type & L_GLOBAL) == 0 && ! allsyms)
			continue;
		dp = (symbol *)alloc(sizeof *dp);
		strncpy(dp->name, lsym.ls_id, NCPLN);
		dp->addr = lsym.ls_addr;
		dp->pcount = dp->ccount = 0;
		*dpp++ = dp;
	}
	*dpp = NULL;
	dsize = dpp - dict;
	dict = (symbol *)realloc(dict, (dpp + 1 - dict) * sizeof *dpp);
}


/*
 * Alloc is simply an interface to malloc which exits if there is no
 * room.
 */
char	*
alloc(size)
unsigned	size;
{
	char	*result;

	result = (char *)malloc(size);
	if (result == NULL)
		die("Out of space");
	return (result);
}


/*
 * Getdata reads in the mon.out file and puts the information into
 * the dictionary.
 */
getdata()
{
	FILE		*fp;
	struct m_hdr	hdr;

	fp = fopen(monout, "r");
	if (fp == NULL)
		die("Can't open %s", monout);
	if (fread(&hdr, sizeof hdr, 1, fp) != 1)
		die("%s not mon.out file", monout);
	scale = hdr.m_scale/2;
	lowpc = hdr.m_lowpc;
	stksz = hdr.m_hisp - hdr.m_lowsp;
	if (calldmp  ||  ! bindmp)
		getcdata(fp, hdr.m_nfuncs);
	else
		fseek(fp, hdr.m_nfuncs * (long)sizeof (struct m_func), REL);
	if (bindmp  ||  ! calldmp)
		getpdata(fp, hdr.m_nbins);
}


/*
 * Getcdata reads in the function call information from the mon.out file.
 */
getcdata(fp, nfnc)
FILE	*fp;
register unsigned	nfnc;
{
	register symbol	**dpp,
			*dp;
	struct m_func	buf;

	while (nfnc-- != 0) {
		if (fread(&buf, sizeof buf, 1, fp) != 1)
			die("unexpected end of file on %s", monout);
		for (dpp=dict; (dp=*++dpp) != NULL && dp->addr <= buf.m_addr;)
			;
		if (calldmp)
			printf("%4ld %*.*s+%u\n", buf.m_ncalls, SWIDTH, NCPLN,
				dpp[-1]->name, buf.m_addr - dpp[-1]->addr);
		tcalls += buf.m_ncalls;
		dpp[-1]->ccount += buf.m_ncalls;
	}
}


/*
 * Getpdata reads in the profileing data and increments the corresponding
 * symbols pcount fields.
 * Note that the global scale must contain the mon.out scale divided
 * by 2.
 */
getpdata(fp, nbins)
FILE		*fp;
unsigned	nbins;
{
	register symbol	**dpp;
	vaddr_t		high,
			low;
	int		highr,
			inc,
			incr;
	short		tick;
	symbol		**credit();

	high = lowpc;
	highr = 0;
	inc = ((long)1<<16) / scale;
	incr = ((long)1<<16) % scale;
	if (incr != 0) {
		++inc;
		incr -= scale;
	}
	for (dpp=dict; nbins > 0; --nbins) {
		low = high;
		high += inc;
		highr += incr;
		if (-highr >= scale) {
			--high;
			highr += scale;
		}
		if (fread(&tick, sizeof tick, 1, fp) != 1)
			die("Unexpected end of file on %s\n", monout);
		if (tick == 0)
			continue;
		tticks += tick;
		dpp = credit(tick, low, high, dpp);
	}
}


symbol	**
credit(tick, low, high, dpp)
int	tick;
vaddr_t	low,
	high;
symbol	**dpp;
{
	register unsigned	overlap;
	register symbol		*cur,
				*nxt;
	unsigned		binlen;

	binlen = high - low;
	nxt = *dpp;
	if (nxt == NULL  ||  nxt->addr >= high) {
		if (bindmp)
			printf("%3d %06o %06o\n", tick, low, high-1);
		return(dpp);
	}
	do {
		cur = nxt;
		nxt = *++dpp;
	} while (nxt != NULL  &&  nxt->addr <= low);
	if (bindmp)
		printf("%3d %*.*s+%-4u ", tick, SWIDTH, NCPLN, cur->name,
			low - cur->addr);
	do {
		if (nxt != NULL  &&  nxt->addr < high)
			overlap = nxt->addr;
		else
			overlap = high;
		if (cur->addr > low)
			overlap -= cur->addr;
		else
			overlap -= low;
		cur->pcount += (PSCALE*overlap*tick + binlen/2) / binlen;
		cur = nxt;
		nxt = *++dpp;
	} while (cur != NULL && cur->addr < high);
	dpp -= 2;
	if (bindmp)
		printf("%*.*s+%u\n", SWIDTH, NCPLN, dpp[0]->name,
			high - 1 - dpp[0]->addr);
	return (dpp);
}


/*
 * Putdata prints out the results which have been tabulated in the
 * dictionary.
 */
putdata()
{
	register symbol	**dpp,
			*dp;
	int		cmpdata();
	char		*centi();

	qsort(dict, dsize, sizeof *dict, cmpdata);
	for (dpp=dict; (dp=*dpp++) != NULL;) {
		if (dp->pcount == 0 && dp->ccount == 0)
			continue;
		printf("%-*.*s", SWIDTH, NCPLN, dp->name);
		centi(dp->pcount, (PSCALE * tticks)/100, 2);
		putchar('%');
		if (dp->ccount != 0) {
			printf(" %7ld ", dp->ccount);
			centi((1000*dp->pcount) / (PSCALE*10),
				(HZ*dp->ccount) / 10, 3);
		}
		putchar('\n');
	}
}


/*
 * Cmpdata compares the data in the dictionary entries `sp1'
 * and `sp2'.  It returns an int which reflects which entry should
 * be listed first.  If the value is positive, `sp2' should occur
 * first.  If it is zero, it makes no difference.  If it is negative,
 * `sp1' should occur first.
 */
cmpdata(sp1, sp2)
symbol	**sp1,
	**sp2;
{
	register symbol	*adr1,
			*adr2;
	long		rel;

	adr1 = *sp1;
	adr2 = *sp2;
	rel = adr2->pcount - adr1->pcount;
	if (rel == 0)
		rel = adr2->ccount - adr1->ccount;
	if (rel > 0)
		return (1);
	else if (rel < 0)
		return (-1);
	else
		return (strncmp(adr1->name, adr2->name, NCPLN));
}


/*
 * Centi prints out on standard output `num' / `den' to two places, with
 * atleast `width' places to the left of the decimal point.
 */
char	*
centi(num, den, width)
long	num,
	den;
int	width;
{
	long	cv;

	cv = (num*100 + den/2) / den;
	printf("%*ld.%02d", width, cv/100, (int)(cv%100));
}
