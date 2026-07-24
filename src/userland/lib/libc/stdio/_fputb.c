/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library Internals
 * Buffered output
 */

#include <stdio.h>

int
_fputb(c, fp)
unsigned char	c;
register FILE	*fp;
{
	if (_fpseek(fp))
		return (EOF);
	fp->_cc = _ep(fp) - fp->_dp - 1;
	return (*fp->_cp++=c);
}
