/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library
 * flush unwritten data, release allocated buffers, call sys close
 */

#include <stdio.h>

int
fclose(fp)
register FILE	*fp;
{
	register int	st;

	if (!(fp->_ff&_FINUSE))
		return (EOF);
	st = fflush(fp);
	close(fileno(fp));
	if (fp->_bp!=NULL && !(fp->_ff&_FSTBUF))
		free(fp->_bp);
	fp->_ff = 0;
	return (st);
}
