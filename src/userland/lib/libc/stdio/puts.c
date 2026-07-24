/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Put string to standard output
 * append '\n'
 */

#include <stdio.h>

void
puts(s)
register char	*s;
{
	for (;  *s;  s++)
		putchar(*s);
	putchar('\n');
}
