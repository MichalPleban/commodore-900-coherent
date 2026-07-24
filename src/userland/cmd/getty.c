/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Read a user name from a terminal.
 * Attempt to determine the proper line
 * speed and mode. Call login. This version
 * asks the tty for the erase and kill
 * characters, so that whatever the system
 * sets up in "ttopen" will be preserved.
 */
#include <stdio.h>
#include <ctype.h>
#include <sgtty.h>
#include <signal.h>

#define	NTYPE	(sizeof stypes/sizeof stypes[0])
#define	GMODE	(ECHO|XTABS|CRT)
#define	LA36M	GMODE			/* Should have LA36 delays w/a */
#define	MAXNAME	50			/* Maximum characters in a name */

char	defm[]	= "\n\rCoherent login: "; /* default login prompt */

struct	sgttyb	isgttyb;		/* Initial stty */
struct	tchars	itchars;		/* Initial tchars */
int	xflags;				/* Extra flags to set */
int	intflag;			/* Interrupt */

/*
 * Structure for each speed table.
 * The `s_nindex' entry is the next
 * indent in the current table to
 * use (for fixed-speed entries, it
 * should be always 0).
 */
struct	stable	{
	char	s_nindex;
	int	s_mode;
	char	s_ispeed;
	char	s_ospeed;
	char	*s_lmsg;
};

/*
 * Fixed-speed tables.
 */
struct	stable	sA[] = {0, GMODE, B50, B50, defm};
struct	stable	sB[] = {0, GMODE, B75, B75, defm};
struct	stable	sC[] = {0, GMODE, B110, B110, defm};
struct	stable	sD[] = {0, GMODE, B134, B134, defm};
struct	stable	sE[] = {0, GMODE, B150, B150, defm};
struct	stable	sF[] = {0, GMODE, B200, B200, defm};
struct	stable	sG[] = {0, GMODE, B300, B300, defm};
struct	stable	sH[] = {0, GMODE, B600, B600, defm};
struct	stable	sI[] = {0, GMODE, B1200, B1200, defm};
struct	stable	sJ[] = {0, GMODE, B1800, B1800, defm};
struct	stable	sK[] = {0, GMODE, B2000, B2000, defm};
struct	stable	sL[] = {0, GMODE, B2400, B2400, defm};
struct	stable	sM[] = {0, GMODE, B3600, B3600, defm};
struct	stable	sN[] = {0, GMODE, B4800, B4800, defm};
struct	stable	sO[] = {0, GMODE, B7200, B7200, defm};
struct	stable	sP[] = {0, GMODE, B9600, B9600, defm};
struct	stable	sQ[] = {0, GMODE, B19200, B19200, defm};
struct	stable	sR[] = {0, GMODE, EXTA, EXTA, defm};
struct	stable	sS[] = {0, GMODE, EXTB, EXTB, defm};

/*
 * Variable-speed tables.
 */
struct	stable	sdash[] = {		/* TTY 33 */
	0,	GMODE,	B110,	B110,	defm,
};

struct	stable	s0[] = {		/* Default--various terminals */
	1,	GMODE,	B300,	B300,	defm,
	2,	GMODE,	B1200,	B1200,	defm,
	3,	GMODE,	B150,	B150,	defm,
	0,	GMODE,	B110,	B110,	defm,
};

struct	stable	s1[] = {		/* 150 baud TTY 37 */
	0,	GMODE,	B150,	B150,	defm,
};

struct	stable	s2[] = {		/* 9600 baud - Tek 4104 */
	0,	GMODE,	B9600,	B9600,	defm,
};

struct	stable	s3[] = {		/* 212 datasets (1200,300,...) */
	1,	GMODE,	B1200,	B1200,	defm,
	0,	GMODE,	B300,	B300,	defm,
};

struct	stable	s5[] = {		/* Reverse of `s3' */
	1,	GMODE,	B300,	B300,	defm,
	0,	GMODE,	B1200,	B1200,	defm,
};

struct	stable	s4[] = {		/* LA36 (needs its delays) */
	0,	LA36M,	B300,	B300,	defm,
};

/*
 * This table is used to map
 * a speed character argument, passed
 * to "getty" from "init", into a speed
 * table base.
 */
struct	stypes	{
	int	s_name;
	struct	stable	*s_ptr;
}	stypes[] = {
	'-',	sdash,
	'0',	s0,
	'1',	s1,
	'2',	s2,
	'3',	s3,
	'4',	s4,
	'5',	s5,
	'A',	sA,
	'B',	sB,
	'C',	sC,
	'D',	sD,
	'E',	sE,
	'F',	sF,
	'G',	sG,
	'H',	sH,
	'I',	sI,
	'J',	sJ,
	'K',	sK,
	'L',	sL,
	'M',	sM,
	'N',	sN,
	'O',	sO,
	'P',	sP,
	'Q',	sQ,
	'R',	sR,
	'S',	sS
};

main(argc, argv)
char *argv[];
{
	register struct stable	*sp;
	register struct stable	*stpp;
	register int	index;
	extern	 int	catch();
	char		name[MAXNAME];

	ioctl(1, TIOCGETP, &isgttyb);
	ioctl(1, TIOCGETC, &itchars);
	signal(SIGINT, &catch);
	stpp = s0;
	if (argc > 1) {
		register struct stypes *stp;

		for (stp = stypes; stp < &stypes[NTYPE]; stp++)
			if (*argv[1] == stp->s_name) {
				stpp = stp->s_ptr;
				break;
			}
	}
	for (index=0; ; index=sp->s_nindex) {
		sp = &stpp[index];
		xflags = 0;
		dostty(sp, RAW, 0);
		if (readname(name, sp) != 0)
			break;
	}
	dostty(sp, 0, RAW);
	execl("/bin/login", "-", name, NULL);
	exit(1);
}

/*
 * Do a "stty" on the terminal.
 * The erase and kill characters come
 * from the defaults in "isgttyb".
 * The speeds and the basic mode come
 * from the "stable" pointed to by
 * "sp". The "on" and "off" masks
 * change the modes.
 */
dostty(stabp, on, off)
register struct stable *stabp;
{
	struct sgttyb	sgttyb;

	sgttyb.sg_ispeed = stabp->s_ispeed;
	sgttyb.sg_ospeed = stabp->s_ospeed;
	sgttyb.sg_erase  = isgttyb.sg_erase;
	sgttyb.sg_kill   = isgttyb.sg_kill;
	sgttyb.sg_flags  = stabp->s_mode;
	sgttyb.sg_flags |= xflags|on;
	sgttyb.sg_flags &= ~off;
	ioctl(1, TIOCSETP, &sgttyb);
}

/*
 * Read a name from the terminal.
 * Return true if you get a name, or false
 * if the speed should be cycled. Use the
 * editing characters from the initial
 * defaults.
 */
readname(s, sp)
char	*s;
struct	stable	*sp;
{
	register char	*cp;
	register int	c;
	register int	seenupper;
	register int	seenlower;

loop:
	seenlower = 0;
	seenupper = 0;
	intflag = 0;
	xflags  = 0;
	write(1, sp->s_lmsg, strlen(sp->s_lmsg));
	cp = s;
	for (;;) {
		if ((c=getchar()&0x7F)==0 || c==itchars.t_intrc)
			return (0);
		if (intflag != 0) {
			intflag = 0;
			return (0);
		}
		if (cp==s && c==itchars.t_eofc)
			return (0);
		if (c==' ' || c=='\t' || c=='\f')
			continue;
		if (c == isgttyb.sg_kill) {
			write(1, "\r\n", 2);
			seenlower = 0;
			seenupper = 0;
			xflags = 0;
			cp = s;
			continue;
		}
		if (c == isgttyb.sg_erase) {
			if (sp != s)
				--cp;
			continue;
		}
		if (c == '\n') {
			putchar ('\r');
			break;
		}
		if (c == '\r') {
			putchar('\n');
			xflags |= CRMOD;
			break;
		}
		if (isupper(c))
			seenupper = 1;
		else if (islower(c))
			seenlower = 1;
		if (cp < s+MAXNAME)
			*cp++ = c;
	}
	if (cp == s)
		goto loop;
	*cp = 0;
	if (seenupper!=0 && seenlower==0) {
		xflags |= LCASE;
		for (cp=s; *cp!=0; ++cp) {
			if (isupper(*cp))
				*cp = tolower(*cp);
		}
	}
	return (1);
}

/*
 * Set a flag on interrupt.
 */
catch()
{
	intflag = 1;
}
