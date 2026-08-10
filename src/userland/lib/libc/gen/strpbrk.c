/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Find the first occurrence in the string s of any
 * character from the string set, or NULL if none occurs.
 */

#include <stdio.h>

char *
strpbrk(s, set)
register char *s;
char *set;
{
	register char *p;

	for (; *s != '\0'; s++)
		for (p = set; *p != '\0'; p++)
			if (*s == *p)
				return (s);
	return (NULL);
}
