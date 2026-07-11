/*
 * Coherent Standard I/O Library.
 * Index function that scans the
 * string from the end towards the
 * front.
 */

#include <stdio.h>

char *
rindex(s, c)
register unsigned char *s;
register c;
{
	register unsigned char *ss;

	for (ss = s; *s++; )
		;
	while (s > ss)
		if (*--s == c)
			return (s);
	return (NULL);
}
