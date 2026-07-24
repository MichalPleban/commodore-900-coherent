/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * execution of the DFA
 */
#include	"egrep.h"
#include	"dragon.h"


static bool		status;		/* set TRUE if any matches */
static char		cbuf[BUFSIZ],	/* first BUFSIZ chars of input line */
			*file;		/* input file name */
static long		nlines,		/* input line count */
			seekpos,	/* start of current line in file */
			matches;	/* # matches */
static struct dragon	*dfa;		/* DFA start state */
static struct newt	*nfa;		/* NFA */
extern struct dragon	*makedfa( );


/*
 * search a file
 *	Return TRUE if any matches.
 */
bool
search( fn, dp, np)
char		*fn;
struct dragon	*dp;
struct newt	*np;
{
	bool	match( );

	file = fn;
	dfa = dp;
	nfa = np;
	nlines = 0;
	seekpos = 0;
	matches = 0;
	status = FALSE;

	while (match( ))
		;

	if (not sflag && not lflag && cflag) {
		printfile( );
		printf( "%D\n", matches);
	}
	return (status);
}


/*
 * look for a match
 */
bool
match( )
{
	register struct dragon	*dp;
	register		c;
	register char		*p;
	long			nchars;
	struct dragon		*dp2;
	extern char		etab[];
	bool			success( );

	if ((dp=dfa->d_p[etab['\n']]) == NULL)
		dp = makedfa( dfa, etab['\n'], nfa);
	nchars = 0;
	p = cbuf;

	while ((c=getchar( )) != EOF) {
		c &= 0177;
		if (p < &cbuf[BUFSIZ])
			*p++ = c;
		++nchars;
		dp2 = dp;
		if ((dp=dp->d_p[etab[c]]) == NULL)
			dp = makedfa( dp2, etab[c], nfa);
		if (c == '\n') {
			++nlines;
			if (vflag!=dp->d_success && not success( file, p))
				break;
			seekpos += nchars;
			if (dp->d_success)
				return (TRUE);
			nchars = 0;
			p = cbuf;
		}
	}

	return (FALSE);
}


/*
 * report a match
 *	If the line is to be printed, and it is over BUFSIZ chars, the
 * input file better be seekable.
 */
bool
success( file, p)
char	*file,
	*p;
{
	register char	*q;
	register	c,
			n;

	++matches;
	status = TRUE;
	if (sflag)
		return (FALSE);
	if (lflag) {
		printf( "%s\n", file);
		return (FALSE);
	}
	if (cflag)
		return (TRUE);
	printfile( );
	if (nflag)
		printf( "%D: ", nlines);
	if (bflag)
		printf( "%D: ", seekpos/BUFSIZ);
	q = cbuf;
	n = p - q;
	while (q < p)
		putchar( *q++);
	if (*--q != '\n') {
		if (fseek( stdin, seekpos+n, 0) == EOF) {
			putchar( '\n');
			fatal( "line too long");
		}
		do {
			c = getchar( );
			if (c == EOF)
				return (FALSE);
			putchar( c);
		} while (c != '\n');
	}
	return (TRUE);
}


static
printfile( )
{

	if (not hflag)
		printf( "%s: ", file);
}
