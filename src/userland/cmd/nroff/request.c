/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Request handler.
 */
#include <stdio.h>
#include <ctype.h>
#include "roff.h"
#include "reg.h"
#include "str.h"

/*
 * This is called when the user gives a request.  It collects the
 * arguments and dispatches the request.
 */
request()
{
	REG *rp;
	int inqflag, argc, i;
	register int c;
	char *argp[ARGSIZE], *abuf, *abufend, name[2], *mpend;
	register char *ap, *mp;

	if (isascii(c=getf(0)) && isspace(c)) {
		while (c != '\n')
			c = getf(0);
		return;
	}
	argc = 0;
	for (i=0; i<ARGSIZE; i++)
		argp[i] = null;
	ap = abuf = nalloc(ABFSIZE);
	abufend = &abuf[ABFSIZE];
	mp = miscbuf;
	mpend = &mp[MSCSIZE-1];
	while (c != '\n') {
		if (argc < ARGSIZE)
			argp[argc++] = ap;
		inqflag = 0;
		while ((c!='\n') && (inqflag || !isascii(c) || !isspace(c))) {
			if (c=='"' && escflag==0)
				inqflag ^= 1;
			else {
				if (ap >= abufend)
					break;
				*ap++ = c;
			}
			if (mp >= mpend)
				break;
			*mp++ = c;
			c = getf(0);
		}
		while (c!='\n' && isascii(c) && isspace(c)) {
			if (ap>=abufend || mp>=mpend) {
				printe("Arguments too long");
				nfree(abuf);
				while (c != '\n')
					c = getf(0);
				return;
			}
			*mp++ = c;
			c = getf(0);
		}
		*ap++ = '\0';
	}
	*mp++ = '\0';
	argname(argp[0], name);
	if (d00flag)
		fprintf(stderr, "%s\n", miscbuf);
	if ((rp=findreg(name, RTEXT)) == NULL)
		printe("Request `%s' not found", argp[0]);
	else {
		if (rp->r_macd.m_type == MREQS)
			(*rp->r_macd.m_func)(argc, argp);
		else {
			if (adstreg(rp) != 0) {
				strp->s_argc = argc;
				for (i=0; i<ARGSIZE; i++)
					strp->s_argp[i] = argp[i];
				strp->s_abuf = abuf;
			}
			return;
		}
	}
	nfree(abuf);
}

/*
 * Store the next argument from the given line `lp' in the given
 * argument buffer, `ap'.  `n' is the length of the buffer.
 * Trailing space characters are skipped over and the new line
 * pointer is returned.
 */
char	*
nextarg(lp, ap, n)
register char *lp, *ap;
{
	register char *apend;

	apend = ap==NULL ? ap : &ap[n-1];
	while (!isascii(*lp) || !isspace(*lp)) {
		if (*lp == '\0')
			break;
		if (ap < apend)
			*ap++ = *lp;
		lp++;
	}
	if (ap != NULL)
		*ap = '\0';
	while (isascii(*lp) && isspace(*lp))
		lp++;
	return (lp);
}
