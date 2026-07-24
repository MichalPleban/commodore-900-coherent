/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Get word (int)
 * Routine rather than macro, for the aesthetically inclined
 */

#include <stdio.h>
#undef	getw

int
getw(fp)
register FILE	*fp;
{
	register int	c0, c1;

	if ((c0=getc(fp))==EOF)
		return (EOF);
	else if ((c1=getc(fp))==EOF) {
		fp->_ff |= _FERR;
		return (EOF);
	} else
		return (c0<<8|c1);
}
