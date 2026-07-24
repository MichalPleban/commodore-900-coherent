/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
