/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: MIT
 */
/* memcmp() -- K&R, for the games/curses ports. */
int
memcmp(a, b, n)
char *a, *b;
unsigned n;
{
	register unsigned char *p = (unsigned char *)a, *q = (unsigned char *)b;

	while (n-- != 0) {
		if (*p != *q)
			return (*p - *q);
		p++, q++;
	}
	return (0);
}
