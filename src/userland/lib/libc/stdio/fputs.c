/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Put string to file
 */

#include <stdio.h>

void
fputs(s, fp)
register char	*s;
register FILE	*fp;
{
	for (;  *s;  s++)
		putc(*s, fp);
}
