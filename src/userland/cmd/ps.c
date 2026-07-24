/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Print out process statuses.
 */
#include <stdio.h>
#include <ctype.h>
#include <const.h>
#include <l.out.h>
#include <pwd.h>
#include <proc.h>
#include <sched.h>
#include <seg.h>
#include <stat.h>
#include <uproc.h>

/*
 * Maximum sizes.
 */
#define ARGSIZE	512

/*
 * Functions to see if a pointer is in alloc space and to map a
 * pointer from alloc space to this process.
 */
#define range(p)	(p>=(char *)(aend) && p<(char *)(aend)+casize)
#define map(p)		(&allp[(char *)p - (char *)aend])

/*
 * For easy referencing.
 */
#define	aprocq		nl[0].n_value
#define	autime		nl[1].n_value
#define astime		nl[2].n_value
#define aasize		nl[3].n_value
#define	aend		nl[4].n_value

/*
 * Variables.
 */
int	aflag;				/* All processes */
int	dflag;				/* Debug flag */
int	fflag;				/* Print out all fields */
int	lflag;				/* Long format */
int	mflag;				/* Print scheduling values */
int	nflag;				/* No header */
int	rflag;				/* Print out real sizes */
int	tflag;				/* Print times */
int	wflag;				/* Wide format */
int	xflag;				/* Get special processes */
dev_t	ttdev;				/* Terminal device */

/*
 * Table for namelist.
 */
struct nlist nl[] ={
	"procq_",	0,	0,
	"utimer_",	0,	0,
	"stimer_",	0,	0,
	"asize_",	0,	0,
	"end_",		0,	0,
	""
};

/*
 * Symbols.
 */
char	 *allp;				/* Pointer to alloc space */
char	*kfile;				/* Kernel data memory file */
char	 *nfile;			/* Namelist file */
char	 *mfile;			/* Memory file */
char	 *dfile;			/* Swap file */
char	 argp[ARGSIZE];			/* Arguments */
int	kfd;				/* Kernel memory file descriptor */
int	 mfd;				/* Memory file descriptor */
int	 dfd;				/* Swap file descriptor */
struct	 uproc u;	 		/* User process area */
int	 cutime;			/* Unsigned time */
PROC	 cprocq;			/* Process queue header */
unsigned casize;			/* Size of alloc area */

char	*uname();

main(argc, argv)
char *argv[];
{
	register int i;
	register char *cp;

	initialise();
	for (i=1; i<argc; i++) {
		for (cp=&argv[i][0]; *cp; cp++) {
			switch (*cp) {
			case '-':
				continue;
			case 'a':
				aflag++;
				continue;
			case 'c':
				if (++i >= argc)
					usage();
				nfile = argv[i];
				continue;
			case 'd':
				dflag++;
				continue;
			case 'f':
				fflag++;
				continue;
			case 'k':
				if (++i >= argc)
					usage();
				mfile = argv[i];
				continue;
			case 'l':
				lflag++;
				continue;
			case 'm':
				mflag++;
				continue;
			case 'n':
				nflag++;
				continue;
			case 'r':
				rflag++;
				continue;
			case 't':
				tflag++;
				continue;
			case 'w':
				wflag++;
				continue;
			case 'x':
				xflag++;
				continue;
			default:
				usage();
			}
		}
	}
	execute();
	exit(0);
}

/*
 * Initialise.
 */
initialise()
{
	register char *cp;

	aflag = 0;
	lflag = 0;
	mflag = 0;
	nflag = 0;
	xflag = 0;
	nfile = "/coherent";
	kfile = "/dev/kmem";
	mfile = "/dev/mem";
	dfile = "/dev/swap";
	if ((cp=malloc(BUFSIZ)) != NULL)
		setbuf(stdout, cp);
}

/*
 * Print out usage.
 */
usage()
{
	panic("Usage: ps [-acdklmnrtwx]");
}

/*
 * Print out information about processes.
 */
execute()
{
	int c, l;
	register PROC *pp1, *pp2;

	nlist(nfile, nl);
	if (nl[0].n_type == 0)
		panic("Bad namelist file %s", nfile);
	if ((mfd=open(mfile, 0)) < 0)
		panic("Cannot open %s", mfile);
	if ((kfd = open(kfile, 0)) < 0)
		panic("Cannot open %s", kfile);
	if ((dfd=open(dfile, 0)) < 0)
		panic("Cannot open %s", dfile);
	kread((long)aasize, &casize, sizeof (casize));
	kread((long)aprocq, &cprocq, sizeof (cprocq));
	if ((allp=malloc(casize)) == NULL)
		panic("Out of core");
	kread((long)aend, allp, casize);
	kread((long)autime, &cutime, sizeof (cutime));
	settdev();
	l = 9;
	if (dflag)
		l += 7;
	if (lflag)
		l += 31;
	if (mflag)
		l += 26;
	if (tflag)
		l += 12;
	if (nflag == 0) {
		if (dflag)
			printf("       ");
		printf("TTY   PID");
		if (lflag)
			printf("  PPID    UID   K    F S  EVENT");
		if (mflag)
			printf("  CVAL  SVAL   IVAL   RVAL");
		if (tflag)
			printf(" UTIME STIME");
		putchar('\n');
		fflush(stdout);
	}
	pp1 = &cprocq;
	while ((pp2=pp1->p_nback) != aprocq) {
		if (range((char *)pp2) == 0)
			panic("Fragmented list");
		pp1 = (PROC *) map(pp2);
		if (xflag==0 && pp1->p_ttdev==NODEV)
			continue;
		if (aflag==0 && pp1->p_ttdev!=ttdev)
			continue;
		if (dflag)
			printf("%06o ", pp2);
		ptty(pp1);
		printf(" %5d", pp1->p_pid);
		if (lflag) {
			printf(" %5d", pp1->p_ppid);
			printf(" %6s", uname(pp1));
			psize(pp1);
			printf(" %4o", pp1->p_flags);
			printf(" %c", c=state(pp1, pp2));
			if (c == 'S') {
				putchar(' ');
				printf(AFMT, pp1->p_event);
			} else {
				if (fflag)
					printf("      -");
				else
					printf("       ");
			}
		}
		if (mflag) {
			printf(" %5u", cval(pp1, pp2));
			printf(" %5u", pp1->p_sval);
			printf(" %6d", pp1->p_ival);
			printf(" %6d", pp1->p_rval);
		}
		if (tflag) {
			ptime(pp1->p_utime);
			ptime(pp1->p_stime);
		}
		printf("  ");
		printl(pp1, (wflag?132:80)-l-1);
		putchar('\n');
		fflush(stdout);
	}
}

/*
 * Set our terminal device.
 */
settdev()
{
	register PROC *pp1, *pp2;
	register int p;

	p = getpid();
	pp1 = &cprocq;
	while ((pp2=pp1->p_nforw) != aprocq) {
		if (range((char *)pp2) == 0)
			break;
		pp1 = (PROC *) map(pp2);
		if (pp1->p_pid == p) {
			ttdev = pp1->p_ttdev;
			return;
		}
	}
	ttdev = NODEV;
}

/*
 * Print out a terminal name.
 */
ptty(pp)
register PROC *pp;
{
	register int d;

	if ((d=pp->p_ttdev) == NODEV) {
		printf("??:");
		return;
	}
	printf("%o", major(d));
	if (minor(d) < 10)
		printf("%d", minor(d));
	else
		printf("%c", 'a'-10+minor(d));
	printf(":");
}

/*
 * Given a process, return it's user name.
 */
char *
uname(pp)
register PROC *pp;
{
	static char name[8];
	register struct passwd *pwp;

	if ((pwp=getpwuid(pp->p_ruid)) != NULL)
		return (pwp->pw_name);
	sprintf(name, "%d", pp->p_ruid);
	return (name);
}

/*
 * Return the core value for a process.
 */
cval(pp1, pp2)
register PROC *pp1, *pp2;
{
	unsigned u;
	register PROC *pp3, *pp4;

	if (pp1->p_state == PSSLEEP) {
		u = (cutime-pp1->p_lctim) * 1;
		if (pp1->p_cval+u > pp1->p_cval)
			return (pp1->p_cval+u);
		return (-1);
	}
	u = 0;
	pp3 = &cprocq;
	while ((pp4=pp3->p_lforw) != aprocq) {
		if (range((char *)pp4) == 0)
			break;
		pp3 = (PROC *) map(pp4);
		u -= pp3->p_cval;
		if (pp2 == pp4)
			return (u);
	}
	if (pp1->p_pid == getpid())
		return (pp1->p_cval);
	return (0);
}

/*
 * Return the size in K of the given process.
 */
psize(pp)
register PROC *pp;
{
	long l;
	register SEG *sp;
	register int n;

	l = 0;
	for (n=0; n<NUSEG+1; n++) {
		if (rflag == 0)
			if (n==SIUSERP || n==SIAUXIL)
				continue;
		if ((sp=pp->p_segp[n]) == NULL)
			continue;
		if (range((char *)sp) == 0) {
			printf("  ?K");
			return;
		}
		sp = (SEG *) map(sp);
		l += ctob(sp->s_size);
	}
	if (l != 0)
		printf("%3ldK", l/1024);
	else {
		if (fflag)
			printf("   -");
		else
			printf("    ");
	}
}

/*
 * Get the state of the process.
 */
state(pp1, pp2)
register PROC *pp1, *pp2;
{
	register int s;

	s = pp1->p_state;
	if (s == PSSLEEP) {
		if ((PROC *)pp1->p_event == pp2)
			return ('W');
		if ((pp1->p_flags&PFSTOP) != 0)
			return ('T');
		return ('S');
	}
	if (s == PSRUN)
		return ('R');
	if (s == PSDEAD)
		return ('Z');
	return ('?');
}

/*
 * Given a time in HZ, print it out.
 */
ptime(l)
long l;
{
	register unsigned m;

	if ((l=(l+HZ/2)/HZ) == 0) {
		if (fflag)
			printf("     -");
		else
			printf("      ");
		return;
	}
	if ((m=l/60) >= 100) {
		printf("%6d", m);
		return;
	}
	printf(" %2d:%02d", m, (unsigned)l%60);
}

/*
 * Print out the command line of a process.
 */
printl(pp, m)
register PROC *pp;
{
	register char *cp;
	register int c;
	register int argc;
	register int n;
	register SEG *sp;

	if (pp->p_state == PSDEAD)
		return;
	if (pp->p_pid == 0) {
		printf("<idle>");
		return;
	}
	if (pp->p_event == astime) {
		printf("<swap>");
		return;
	}
	if ((pp->p_flags & PFSLIB) != 0) {
		printf("<slib>");
		return;
	}
	if (segread(pp->p_segp[SIUSERP], 0, &u, sizeof (u)) == 0)
		return;
	if ((sp = pp->p_segp[SISTACK]) == NULL)
		sp = pp->p_segp[SIPDATA];
	n = segread(sp, (unsigned)(u.u_argp-u.u_segl[SISTACK].sr_base),
		argp, sizeof (argp));
	if (n == 0)
		return;
	if ((argc=u.u_argc) <= 0)
		return;
	m -= 2;
	cp = argp;
	while (argc--) {
		while ((c=*cp++) != '\0') {
			if (!isascii(c) || !isprint(c))
				return;
			if (m-- == 0)
				return;
			putchar(c);
		}
		if (m-- == 0)
			return;
		if (argc != 0)
			putchar(' ');
	}
}

/*
 * Given a segment pointer `sp' and an offset into the segment,
 * `s', read `n' bytes from the segment into the buffer `bp'.
 */
segread(sp, s, bp, n)
SEG *sp;
char *bp;
{
	register SEG *sp1;

	if (range((char *)sp) == 0)
		return (0);
	sp1 = (SEG *) map(sp);
	if ((sp1->s_flags&SFCORE) != 0)
		mread(ctob((long)sp1->s_mbase) + s, bp, n);
	else
		dread((long)sp1->s_dbase*BSIZE + s, bp, n);
	return (1);
}

/*
 * Read `n' bytes into the buffer `bp' from kernel memory
 * starting at seek position `s'.
 */
kread(s, bp, n)
long s;
char *bp;
{
#if 1
	unsigned x;

	x = s;
	s = x;
#endif
	lseek(kfd, s, 0);
	if (read(kfd, bp, n) != n)
		panic("Kernel memory read error");
}

/*
 * Read `n' bytes into the buffer `bp' from absolute memory
 * starting at seek position `s'.
 */
mread(s, bp, n)
long s;
char *bp;
{
	lseek(mfd, s, 0);
	if (read(mfd, bp, n) != n)
		panic("Memory read error");
}

/*
 * Read `n' bytes into the buffer `bp' from the swap file
 * starting at seek position `s'.
 */
dread(s, bp, n)
long s;
char *bp;
{
	lseek(dfd, s, 0);
	if (read(dfd, bp, n) != n)
		panic("Swap read error");
}

/*
 * Print out an error message and exit.
 */
panic(a1)
char *a1;
{
	fflush(stdout);
	fprintf(stderr, "%r", &a1);
	fprintf(stderr, "\n");
	exit(1);
}
