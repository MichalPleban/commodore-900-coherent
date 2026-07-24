/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Gpph() gets a passphrase from the terminal after disabling echo, writing
 * 'prompt' and flushing input (ie, throwing input away). The line is placed
 * in 'buf'.
 * If gpph() cannot read from /dev/tty it exits with status 1.
 */
#include <stdio.h>
#include <signal.h>
#include <sgtty.h>
#include "knapsack.h"

static	FILE	*devtty;
static	struct	sgttyb	sg;
static	int	flagsave;		/* to save sg.sg_flags */

static	int	(*fint)();
static	int	(*fquit)();
static	int	(*fterm)();
static	int	(*fhup)();
static	int	sigcaught();
static	void	sigcatch();
static	void	sigreset();

void
gpph(prompt, buf)
char *prompt;
char buf[PPSIZ];
{
	register int n;

	sigcatch(SIGINT, fint);
	sigcatch(SIGQUIT, fquit);
	sigcatch(SIGTERM, fterm);
	sigcatch(SIGHUP, fhup);

	if ((devtty = fopen("/dev/tty", "wr")) == NULL
	or ioctl(fileno(devtty), TIOCGETP, &sg) < 0) {
		fputs("Cannot open /dev/tty to read passphrase.\n", stderr);
		exit(1);
	}

	flagsave = sg.sg_flags;		/* Save old status */
	sg.sg_flags &= ~ECHO;
	if (ioctl(fileno(devtty), TIOCSETP, &sg) < 0) {
		fputs("Cannot disable echo on /dev/tty.\n", stderr);
		exit(1);
	}

	fputs(prompt, devtty);
	fflush(devtty);
	if (fgets(buf, PPSIZ, devtty) == NULL) {
		sigreset();
		exit(1);
	}
	n = strlen(buf) - 1;
	if (buf[n] == '\n')
		buf[n] = '\0';

	fputs("\n", devtty);
	fflush(devtty);
	sigreset();
	fclose(devtty);
	return;
}

static void
sigreset()
{
	sg.sg_flags = flagsave;
	ioctl(fileno(devtty), TIOCSETP, &sg);
	signal(SIGINT, fint);
	signal(SIGQUIT, fquit);
	signal(SIGHUP, fhup);
	signal(SIGTERM, fterm);
	return;
}

static void
sigcatch(n, func)
int n;
int (*func)();
{
	if ((func = signal(n, SIG_IGN)) == SIG_DFL)
		signal(n, sigcaught);
	return;
}

static int
sigcaught(n)
int n;
{
	signal(n, SIG_IGN);
	switch (n) {
	case SIGINT:
		fputs("Interrupt\n", stderr);
		break;
	case SIGHUP:
		fputs("Sighup\n", stderr);
		break;
	case SIGTERM:
		fputs("Sigterm\n", stderr);
		break;
	case SIGQUIT:
		fputs("Quit -- no core dump.\n", stderr);
		break;
	}
	sigreset();
	exit(n);
}

