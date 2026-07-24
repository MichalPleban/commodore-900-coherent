/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library Internals
 * Align file seek addr with current pointer in buffer
 * by writing out unflushed data or seeking back over unread data
 * ungot char is discarded
 */

#include <stdio.h>

int
_fpseek(fp)
register FILE	*fp;
{
	extern	long	lseek();

	if (fp->_ff&_FUNGOT)
		(*fp->_gt)(fp);
	if (fp->_dp <= fp->_cp)
		return (fflush(fp));
	if (lseek(fileno(fp), (long)(fp->_cp-fp->_dp), 1) == -1L)
		return (EOF);
	fp->_dp = fp->_cp;
	fp->_cc = 0;
	return (0);
}
