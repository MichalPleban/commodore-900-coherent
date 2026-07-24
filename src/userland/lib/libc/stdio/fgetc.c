/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Get character function (rather than macro)
 */

#include <stdio.h>

int
fgetc(fp)
register FILE	*fp;
{
	return (getc(fp));
}
