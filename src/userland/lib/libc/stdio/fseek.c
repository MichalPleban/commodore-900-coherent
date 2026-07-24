/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * Seek; first ensure buffer is clean; afterwards put ptrs at right place
 */

#include <stdio.h>

int
fseek(fp, offset, origin)
register FILE	*fp;
long	offset;
int	origin;
{
	long	lseek();

	if (_fpseek(fp)==EOF)
		return (EOF);
	if ((offset=lseek(fileno(fp), offset, origin)) == -1L)
		return (EOF);
	if (fp->_bp!=NULL)
		fp->_dp = fp->_cp = fp->_bp + (int)offset%BUFSIZ;
	return (0);
}
