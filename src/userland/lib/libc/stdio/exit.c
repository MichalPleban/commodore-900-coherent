/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Close all files and call sys exit
 */

#include <stdio.h>

int
exit(s)
{
	_finish();
	_exit(s);
}
