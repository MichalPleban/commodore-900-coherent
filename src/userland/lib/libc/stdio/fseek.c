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
	/* The modulo must be computed in LONG: the old `(int)offset%BUFSIZ'
	 * truncated first, so any offset past 32767 went negative (60882 ->
	 * -4654 -> remainder -46) and the buffer pointers landed BEFORE _bp,
	 * corrupting the heap around the buffer on the next read.  Seen as
	 * nlist("/coherent") crashing ps/mem once the kernel grew past 32K. */
	if (fp->_bp!=NULL)
		fp->_dp = fp->_cp = fp->_bp + (int)(offset%BUFSIZ);
	return (0);
}
