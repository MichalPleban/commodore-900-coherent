/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Find the last occurrence of the character c in the
 * string s, or NULL if it does not occur.  Unlike rindex(),
 * c may be '\0': the terminator is part of the string.
 */

#include <stdio.h>

char *
strrchr(s, c)
register char *s;
register int c;
{
	register char *r;

	r = NULL;
	do {
		if (*s == c)
			r = s;
	} while (*s++ != '\0');
	return (r);
}
