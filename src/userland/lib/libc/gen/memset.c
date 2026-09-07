/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: MIT
 */
/*
 * memset() -- K&R implementation for the games ports (the 0.7.3 libc
 * has kclear-style helpers but no public memset).
 */
char *
memset(s, c, n)
char *s;
int c;
unsigned n;
{
	register char *p;

	p = s;
	while (n-- != 0)
		*p++ = c;
	return (s);
}
