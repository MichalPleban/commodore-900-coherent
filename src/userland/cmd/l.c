/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * l (scat) -- print files on a video screen, a page at a time.
 *
 * Cleans up typewriter output on the way (backspace erases, CR kills
 * the pending line, overstruck '_' and ' ' merge with what is under
 * them), folds long lines at a word break with a trailing '\',
 * packs runs of output spaces into tabs, and pauses for a keystroke
 * (read from fd 2) after every screenful.
 *
 * Reconstructed from the disassembly of the original binary
 * (see disasm/l.asm, disasm/l_brief.md).
 */

#include <stdio.h>
#include <sgtty.h>
#include <signal.h>
#include <errno.h>

extern char	*getenv();
extern long	lseek();

/*
 * Option and geometry parameters; "dfl" holds the defaults, "parm"
 * the working copy for the current file.
 */
struct	parm	{
	int	p_flags;		/* option flag bits */
	long	p_skip;			/* -S: bytes to seek first */
	int	p_lines;		/* -l: lines per page */
	int	p_cols;			/* -w: columns per line */
	int	p_begin;		/* -b: first line to print */
	int	p_indent;		/* -i: input columns to drop */
};

#define	F_TRUNC		0x01		/* -t: truncate long lines */
#define	F_CTL		0x02		/* -c: show control characters */
#define	F_CTLSPACE	0x04		/* -cs: show spaces as '_' */
#define	F_CTLTAB	0x08		/* -ct: expand tabs even with -c */
#define	F_SQUEEZE	0x10		/* -s: squeeze indented-away lines */
#define	F_NOTAB		0x20		/* -x: no space to tab packing */
#define	F_NOPAUSE	0x40		/* -r: never pause */
#define	F_NUMBER	0x80		/* -n: number lines */

/*
 * A "-.suf options..." registration.
 */
#define	NINVOKE	5
struct	invoke	{
	char	*i_name;		/* suffix, '.' included */
	char	**i_argv;		/* arguments following "-.suf" */
};

struct	invoke	invokes[NINVOKE];
struct	invoke	*invp	= invokes;

/*
 * Word-character classes for fold(): bit set means the character is
 * part of a word (alphanumerics, '\' and '_') and may not be broken at.
 */
char	brkmap[16] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xc0,
	0x7f, 0xff, 0xff, 0xe9, 0x7f, 0xff, 0xff, 0xe0
};
char	bittab[8] = {
	0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01
};

char	nbuf[]	= "12345   ";		/* numstr() scratch */

#define	LSIZE	256			/* line buffer size */
#define	TSIZE	256			/* typewriter buffer size */

struct	parm	dfl;			/* default parameters */
char	lbuf[LSIZE];			/* assembled output line */
struct	sgttyb	curtty;			/* modes on entry */
struct	sgttyb	t401;			/* modes for clearing the 401 */
int	rawflag;			/* tty is in our modes */
int	lineno;				/* current input line number */
int	(*infn)();			/* line getter for this file */
int	eoff;				/* input() hit end of file */
int	tpos;				/* tbuf read position */
int	tcnt;				/* tbuf cursor offset */
int	pend;				/* putline() pending column */
int	nfiles;				/* file arguments seen */
int	ttot;				/* tbuf buffered characters */
struct	parm	parm;			/* parameters for current file */
int	col;				/* putline() output column */
struct	sgttyb	rawtty;			/* curtty, CBREAK on, ECHO off */
int	istty;				/* output is a terminal */
char	tbuf[TSIZE];			/* typewriter/pushback buffer */

char	**options();
char	*numstr();
long	getnum();
FILE	*fileopen();
int	getline0(), tgetline(), cgetline();
int	putline(), putplain();
int	quit();

main(argc, argv)
int argc;
char **argv;
{
	char obuf[BUFSIZ];
	char ibuf[BUFSIZ];
	char *envv[20];

	setbuf(stdin, ibuf);
	setbuf(stdout, obuf);
	initterm(&dfl);
	getenvargs(envv);
	options(envv, &dfl, 0);
	options(argv + 1, &dfl, 0);
	if (nfiles == 0)
		process((char *)NULL, &dfl);
	fflush(stdout);
	quit(0);
}

/*
 * Find the screen geometry and build the terminal mode copies;
 * arrange for interrupts to restore the terminal.
 */
initterm(tp)
register struct parm *tp;
{
	register char *p;

	tp->p_lines = 24;
	tp->p_cols = 80;
	if (ioctl(1, TIOCGETP, &curtty) == 0) {
		istty++;
		rawtty = curtty;
		rawtty.sg_flags |= CBREAK;
		rawtty.sg_flags &= ~ECHO;
		if ((p = getenv("TERM")) != NULL && strcmp(p, "4012") == 0) {
			t401 = curtty;
			t401.sg_flags |= 0;	/* masks are 0 here */
			t401.sg_flags &= ~0;
			tp->p_lines = 34;
			tp->p_cols = 72;
		}
	}
	if (signal(SIGINT, SIG_IGN) == SIG_DFL)
		signal(SIGINT, quit);
}

/*
 * Split $SCAT into an argument vector, NUL-terminating each
 * token in place.
 */
getenvargs(vec)
char **vec;
{
	register char **vp;
	register char *p;
	register int intok;
	register int c;

	vp = vec;
	*vp = NULL;
	if ((p = getenv("SCAT")) == NULL)
		return;
	intok = 0;
	while (*p) {
		c = *p++;
		if (c == ' ' || c == '\t' || c == '\n') {
			intok = 0;
			p[-1] = 0;
			continue;
		}
		if (intok)
			continue;
		if (vp >= &vec[19])
			error("too many env args", (char *)NULL);
		*vp++ = p - 1;
		intok++;
	}
	*vp = NULL;
}

/*
 * Scan option arguments into *tp, one option letter per argument.
 * Non-options stop the scan if stopfile is set, else are processed
 * as files on the spot.  Returns where the scan stopped.
 */
char **
options(argv, tp, stopfile)
register char **argv;
register struct parm *tp;
int stopfile;
{
	register int c;
	register char *p;
	struct parm junk;

	while (*argv != NULL) {
		if (**argv != '-') {
			if (stopfile)
				return (argv);
			process(*argv++, tp);
			nfiles++;
			continue;
		}
		c = (*argv)[1];
		switch (c) {

		case '-':			/* end of options */
			return (argv + 1);

		case '.':			/* per-suffix options */
			if (stopfile)
				return (argv);
			if (invp == &invokes[NINVOKE])
				error("too many invokes", (char *)NULL);
			invp->i_name = *argv + 1;
			invp->i_argv = argv + 1;
			invp++;
			argv = options(argv + 1, &junk, 1);
			continue;

		case 'S':
			tp->p_skip = getnum(*argv + 2);
			break;
		case 'b':
			tp->p_begin = getnum(*argv + 2);
			break;
		case 'c':
			p = *argv + 2;
			for (;;) {
				c = *p++;
				if (c == 0) {
					tp->p_flags |= F_CTL;
					break;
				} else if (c == 's')
					tp->p_flags |= F_CTLSPACE;
				else if (c == 't')
					tp->p_flags |= F_CTLTAB;
				else
					error("no such sub-options ", *argv);
			}
			break;
		case 'i':
			tp->p_indent = getnum(*argv + 2);
			break;
		case 'l':
			tp->p_lines = getnum(*argv + 2);
			break;
		case 'n':
			tp->p_flags |= F_NUMBER;
			break;
		case 'r':
			tp->p_flags |= F_NOPAUSE;
			break;
		case 's':
			tp->p_flags |= F_SQUEEZE;
			break;
		case 't':
			tp->p_flags |= F_TRUNC;
			break;
		case 'w':
			tp->p_cols = getnum(*argv + 2);
			break;
		case 'x':
			tp->p_flags |= F_NOTAB;
			break;
		default:
			error("no such switch ", *argv);
		}
		argv++;
	}
	return (argv);
}

/*
 * Complain on fd 2 and give up.
 */
error(msg, arg)
char *msg, *arg;
{
	fflush(stdout);
	errstr("scat: ");
	errstr(msg);
	errstr(arg);
	errstr("\n");
	quit(1);
}

/*
 * Restore the terminal and leave.  Also the interrupt catcher, so a
 * caught SIGINT exits with the signal number as status.
 */
quit(status)
int status;
{
	signal(SIGINT, SIG_IGN);
	if (rawflag)
		ioctl(1, TIOCSETP, &curtty);
	_exit(status);
}

/*
 * Print one file (stdin if name is NULL).
 */
process(name, tp)
register char *name;
struct parm *tp;
{
	register char *p;
	register struct invoke *ip;
	register int n;
	register int k;
	int (*outfn)();
	char c;

	parm = *tp;

	if (name != NULL) {
		if (fileopen(name) == NULL)
			error(errno == EACCES ?
			    "no permission on " : "can't find ", name);
	} else
		name = "[stdin].";

	/*
	 * Apply registered "-.suf" options matching the file's suffix.
	 */
	p = name;
	while (*p)
		p++;
	p++;
	do {
		if (*--p == '.') {
			for (ip = invokes; ip < invp; ip++)
				if (strcmp(p, ip->i_name) == 0)
					options(ip->i_argv, &parm, 1);
			break;
		}
	} while (p > name);

	if (parm.p_flags & F_NUMBER)
		parm.p_cols -= 8;
	parm.p_cols += parm.p_indent;
	if ((unsigned)(parm.p_cols - 8) >= 256)
		error("page width don't jive", (char *)NULL);

	infn = getline0;
	if (parm.p_flags & F_TRUNC)
		infn = tgetline;
	if (parm.p_flags & F_CTL)
		infn = cgetline;
	outfn = putline;
	if (parm.p_flags & F_NOTAB)
		outfn = putplain;

	if (parm.p_skip)
		lseek(0, parm.p_skip, 0);

	lineno = 1;
	while (lineno < parm.p_begin) {
		for (;;) {
			k = getc(stdin);
			if (k == '\n')
				break;
			if (k < 0)
				return;
		}
		lineno++;
	}

	n = parm.p_lines;
	for (;;) {
		if (putpage(n, outfn) == 0)
			return;
		if (istty == 0 || (parm.p_flags & F_NOPAUSE))
			goto contin;
		fflush(stdout);
		if (!rawflag) {
			rawflag++;
			ioctl(1, TIOCSETN, &rawtty);
		}
	key:
		if (read(2, &c, 1) <= 0)
			quit(0);
		switch (c) {
		case 004:			/* EOT */
		case 'q':
			errstr("\n");
			quit(0);
		case '\n':			/* next page */
			n = parm.p_lines;
			break;
		case ' ':			/* one more line */
			if (t401.sg_flags)
				goto beep;
			n = 1;
			break;
		case '/':			/* half a page */
			if (t401.sg_flags)
				goto beep;
			n = parm.p_lines / 2 + 1;
			break;
		case 'f':			/* where are we? */
			errstr("\n");
			errstr(name);
			errstr(numstr(lineno - 1));
			goto key;
		case 'n':			/* next file */
			errstr("\n");
			return;
		default:
		beep:
			errstr("\007");
			goto key;
		}
	contin:
		putc('\n', stdout);
	}
}

/*
 * Print up to n lines.  Newlines separate lines rather than end them,
 * so the last line on the screen carries no newline while we pause.
 * Returns the number of lines used up; 0 means end of file.
 */
putpage(n, outfn)
int n;
register int (*outfn)();
{
	register int cont;
	register int len;
	register char *p;
	int left;

	cont = 0;
	left = n;
	do {
		len = (*infn)(lbuf);
		if (len < 0)
			break;
		p = lbuf + parm.p_indent;
		if (p >= lbuf + len && (parm.p_flags & F_SQUEEZE)) {
			left++;
			if (lbuf[len] == '\n')
				lineno++;
			continue;
		}
		if (left != n)
			putc('\n', stdout);
		else if (t401.sg_flags)
			clearscr();
		pend = 0;
		col = 0;
		if (parm.p_flags & F_NUMBER) {
			if (cont)
				(*outfn)("        ", 8);
			else {
				(*outfn)(numstr(lineno), 5);
				(*outfn)(":  ", 3);
				cont++;
			}
		}
		if (lbuf[len] == '\n') {
			lineno++;
			cont = 0;
		}
		(*outfn)(p, (lbuf + len) - p);
	} while (--left);
	return (n - left);
}

/*
 * Default line getter: expand tabs, drop form feeds, fold lines
 * that get too long.  The terminating character is stored at
 * lbuf[len]; returns the length, or -1 at end of file.
 */
getline0(lbuf)
register char *lbuf;
{
	register int len;
	register int c;

	len = 0;
	for (;;) {
		c = input();
		if (c < 0 || c == '\n')
			break;
		if (c == '\t') {
			if ((len | 7) + 1 > parm.p_cols)
				goto toolong;
			do
				lbuf[len++] = ' ';
			while (len & 7);
			continue;
		}
		if (c == '\f')
			continue;
		if (len >= parm.p_cols) {
		toolong:
			len = fold(lbuf, len, c);
			c = 0;
			break;
		}
		lbuf[len++] = c;
	}
	lbuf[len] = c;
	if (len == 0 && c < 0)
		return (-1);
	return (len);
}

/*
 * -t line getter: truncate instead of folding.
 */
tgetline(lbuf)
register char *lbuf;
{
	register int len;
	register int c;

	len = 0;
	for (;;) {
		c = input();
		if (c < 0 || c == '\n')
			break;
		if (c == '\t') {
			do {
				if (len < parm.p_cols)
					lbuf[len] = ' ';
				len++;
			} while (len & 7);
		} else if (c == '\f')
			continue;
		else if (len < parm.p_cols)
			lbuf[len++] = c;
		else
			len += 2;
		if (len > parm.p_cols) {
			do {
				c = input();
			} while (c >= 0 && c != '\n');
			len = parm.p_cols;
			break;
		}
	}
	lbuf[len] = c;
	if (len == 0 && c < 0)
		return (-1);
	return (len);
}

/*
 * -c line getter: raw characters made visible.
 */
cgetline(lbuf)
register char *lbuf;
{
	register int len;
	register int c;

	len = 0;
	for (;;) {
		c = getc(stdin);
		if (c < 0 || c == '\n')
			break;
		if (c & 0xff80) {
			lbuf[len++] = '~';
			c &= 0x7f;
		}
		if (c == '\t' && (parm.p_flags & F_CTLTAB)) {
			do {
				if (len < parm.p_cols)
					lbuf[len] = ' ';
				len++;
			} while (len & 7);
		} else if (c < 040 || c == 0177) {
			lbuf[len++] = '^';
			lbuf[len++] = c + 0100;
		} else {
			switch (c) {
			case ' ':
				if (parm.p_flags & F_CTLSPACE)
					c = '_';
				break;
			case '_':
				if ((parm.p_flags & F_CTLSPACE) == 0)
					break;
				/* fall through */
			case '\\':
			case '^':
			case '~':
				lbuf[len++] = '\\';
				break;
			}
			lbuf[len++] = c;
		}
		if (len + 3 >= parm.p_cols) {
			lbuf[len++] = '\\';
			c = 0;
			break;
		}
	}
	lbuf[len] = c;
	if (len == 0 && c < 0)
		return (-1);
	return (len);
}

/*
 * Fold a long line: push the overflow back, back up to a word
 * break (at most 10 characters), append a '\'.
 */
fold(lbuf, len, c)
register char *lbuf;
register int len;
int c;
{
	register int i;

	pushback(c);
	if (len == parm.p_cols) {
		len--;
		pushback(lbuf[len]);
	}
	i = len;
	while (i > 10 && i > len - 10) {
		i--;
		if (brkmap[(lbuf[i] >> 3) & 0xf] & bittab[lbuf[i] & 7])
			continue;
		i++;
		while (len > i)
			pushback(lbuf[--len]);
		break;
	}
	lbuf[len++] = '\\';
	return (len);
}

/*
 * Default writer: pack runs of spaces into tabs.
 */
putline(buf, n)
register char *buf;
register int n;
{
	register int c;

	while (--n >= 0) {
		c = *buf++;
		if (c == ' ') {
			pend++;
			continue;
		}
		while ((col | 7) + 1 <= pend) {
			putc('\t', stdout);
			col = (col | 7) + 1;
		}
		while (col < pend) {
			putc(' ', stdout);
			col++;
		}
		putc(buf[-1], stdout);
		col++;
		pend = col;
	}
	return (0);
}

/*
 * -x writer: characters verbatim.
 */
putplain(buf, n)
register char *buf;
register int n;
{
	while (--n >= 0)
		putc(*buf++, stdout);
	return (0);
}

/*
 * Reset the typewriter buffer and reopen stdin on the named file.
 */
FILE *
fileopen(name)
char *name;
{
	register char *p;

	tcnt = 0;
	ttot = 0;
	eoff = 0;
	for (p = tbuf; p < tbuf + TSIZE; p++)
		*p = 0;
	return (freopen(name, "r", stdin));
}

/*
 * Push a character back into the typewriter buffer.
 */
pushback(c)
int c;
{
	tpos--;
	tbuf[tpos & 0xff] = c;
	tcnt++;
	ttot++;
	return (c);
}

/*
 * Typewriter-normalizing character source.  Reads ahead into a ring,
 * honoring backspace, CR (kill line), ESC (shown as '$') and merging
 * overstruck spaces and underlines; then drains one character.
 */
input()
{
	register int c;
	register int i;

	for (;;) {
		if (eoff)
			break;
		if (ttot >= 0xf6)
			break;
		c = getc(stdin);
		if (c < 0) {
			eoff++;
			break;
		}
		c &= 0x7f;
		switch (c) {
		case 010:			/* erase a character */
			if (tcnt == 0)
				continue;
			i = (tcnt + tpos - 1) & 0xff;
			if (tbuf[i] == '\n')
				continue;
			tcnt--;
			continue;
		case '\r':			/* kill the line */
			while (tcnt) {
				i = (tcnt + tpos - 1) & 0xff;
				if (tbuf[i] == '\n')
					break;
				tcnt--;
			}
			continue;
		case 033:
			c = '$';
			break;
		case '\n':
			tcnt = ttot;
			break;
		case ' ':			/* overstrike: keep char */
			i = (tpos + tcnt) & 0xff;
			if (tbuf[i] != 0)
				goto advance;
			break;
		case '_':			/* underline: keep char */
			i = (tpos + tcnt) & 0xff;
			if (tbuf[i] != 0 && tbuf[i] != ' ')
				goto advance;
			break;
		}
		tbuf[(tpos + tcnt) & 0xff] = c;
	advance:
		tcnt++;
		if (tcnt > ttot)
			ttot++;
	}
	if (eoff && ttot == 0)
		return (-1);
	tcnt--;
	ttot--;
	c = tbuf[tpos & 0xff];
	tbuf[tpos & 0xff] = 0;
	tpos++;
	return (c);
}

/*
 * Put a string on the terminal (fd 2).
 */
errstr(s)
register char *s;
{
	register char *p;

	if (s == NULL)
		return;
	p = s;
	while (*p)
		p++;
	write(2, s, p - s);
}

/*
 * Read a number: leading 0 for octal, 'b' suffix for blocks (512),
 * 'k' suffix for K (1024).
 */
long
getnum(s)
register char *s;
{
	register int base;
	register int shift;
	register int c;
	long v;

	base = 10;
	if (*s == '0')
		base = 8;
	shift = 0;
	v = 0;
	for (;;) {
		c = *s++;
		if (c == 'b') {
			shift = 9;
			break;
		}
		if (c == 'k') {
			shift = 10;
			break;
		}
		c -= '0';
		if ((unsigned)c > 9)
			break;
		v = v * base + c;
	}
	return (v << shift);
}

/*
 * Edit a number, right-aligned in a 5-character field.
 */
char *
numstr(n)
int n;
{
	register char *p;
	register int v;

	v = n;
	p = &nbuf[5];
	while (p > nbuf) {
		if (v) {
			*--p = v % 10 + '0';
			v /= 10;
		} else
			*--p = ' ';
	}
	return (p);
}

/*
 * Clear the screen of a 401 terminal.
 */
clearscr()
{
	register char *p;

	fflush(stdout);
	rawflag++;
	ioctl(1, TIOCSETP, &t401);
	for (p = "\033\014\r\r\r"; *p; p++)
		putc(*p, stdout);
	fflush(stdout);
	ioctl(1, TIOCSETP, &curtty);
	rawflag = 0;
}
