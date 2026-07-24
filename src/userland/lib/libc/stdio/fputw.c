/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Put word (int) to file
 */

#include <stdio.h>

int
fputw(w, fp)
int	w;
register FILE	*fp;
{
	putc(w>>8, fp);
	putc(w, fp);
	return (w);
}
