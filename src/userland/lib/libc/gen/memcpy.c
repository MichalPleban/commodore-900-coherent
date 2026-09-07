/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: MIT
 */
/* memcpy() -- K&R, for the games ports. */
char *
memcpy(dst, src, n)
char *dst, *src;
unsigned n;
{
	register char *d, *s;

	d = dst; s = src;
	while (n-- != 0)
		*d++ = *s++;
	return (dst);
}
