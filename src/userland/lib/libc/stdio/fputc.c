/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Put char function (rather than macro)
 */

#include <stdio.h>

int
fputc(c, fp)
char	c;
register FILE	*fp;
{
	return (putc(c, fp));
}
