/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Find the first occurrence of the character c in the
 * string s, or NULL if it does not occur.  Unlike index(),
 * c may be '\0': the terminator is part of the string.
 */

#include <stdio.h>

char *
strchr(s, c)
register char *s;
register int c;
{
	do {
		if (*s == c)
			return (s);
	} while (*s++ != '\0');
	return (NULL);
}
