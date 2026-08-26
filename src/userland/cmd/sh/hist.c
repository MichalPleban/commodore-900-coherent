/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * sh/hist.c
 * Interactive line input with a command history.
 *
 * When the shell is reading commands from a terminal, getn() takes its
 * characters from here instead of from stdio.  A whole line is collected
 * with the terminal in RAW mode and echoed by hand, so that the cursor
 * keys can be seen; every other line of input in the shell (a script, a
 * pipe, the `read' builtin, `sh -c') is untouched and still goes through
 * getc().
 *
 * Recall is the simple-minded kind: the line on screen is rubbed out with
 * backspaces and the remembered one is typed in its place.  There is no
 * cursor motion inside the line -- erase and line-kill are the only edits,
 * exactly as in cooked mode.
 *
 * WHY RAW AND NOT CBREAK.  The three consoles do not agree on what a cursor
 * key is:
 *
 *	zterm (the GUI terminal)		Up = ^P	   Down = ^N
 *	  -- zview's input pump maps the nav block to the MicroEMACS codes
 *	hi-res console (drv/hrtty/kb.c)		Up = 0x80  Down = 0x83
 *	  -- CUP/CDOWN of kbchar.h, delivered as single 8-bit bytes
 *	serial terminal				Up = ESC[A Down = ESC[B
 *	  -- or ESC O A / ESC O B in keypad-application mode
 *
 * Cooked and CBREAK input both do `c &= 0177' (drv/tty.c ttin), which turns
 * the hi-res CUP into a NUL and -- worse -- CDOWN into 0x03, the interrupt
 * character.  Only RAWIN leaves the 8th bit alone, so RAWIN it is; the
 * price is that the driver stops recognising the interrupt, quit and flow
 * control characters while a line is being typed, and this file has to do
 * their work.  Raw mode is on ONLY while typing: it is restored before the
 * line is handed to the parser, so commands always run on a cooked
 * terminal.
 *
 * Bytes >= 0x80 that are not one of the hi-res special keys are masked to
 * seven bits, which is what cooked mode would have done to them.
 */

#include "sh.h"
#include <sgtty.h>
#include <signal.h>

#define HISTMAX	64			/* Commands remembered */
#define LINEMAX	256			/* Longest line that can be typed */

/*
 * Special keys from the hi-res console keyboard (drv/hrtty/kbchar.h).
 */
#define KCUP	0x80			/* Cursor up */
#define KCLEFT	0x81			/* Cursor left */
#define KCRIGHT	0x82			/* Cursor right */
#define KCDOWN	0x83			/* Cursor down */
#define KCE	0x84			/* Clear entry */
#define KCSLOCK	0x8A			/* Scroll lock */
#define KCBRK	0x8B			/* Break */

/*
 * MicroEMACS codes, which is what the GUI keyboard pump sends.
 */
#define CPREV	0x10			/* ^P - previous */
#define CNEXT	0x0E			/* ^N - next */
#define ESC	0x1B

/*
 * The remembered lines.  A ring of HISTMAX malloc'd strings, newest at
 * hist[hnext-1].  They are malloc'd rather than kept in an array of fixed
 * cells because the shell is linked with shared text: every terminal
 * window costs a private copy of this file's data, and an unused history
 * should cost nothing.
 */
static char	*hist[HISTMAX];
static int	hnext;			/* Where the next line goes */
static int	hcount;			/* How many are remembered */
static int	histsize = -1;		/* Lines to remember, -1 = not set up */

/*
 * The line being typed, and how much of it getn() has taken.  The
 * terminating newline is part of it.
 */
static char	line[LINEMAX+2];
static int	linelen;
static int	linepos;

/*
 * Saved terminal state, valid while rawf is set.
 */
static struct sgttyb	savesg;
static struct tchars	savetc;
static int		rawf;

static	int	hisread();
static	void	hisadd();
static	void	hisecho();
static	void	hisrub();

/*
 * Can this file take over the given file descriptor?  Only a terminal
 * whose modes can be read and set, and only if the history has not been
 * switched off with HISTSIZE=0.  The answer is worked out once per
 * descriptor; the shell only ever asks about its input.
 *
 * HISTSIZE is read ONCE, on the first line read from a terminal, and
 * the ring is sized by it for the life of the shell.  A login shell has
 * read its profiles by then, so setting it there works; to switch the
 * history off for a shell that is already running, start another one
 * with `HISTSIZE=0 sh'.
 */
hisok(fd)
register int fd;
{
	static int	lastfd = -1;
	static int	lastok = 0;
	struct sgttyb	sg;
	register VAR	*vp;
	register char	*cp;

	if (histsize < 0) {
		histsize = HISTMAX;
		if ((vp=findvar("HISTSIZE")) != (VAR *)0
		 && (cp=convvar(vp)) != (char *)0 && *cp != '\0') {
			histsize = atoi(cp);
			if (histsize < 0)
				histsize = 0;
			else if (histsize > HISTMAX)
				histsize = HISTMAX;
		}
	}
	if (histsize == 0)
		return (0);
	if (fd == lastfd)
		return (lastok);
	lastfd = fd;
	lastok = gtty(fd, &sg) >= 0;
	return (lastok);
}

/*
 * Hand getn() the next character of the current line, collecting a new
 * one when the last is used up.  Returns EOF for an end of file.
 */
hisgetc(fd)
register int fd;
{
	if (linepos >= linelen) {
		if (hisread(fd) < 0)
			return (EOF);
	}
	return (line[linepos++] & 0377);
}

/*
 * Put the terminal into raw mode with the echo off, remembering how it
 * was.  The interrupt, quit and flow control characters are read too:
 * the driver stops acting on them in raw mode, so we must.
 */
static
rawon(fd)
register int fd;
{
	struct sgttyb sg;

	if (rawf)
		return (0);
	if (gtty(fd, &savesg) < 0)
		return (-1);
	if (ioctl(fd, TIOCGETC, &savetc) < 0)
		return (-1);
	sg = savesg;
	sg.sg_flags |= RAWIN;
	sg.sg_flags &= ~ECHO;
	if (stty(fd, &sg) < 0)
		return (-1);
	rawf = 1;
	return (0);
}

/*
 * Put the terminal back the way it was.  Every path out of hisread()
 * goes through here: a command must never run on a raw terminal.
 */
static
rawoff(fd)
register int fd;
{
	if (rawf) {
		rawf = 0;
		stty(fd, &savesg);
	}
}

/*
 * Collect one line.  Returns 0 for a line (which may be empty), -1 for
 * an end of file or a terminal that would not go raw.
 */
static
hisread(fd)
register int fd;
{
	register int	c;
	register int	len;
	register int	cur;		/* 0 = the line being typed */
	char		buf[1];
	int		n;

	linepos = linelen = 0;
	if (rawon(fd) < 0) {
		/*
		 * The terminal would not go raw.  Fall back to a plain
		 * cooked line -- no history and no cursor keys, but never
		 * an end of file the user did not type, which is what
		 * returning -1 here would mean to the shell.
		 */
		len = 0;
		while (len < LINEMAX && read(fd, buf, 1) == 1) {
			line[len++] = buf[0];
			if (buf[0] == '\n')
				break;
		}
		if (len == 0)
			return (-1);
		linelen = len;
		return (0);
	}
	fflush(stderr);			/* The prompt, before we echo */
	len = 0;
	cur = 0;
	for (;;) {
		if ((n=read(fd, buf, 1)) != 1) {
			/*
			 * A signal cut the read short (the handler in
			 * trap.c only sets a flag; recovery happens at the
			 * next safe point) or the terminal went away.  Drop
			 * what was typed and let the shell sort it out.
			 */
			rawoff(fd);
			if (n == 0)
				return (-1);
			linelen = 0;
			line[linelen++] = '\n';
			return (0);
		}
		c = buf[0] & 0377;

		/*
		 * The keys that move through the history.  ESC introduces
		 * the sequences a serial terminal sends; anything else
		 * after ESC is dropped, since there is nothing in a shell
		 * line for it to mean.
		 */
		if (c == ESC) {
			if (read(fd, buf, 1) != 1)
				continue;
			c = buf[0] & 0377;
			if (c != '[' && c != 'O')
				continue;
			if (read(fd, buf, 1) != 1)
				continue;
			switch (buf[0]) {
			case 'A':
				c = CPREV;
				break;
			case 'B':
				c = CNEXT;
				break;
			default:
				continue;
			}
		}
		switch (c) {
		case KCUP:
			c = CPREV;
			break;
		case KCDOWN:
			c = CNEXT;
			break;
		case KCE:			/* Clear entry: kill line */
			c = savesg.sg_kill;
			break;
		case KCLEFT:			/* Nothing to move to */
		case KCRIGHT:
		case KCSLOCK:
		case KCBRK:
			continue;
		}
		if (c > 0177)
			c &= 0177;		/* As cooked mode would */

		if (c == CPREV || c == CNEXT) {
			register int want;

			want = cur + (c==CPREV ? 1 : -1);
			if (want < 0 || want > hcount)
				continue;	/* Off either end: stay put */
			hisrub(len);
			cur = want;
			if (cur == 0)
				len = 0;
			else {
				strcpy(line, hist[(hnext-cur+histsize)%histsize]);
				len = strlen(line);
				hisecho(line, len);
			}
			continue;
		}
		if (c=='\r' || c=='\n') {
			hisecho("\n", 1);
			break;
		}
		if (c == savesg.sg_erase || c=='\b' || c==0177) {
			if (len > 0) {
				len -= 1;
				hisrub(1);
			}
			continue;
		}
		if (c == savesg.sg_kill) {
			hisrub(len);
			len = 0;
			continue;
		}
		if (c == savetc.t_eofc) {
			/*
			 * Only an end of file on an empty line.  Cooked
			 * mode would hand over a part-typed line here;
			 * this does not, and the line stands.
			 */
			if (len == 0) {
				rawoff(fd);
				return (-1);
			}
			continue;
		}
		if (c == savetc.t_intrc) {
			/*
			 * The driver is not watching for this in raw mode,
			 * so raise it here.  On THIS process only, the way
			 * recover() does it: ttsignal() would have hit the
			 * whole process group, but the group a single-user
			 * shell is in also holds init and the shared-library
			 * holders, and signalling those wedges the console.
			 * Cooked modes are back on first: the shell may
			 * longjmp out of the recovery this starts.
			 */
			hisecho("\n", 1);
			rawoff(fd);
			kill(getpid(), SIGINT);
			linelen = 0;
			line[linelen++] = '\n';
			return (0);
		}
		if (c == savetc.t_quitc) {
			hisecho("\n", 1);
			rawoff(fd);
			kill(getpid(), SIGQUIT);
			linelen = 0;
			line[linelen++] = '\n';
			return (0);
		}
		if (c==savetc.t_startc || c==savetc.t_stopc)
			continue;		/* Flow control, eaten */
		if (c == '\t') {
			/*
			 * A tab goes into the line but is echoed as one
			 * space, so that a rub-out stays in step with the
			 * screen.  (Here documents want real tabs.)
			 */
			if (len < LINEMAX) {
				line[len++] = '\t';
				hisecho(" ", 1);
			}
			continue;
		}
		if (c < ' ')
			continue;		/* Other control keys: ignored */
		if (len < LINEMAX) {
			line[len] = c;
			hisecho(&line[len], 1);
			len += 1;
		}
	}
	rawoff(fd);
	line[len] = '\0';
	hisadd(line, len);
	line[len++] = '\n';
	linelen = len;
	linepos = 0;
	return (0);
}

/*
 * Echo, to the same place the prompt went.
 */
static void
hisecho(cp, n)
char *cp;
int n;
{
	if (n > 0)
		write(2, cp, n);
}

/*
 * Rub out n characters of what has been echoed.
 */
static void
hisrub(n)
register int n;
{
	while (n-- > 0)
		write(2, "\b \b", 3);
}

/*
 * Remember a line.  Empty lines and a line the same as the last one are
 * not worth a slot.  A history that cannot be allocated is simply not
 * kept: it must never cost the shell a command.
 */
static void
hisadd(cp, len)
register char *cp;
register int len;
{
	register char *np;
	register int last;
	extern char *malloc();

	if (len == 0)
		return;
	if (hcount > 0) {
		last = (hnext-1+histsize) % histsize;
		if (strcmp(hist[last], cp) == 0)
			return;
	}
	if ((np=malloc(len+1)) == NULL)
		return;
	strcpy(np, cp);
	if (hist[hnext] != NULL)
		free(hist[hnext]);
	hist[hnext] = np;
	hnext = (hnext+1) % histsize;
	if (hcount < histsize)
		hcount += 1;
}

/* end of sh/hist.c */
