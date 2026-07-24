/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Emulate a terminal on the port
 * identified to Coherent as `/dev/modem'.
 * This program is still a subset of
 * what it will eventually do.
 */

#include <stdio.h>
#include <sgtty.h>
#include <signal.h>
#include <stat.h>
#include <errno.h>
#include <setjmp.h>
#include "cu.h"

#define	pgetc()	fgetc(infp)

#define	NINSTACK	20		/* Size of input file stack */

struct	sgttyb	old, new;
jmp_buf	env;
char	buf[400];
char	*serial = "/dev/modem";
FILE	*infp = stdin;
FILE	*infps[NINSTACK] = {stdin};
FILE	**infpp = &infps[0];		/* Pointer into input stack */
FILE	*outfp = stdout;
FILE	*out2fp = NULL;			/* Second output for redirection */
FILE	*dldfp;				/* Used by downloader */
int	pip[2];
char	sfd;				/* Serial port */
char	intty;				/* True if stdin is a tty */

struct	bauds {
	int	rate;
	int	val;
}	bauds[] = {
	50,	B50,
	75,	B75,
	110,	B110,
	134,	B134,
	150,	B150,
	200,	B200,
	300,	B300,
	600,	B600,
	1200,	B1200,
	1800,	B1800,
	2000,	B2000,
	2400,	B2400,
	3600,	B3600,
	4800,	B4800,
	7200,	B7200,
	9600,	B9600,
	19200,	B19200,
};

#define	NBAUD	(sizeof(bauds)/sizeof(bauds[0]))

char	speclist[] = "\
List of special commands. Each command must be both\n\r\
preceded and followed by a carriage return:\n\r\
~#		comment\n\r\
~~		A `~' character\n\r\
~!		invoke the shell\n\r\
~<file		redirect input from `file'\n\r\
~>[>:]file	redirect (append) (silently) to `file'\n\r\
~>		end output redireciton\n\r\
~pstring	search for pattern `string'\n\r\
~Pn		Pause `n' seconds\n\r\
~q		leave cu and hang up phone line\n\r\
~Q		leave cu without dropping phone line\n\r\
~f		enter full duplex (no echo) mode\n\r\
~h		enter half duplex (echo) mode\n\r\
~s n		set serial port speed to `n' (e.g. 300)\n\r\
";

int	hflag;			/* Half duplex operation */
int	slave = -1;		/* Child process is slave */
int	baud = 1200;		/* BPS of serial transmission */

int	texit();
int	sbreak();
int	hangup();
int	auxspec();
int	exit();
int	intend();
int	get();
int	put();

main(argc, argv)
int argc;
char *argv[];
{

	while (argc>1 && *argv[1]=='-') {
		switch (argv[1][1]) {
		case 'h':
			hflag++;
			break;

		case 's':
			argc--;
			argv++;
			if (argc < 2)
				usage();
			baud = atoi(argv[1]);
			break;

		case 'l':
			argc--;
			argv++;
			if (argc < 2)
				usage();
			serial = argv[1];
			break;

		default:
			usage();
		}
		argc--;
		argv++;
	}
	if (argc > 1)
		doprofile(argv[1]);
	sinit();
	tinit();
	pstart();
	/* NOTREACHED */
}

/*
 * Setup parent and child processes.
 */
pstart()
{
	if (pipe(pip) < 0) {
		fprintf(stderr, "Cannot pipe to child\n\r");
		texit(1);
	}
	if ((slave = fork()) < 0) {
		fprintf(stderr, "cannot create reader process\n\r\r");
		texit(1);
	}
	if (slave) {
		close(pip[0]);
		pip[0] = -1;
		readtty();
		texit(0);
	} else {
		close(pip[1]);
		pip[1] = -1;
		fclose(infp);
		readserial();
	}
}

/*
 * Protect a signal that is not already ignored
 * to the value specified.
 */
protect(sig, val)
register int sig;
int (*val)();
{
	if (signal(sig, SIG_IGN) != SIG_IGN)
		signal(sig, val);
}

/*
 * Set up our terminal for input.
 * If a terminal, it runs in raw mode.
 */
tinit()
{
	if (ioctl(0, TIOCGETP, &old) >= 0) {
		intty++;
		new = old;
		if (!hflag)
			new.sg_flags &= ~ECHO;
		new.sg_flags &= ~CRMOD;
		new.sg_flags |= CBREAK;	/* Eventually RAW */
		ioctl(0, TIOCSETN, &new);
		setbuf(stdin, NULL);
	}
	if (isatty(fileno(stdout)))
		setbuf(stdout, NULL);
}

texit(s)
int s;
{
	if (slave == 0)
		exit(s);
	if (slave > 0)
		if (kill(slave, SIGTERM) < 0)
			perror("kill");
	sexit();
	if (intty)
		ioctl(0, TIOCSETN, &old);
	wait((int *)0);
	exit(s);
}

/*
 * Setup full/half duplex on the input terminal.
 */
setduplex()
{
	if (!intty)
		return;
	if (hflag)
		new.sg_flags |= ECHO; else
		new.sg_flags &= ~ECHO;
	ioctl(0, TIOCSETN, &new);
}

/*
 * Setup the speed of the serial port.
 */
setspeed(n)
{
	register struct bauds *bp;
	static struct sgttyb sgb;

	for (bp = &bauds[0]; bp < &bauds[NBAUD]; bp++)
		if (bp->rate == n)
			break;
	if (bp == &bauds[NBAUD])
		fprintf(stderr, "%d is an unknown baud rate\n\r", n);
	ioctl(sfd, TIOCGETP, &sgb);
	sgb.sg_ispeed = sgb.sg_ospeed = bp->val;
	sgb.sg_flags &= ~ECHO;
	sgb.sg_flags |= RAW;
	ioctl(sfd, TIOCSETP, &sgb);
}

hangup()
{
	fprintf(stderr, "Hangup\r\n");
	texit(1);
}

/*
 * Initialise the serial port.
 */
sinit()
{
	if ((sfd = open(serial, 2)) < 0) {
		fprintf(stderr, "Unable to open terminal %s\n", serial);
		texit(1);
	}
	fprintf(stderr, "Data terminal ready\n\r");
	setspeed(baud);
}

/*
 * Finish up use of serial port.
 * Conditionally turn off
 * DTR and RTS.
 */
sexit()
{
	close(sfd);
}

/*
 * Send a break to the serial port.
 * [NOTE: this is implemented (wrongly)
 * in a hardware-specific manner for the
 * IBM PC.  A correct solution is to add
 * an ioctl call to the terminal driver.]
 */
sbreak()
{
	register int i;
	register int l;

	signal(SIGINT, SIG_IGN);
#ifdef I8086
	l = inb(0x3FB);		/* LCR */
	outb(0x3FB, l|0x40);	/* Send break */
	for (i=0; i<200; i++)	/* ~100 ms */
		getpid();
	outb(0x3FB, l);
#else
	write(2, "interrupts not supported\n\r", 26);
#endif
	signal(SIGINT, sbreak);
}

/*
 * Read the profile file (if any)
 * and do commands up to any wait
 * command.  If a `wait' is found,
 * return to allow port to be opened.
 * After wait command, the rest of the
 * profile file is pushed onto the input stack.
 */
doprofile(f)
char *f;
{
	register int c;
	static char lbuf[60];
	register FILE *fp;

	if ((fp = fopen(f, "r")) == NULL) {
		fprintf(stderr, "cu: cannot open profile file `%s'\n", f);
		texit(1);
	}
	push(fp);
	while ((c = pgetc())!=EOF && c == '~') {
		if (c=='\r' || c=='\n')
			continue;
		pgets(buf, sizeof buf, 0);
		switch (buf[0]) {
		case '\n':
		case '\r':
			break;

		case '#':
			break;

		case 'f':			/* Full duplex */
			hflag = 0;
			break;

		case 'h':			/* Half duplex */
			hflag = 1;
			break;

		case 'l':		/* Serial line */
			strcpy(lbuf, buf+1);
			serial = lbuf;
			break;

		case 's':		/* Speed */
			baud = atoi(&buf[1]);
			break;

		case 'w':		/* Wait for carrier */
			return;

		default:
			fprintf(stderr, "Profile file error at `~%c'\n\r", c);
			texit(1);
		}
	}
	if (c != EOF) {
		fprintf(stderr, "Bad input in profile file\n\r");
		texit(1);
	}
	pop(1);
}

/*
 * Flush input in profile file to end of line.
 */
pflush()
{
	register int c;

	while ((c = getc(infp))!='\r' && c!='\n') {
		if (c == '\\')
			c = getc(infp);
		if (c == EOF) {
			ungetc(EOF, infp);
			break;
		}
	}
}

/*
 * Get a string up until the newline but not
 * including it.  Newlines can be put into a
 * string by escaping them with `\'.
 * Flag means do local editing.
 */
pgets(as, l, f)
char *as;
unsigned l;
int f;
{
	register int c;
	register char *s;

	if (infp!=stdin || !intty)
		f = 0;
	s = as;
	while (--l>0 && (c = pgetc())!='\r' && c!='\n') {
		if (c == '\\')
			c = pgetc();
		if (c == EOF)
			break;
		else if (f != 0) {
			if (c == old.sg_kill) {
				l += s-as;
				s = as;
				continue;
			} else if (c==old.sg_erase && s>as) {
				s--;
				l++;
				continue;
			}
		}
		*s++ = c;
	}
	*s = 0;
	return (c==EOF && s==as ? NULL : as);
}

/*
 * Read the serial port
 * and write to the standard
 * output terminal.
 * This handles all output diversions.
 */
readserial()
{
	char c;

	signal(SIGINT, SIG_IGN);
	signal(SIGTRAP, auxspec);
	signal(SIGTERM, exit);
	for (;;) {
		c = sget() & ~0200;
		switch (c) {
		case 0177:		/* DEL */
		case 0:			/* NUL */
			continue;

		default:
			putc(c, outfp);
			if (out2fp != NULL)
				putc(c, out2fp);
		}
	}
}

/*
 * Read in special commands.
 * This includes calls to the system
 * and redirection of input or output.
 * Also ways to exit this system.
 */
special()
{
	register int n;
	register int c;
	register FILE *fp;

	switch (c = pgetc()) {
	case '~':
		return ('~');

	case '#':
		break;

	case '!':
		pgets(buf, sizeof buf, 1);
		if (intty)
			ioctl(0, TIOCSETN, &old);
		if (buf[0] == '\0')
			system("sh"); else
			system(buf);
		putc('!', stderr);
		putc('\n', stderr);
		putc('\r', stderr);
		if (intty)
			ioctl(0, TIOCSETN, &new);
		break;

	case '$':		/* Command > remote */
		pgets(buf, sizeof buf, 1);
		system(buf);
		putc('!', stderr);
		putc('\n', stderr);
		putc('\r', stderr);
		break;

	case '>':
		buf[0] = '>';
		pgets(buf+1, sizeof buf - 1, 1);
		sendchild(buf);
		break;

	case '<':
		pgets(buf, sizeof buf, 1);
		if ((fp = fopen(buf, "r")) != NULL)
			push(fp);
		else
			fprintf(stderr, "Cannot open %s\n\r", buf);
		break;

	case '?':
		pflush();
		fprintf(stderr, speclist);
		break;

	case 'f':		/* Full duplex */
		pflush();
		hflag = 0;
		setduplex();
		break;

	case 'g':		/* Get file */
		pgets(buf, sizeof buf, 1);
		gpinit(get, buf, 'r');
		break;

	case 'p':		/* Put files to remote system */
		pgets(buf, sizeof buf, 1);
		gpinit(put, buf, 's');
		break;

	case 'h':		/* Half duplex */
		pflush();
		hflag = 1;
		setduplex();
		break;

	case 'H':		/* Halt child */
	case 'C':		/* Continue child */
		buf[0] = c;
		buf[1] = '\0';
		sendchild(buf);
		break;

	case 'm':
		match(buf);
		break;

	case 'P':
		pgets(buf, sizeof buf, 1);
		sleep(atoi(buf));
		break;

	case 'q':
		ioctl(sfd, TIOCHPCL, NULL);
	case 'Q':
		pflush();
		texit(0);
		/* NOTREACHED */

	case 's':		/* Line speed */
		pgets(buf, sizeof buf, 1);
		if ((n = atoi(buf)) != 0)
			setspeed(n);
		break;

	default:
		pflush();
		fprintf(stderr, "`%c' is a bad special command\n\r", c);
		fprintf(stderr, "Type  ~?  for a list\n\r");
	}
	return (EOF);
}

/*
 * Auxiliary special commands that cannot be
 * handled by the parent.  This routine gets
 * invoked upon SIGTRAP signals from the master
 * process.
 */
auxspec()
{
	register char *s;
	register char *m;
	int l;

loop:
	signal(SIGTRAP, SIG_IGN);
	if (read(pip[0], &l, sizeof l) != sizeof l
	 || l < 0
	 || l > sizeof buf
	 || read(pip[0], buf, l) != l) {
		fprintf(stderr, "Pipe error\n\r");
		return;
	}
	s = buf;
	s[l] = '\0';
	switch (*s++) {
	case '>':
		if (*s == '\0') {
			outfp = stdout;
			out2fp = NULL;
			break;
		}
		m = "w";
		if (*s == '>') {
			m = "a";
			s++;
		}
		l = 0;
		if (*s == ':') {
			s++;
			l++;
		}
		if ((out2fp = fopen(s, m)) == NULL) {
			fprintf(stderr, "Redirection failed to `%s'\n\r", s);
			outfp = stdout;
			out2fp = NULL;
		} else if (l != 0) {
			outfp = out2fp;
			out2fp = NULL;
		}
		break;

	case 'H':		/* Halt until next command */
		goto loop;
		break;

	case 'C':		/* Continue */
		break;

	default:
		fprintf(stderr, "Slave: bad auxiliary command `%s'", buf);
	}
	signal(SIGTRAP, auxspec);
}

/*
 * Get a file.
 * The filename is passed to the remote
 * server process which determines the local
 * name for us.
 */
get(rf)
char *rf;
{
	register PKT *rpp;
	register unsigned seq;

	seq = 0;
	for (;;) {
		rpp = rcvpkt(seq);
		switch (rpp->p_type) {
		case 'F':
			if (dldfp != NULL)
				fclose(dldfp);
			if ((dldfp = fopen(rpp->p_data, "w")) == NULL) {
				fprintf(stderr, "cu: cannot create `%s'\n\r",
				    rpp->p_data);
				endpkt("bad open", 1);
				doend();
			}
			fprintf(stderr, "r %s\n\r", rpkt.p_data);
			seq = 0;
			break;

		case 'I':
			seq++;
			fwrite(rpp->p_data, 1, rpp->p_len - 3, dldfp);
			break;

		default:
			nakpkt();
			continue;
		}
		ackpkt();
	}
	/* NOTREACHED */
}

/*
 * Put files to the remote system.
 * Invoke the remote server process with a
 * command line.  It will ask for the files
 * one at a time and decide where to put them.
 */
put(rf)
char *rf;
{
	register PKT *rpp;
	register unsigned seq;
	register int n;

	seq = 0;
	for (;;) {
		rpp = rcvpkt(seq);
		if (rpp->p_type == 'F') {
			if (dldfp != NULL)
				fclose(dldfp);
			if ((dldfp = fopen(rpp->p_data, "r")) == NULL) {
				fprintf(stderr, "cu: cannot open `%s'\n\r",
				    rpp->p_data);
				endpkt("bad open", 1);
				doend();
			}
			fprintf(stderr, "p %s\n\r", rpkt.p_data);
			seq = 0;
			break;
		} else
			nakpkt();
	}
	while ((n = fread(xpkt.p_data, 1, NPKT, dldfp)) > 0) {
		xpkt.p_type = 'I';
		xpkt.p_seq[0] = seq&0xFF;
		xpkt.p_seq[1] = seq>>8;
		xpkt.p_len = n+3;
		sendpkt(&xpkt);
		seq++;
	}
	endpkt("END", 1);
	doend();
}

/*
 * Initialisation code for
 * get/put operations.
 * Return 1 when done.
 */
gpinit(func, fn, c)
int (*func)();
register char *fn;
int c;
{
	register char *cp;
	extern int nretry;

	while (*fn==' ' || *fn=='\t')
		fn++;
	if (*fn == '\0')
		return;		/* No work ? */
	nretry = 0;
	pktinit();
	sendchild("H");
	for (cp = "/etc/cuxcvr "; *cp != '\0'; )
		sput(*cp++);
	sput(c);
	sput(' ');
	while (*fn != '\0')
		sput(*fn++);
	sput('\r');
	if (setjmp(env) != 0) {
		/*
		 * Returns here at end of file.
		 */
		if (dldfp != NULL) {
			fclose(dldfp);
			dldfp = NULL;
		}
		protect(SIGINT, sbreak);
		if (rpkt.p_type == 'E') {
			fprintf(stderr, "%s\n\r", rpkt.p_data);
			if (nretry != 0)
				fprintf(stderr, "Retry: %d\n\r", nretry);
			if (c == 'r')
				endpkt("", 0);
		} else
			endpkt("", 0);
		sendchild("C");
		return;
	}
	protect(SIGINT, intend);
	(*func)(fn);
}

/*
 * On interrupt, send an end packet.
 */
intend()
{
	protect(SIGINT, intend);
	endpkt("Interrupt", 1);
}

/*
 * End of file sequence during downloading.
 */
doend(pp)
register PKT *pp;
{
	longjmp(env, 1);
}

/*
 * Read input until synchronised with an input
 * pattern following the `pnmatch' semantics.
 */
match(s)
register char *s;
{
	fprintf(stderr, "Pattern matching not supported\n\r");
}

readtty()
{
	register int c;
	static char nlf = 1;

	protect(SIGINT, sbreak);
	for (;;) {
		c = pgetc();
	swit:
		switch (c) {
		case EOF:
			pop(0);
			continue;

		case '\n':
		case '\r':
			nlf++;
			break;

		case '~':
			if (nlf!=0 && (c = special())==EOF)
				continue;
			else
				break;
			nlf = 0;
			goto swit;

		default:
			nlf = 0;
		}
		sput(c);
	}
}

/*
 * Push the file pointer stack.
 */
push(fp)
FILE *fp;
{
	if (infpp < &infps[NINSTACK]) {
		infp = fp;
		*++infpp = fp;
	}
}

/*
 * Pop the current input file from the
 * stack because of end of file.
 * `sf' is silent flag (set if not message printed).
 */
pop(sf)
{
	if (infpp != &infps[0]) {
		fclose(*infpp--);
		if (sf == 0) {
			putc('~', stderr);
			putc('\n', stderr);
			putc('\r', stderr);
		}
		infp = *infpp;
	}
}

/*
 * Send a command that cannot be processed by
 * the parent to the child process.
 */
sendchild(s)
register char *s;
{
	int l;

	kill(slave, SIGTRAP);
	l = strlen(s);
	write(pip[1], (char *)&l, sizeof l);
	write(pip[1], s, l);
}

/*
 * Get a character from the serial port
 */
sget()
{
	unsigned char c;

	while (read(sfd, &c, sizeof c) != sizeof c)
		if (errno != EINTR)
			hangup();
	return (c);
}

/*
 * Put a character to the serial port.
 */
sput(c)
{
	char lc = c;

	if (write(sfd, &lc, sizeof lc) != sizeof lc)
		hangup();
}

usage()
{
	fprintf(stderr, "Usage: term [-h] [-l port] [-s speed] [profile]\n");
	exit(1);
}
