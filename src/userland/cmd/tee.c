/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * Rec'd from Lauren Weinstein, 7-16-84.
 * tee -- pipe redirection
 *	hacked by rec to use stdio so that everything doesn't come
 *	out buffered.
 */
#include	<signal.h>
#include	<errno.h>
#include	<stdio.h>


#define	NUFILE	20			/* max # open files */


int	aflag;				/* append to output files */
char	iobuf[BUFSIZ];
FILE	*openf();


main( argc, argv)
register char	**argv;
{
	register int	c;
	register FILE	**fpp;
	FILE	*fp[NUFILE];
	extern int exit();

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, exit);
	while (*++argv && argv[0][0]=='-')
		switch (argv[0][1]) {
		case 'i':
			signal( SIGINT, SIG_IGN);
			break;
		case 'a':
			++aflag;
			break;
		default:
			fprintf(stderr,
			   "Usage: tee [ -a ] [ -i ] [ file ] ...\n");
			exit(1);
			}

	for (fpp=fp; *argv; ) {
		if (fpp >= &fp[NUFILE])
			fatal( "too many files");
		*fpp++ = openf( *argv++);
	}
	*fpp = NULL;

	while ((c = getchar()) != EOF) {
		putchar(c);
		for (fpp=fp; *fpp!=NULL; fpp++)
			putc(c, *fpp);
	}

	return (0);
}


FILE *
openf( file)
char	*file;
{
	register FILE	*fp;
	extern		errno;

	if (aflag) {
		fp = fopen( file, "a");
		if (fp != NULL) {
			fseek(fp, 0L, 2);
			return (fp);
		}
	} else {
		fp = fopen( file, "w");
		if (fp != NULL)
			return (fp);
	}
	switch (errno) {
	case EMFILE:
	case ENFILE:
		fatal( "too many files");
		break;
	default:
		fatal( "can't create %s", file);
	}
}


fatal( arg0)
{	fprintf( stderr, "tee: %r\n", &arg0);
	exit( 1);
}
