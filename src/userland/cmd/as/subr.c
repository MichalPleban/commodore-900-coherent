#include "asm.h"

/*
 * Note an error.
 * If the error is a `q' just
 * give up.
 */
err(c)
{
	if (pass == 2) {
		++nerr;
		if (lflag)
			stoerr(c);
		else {
			printf("%04d %c", line, c);
			if (ifn != NULL)
				printf(" %s", ifn);
			putchar('\n');
		}
	}
	if (c == 'q')
		longjmp(env, 1);
}

/*
 * Note a `u' error.
 * Tag it with the name if not making
 * a listing file.
 */
uerr(id)
char *id;
{
	if (pass == 2) {
		++nerr;
		if (lflag)
			stoerr('u');
		else {
			printf("%04d u %.*s", line, NCPLN, id);
			if (ifn != NULL)
				printf(" %s", ifn);
			putchar('\n');
		}
	}
}

/*
 * Note an 'r' error.
 */
rerr()
{
	err('r');
}

/*
 * Note an 'a' error.
 */
aerr()
{
	err('a');
}

/*
 * Note a 'q' error.
 */
qerr()
{
	err('q');
}

/*
 * Store an error tag into the
 * listing error buffer.
 */
stoerr(c)
register c;
{
	register char *p;

	p = eb;
	while (p < ep)
		if (*p++ == c)
			return;
	if (p < &eb[NERR]) {
		*p++ = c;
		ep = p;
	}
}
