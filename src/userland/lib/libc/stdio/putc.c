/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * putc function for those too lazy or cheap to use macro
 */

#include <stdio.h>
#undef	putc

int
putc(c, fp)
char	c;
FILE	*fp;
{
	return (fputc(c, fp));
}
