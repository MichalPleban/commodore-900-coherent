/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Rewind (position at beginning) file
 */

#include <stdio.h>

int
rewind(fp)
register FILE	*fp;
{
	return (fseek(fp, 0L, 0));
}
