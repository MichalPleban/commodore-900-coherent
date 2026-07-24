/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * kill -- send a signal to a process
 */
#include	<signal.h>
#include	<ctype.h>
#include	<errno.h>
#include	<stdio.h>

#define	NOTREACHED return

int err;

main( argc, argv)
register char	**argv;
{
	register	pid;
	char		**getsignal( );
	extern char	*sys_errlist[];
	extern		errno;
	static		sig	= SIGTERM;

	argv = getsignal(argv, &sig);
	if (*argv == NULL) {
     		fprintf(stderr, "Usage: kill [ signal ] pid ...\n");
		exit(1);	
	}
	signal(sig, SIG_IGN);

	do {
		if (isdigit(**argv)==0 && **argv!='-') {
			error("\"%s\" is not a process id", *argv);
			continue;
		}
		pid = atoi(*argv);
		if (kill(pid, sig)) {
			if (errno == EINVAL)
				fatal("signal %d is invalid", sig);
			error("process %d: %s", pid, sys_errlist[errno]);
			continue;
		}
	} while (*++argv);
	exit(err);
}


char **
getsignal( av, sigp)
register char **av;
int *sigp;
{
	if ((++av)[0] == NULL)
		return (av);
	if (isdigit( av[0][0]))
		return (av);
	if (av[0][0] == '-') {
		++av[0];
		if (isdigit( av[0][0])) {
			*sigp = atoi( &av[0][0]);
			return (++av);
		}
	}
	uppercase( av[0]);
	if (strncmp( av[0], "SIG", 3) == 0)
		*av += 3;
	*sigp = 1;
	while (notsame( *sigp, av[0]))
		++*sigp;
	return (++av);
}


notsame(sig, name)
char *name;
{
	static char	*names[] = {
		0,
		"HUP",  "INT",  "QUIT", "ALRM",
		"TERM", "REST", "SYS",  "PIPE",
		"KILL", "TRAP", "SEGV", "ILL",
		"IOT",  "EMT",  "FPE",  "BUS",
		NULL
	};

	if (sig>NSIG || names[sig]==NULL)
		fatal( "no such signal SIG%s", name);
	return (strcmp( names[sig], name));
}


uppercase(s)
register char *s;
{
	for (; *s; ++s)
		if (islower( *s))
			*s &= ~040;
}


error(arg0)
char *arg0;
{
	fprintf( stderr, "kill: %r\n", &arg0);
	err = 1;
}


fatal(arg0)
char *arg0;
{
	error(arg0);
	exit(1);
}
