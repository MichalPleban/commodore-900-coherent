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
 * with the terminal in CBREAK mode and echoed by hand, so that the cursor
 * keys can be seen; every other line of input in the shell (a script, a
 * pipe, the `read' builtin, `sh -c') is untouched and still goes through
 * getc().
 *
 * Recall is the simple-minded kind: the line on screen is rubbed out with
 * backspaces and the remembered one is typed in its place.  There is no
 * cursor motion inside the line -- erase and line-kill are the only edits,
 * exactly as in cooked mode.
 *
 * WHY CBREAK AND NOT COOKED.  Cooked input hands over whole lines, so a
 * cursor key could never be seen mid-line; CBREAK delivers each character
 * as it is typed while the DRIVER keeps its own duties -- interrupt, quit
 * and flow control act at the keyboard exactly as at a cooked prompt.
 * Echo is off and done by hand.  Erase, line-kill and end-of-file are not
 * cooked-mode work in CBREAK, so this file does them.
 *
 * CBREAK is enough because every console now sends 7-bit codes for the
 * cursor keys: zterm's input pump and BOTH kernel console keyboards
 * (drv/hrtty and drv/lrtty kb.c, kbmap()) send the MicroEMACS control
 * codes -- Up = ^P, Down = ^N -- and a serial terminal sends ESC [ A /
 * ESC [ B (or ESC O A / ESC O B in keypad-application mode).  This file
 * once used RAWIN and did the driver's work itself, because old kernels
 * sent the console cursor keys as 8-bit codes (0x80..0x8B of kbchar.h)
 * that the 7-bit mask of cooked and CBREAK input mangles; those codes,
 * and the raw mode, are gone.
 */

#include "sh.h"
#include <sgtty.h>

#define HISTMAX	64			/* Commands remembered */
#define LINEMAX	256			/* Longest line that can be typed */

/*
 * MicroEMACS codes, which is what the GUI keyboard pump and the kernel
 * console keyboards send for the cursor keys.
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
 * Saved terminal state, valid while cbrkf is set.
 */
static struct sgttyb	savesg;
static struct tchars	savetc;
static int		cbrkf;

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
 * Put the terminal into CBREAK with the echo off, remembering how it
 * was.  The driver still acts on interrupt, quit and flow control; the
 * tchars are read for t_eofc, which CBREAK leaves to us.
 */
static
cbrkon(fd)
register int fd;
{
	struct sgttyb sg;

	if (cbrkf)
		return (0);
	if (gtty(fd, &savesg) < 0)
		return (-1);
	if (ioctl(fd, TIOCGETC, &savetc) < 0)
		return (-1);
	sg = savesg;
	sg.sg_flags |= CBREAK;
	sg.sg_flags &= ~ECHO;
	if (stty(fd, &sg) < 0)
		return (-1);
	cbrkf = 1;
	return (0);
}

/*
 * Put the terminal back the way it was.  Every path out of hisread()
 * goes through here: a command must never run on a CBREAK terminal.
 */
static
cbrkoff(fd)
register int fd;
{
	if (cbrkf) {
		cbrkf = 0;
		stty(fd, &savesg);
	}
}

/*
 * Collect one line.  Returns 0 for a line (which may be empty), -1 for
 * an end of file or a terminal that would not change modes.
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
	if (cbrkon(fd) < 0) {
		/*
		 * The terminal would not change modes.  Fall back to a plain
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
			 * A signal cut the read short -- in CBREAK the
			 * driver itself raises interrupt and quit, and the
			 * handler in trap.c only sets a flag -- or the
			 * terminal went away.  Drop what was typed and let
			 * the shell sort it out.
			 */
			cbrkoff(fd);
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
			 * this does not, and the line stands.  (Interrupt,
			 * quit and flow control never get this far: the
			 * driver acts on them even in CBREAK.)
			 */
			if (len == 0) {
				cbrkoff(fd);
				return (-1);
			}
			continue;
		}
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
	cbrkoff(fd);
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
