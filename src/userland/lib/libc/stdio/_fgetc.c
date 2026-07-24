/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library Internals
 * Unbuffered input
 */

#include <stdio.h>
#include <errno.h>

int
_fgetc(fp)
register FILE	*fp;
{
	register unsigned char	s[1];
	extern	int	_fputt();

	if (stdout->_pt==&_fputt)		/* special kludge */
		fflush(stdout);
	fp->_cc = 0;
	errno = 0;
	switch (read(fileno(fp), s, 1)) {
	case -1:
		if (errno != EINTR)
			fp->_ff |= _FERR;
		break;
	case 0:
		fp->_ff |= _FEOF;
		break;
	default:
		return (s[0]);
	}
	return (EOF);
}
