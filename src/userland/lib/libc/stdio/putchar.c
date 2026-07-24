/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * putchar function for those too lazy to include stdio.h
 */

#include <stdio.h>
#undef	putchar

int
putchar(c)
unsigned char	c;
{
	return (putc(c, stdout));
}
