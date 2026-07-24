/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * getc function for those too lazy or cheap to use macro
 */

#include <stdio.h>
#undef	getc

int
getc(fp)
FILE	*fp;
{
	return (fgetc(fp));
}
