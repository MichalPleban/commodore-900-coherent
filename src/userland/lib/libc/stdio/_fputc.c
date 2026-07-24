/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O Library Internals
 * Unbuffered output
 */

#include <stdio.h>
#include <errno.h>

int
_fputc(c, fp)
register unsigned char	c;
register FILE	*fp;
{
	char	s[1] = c;

	fp->_cc = 0;
	errno = 0;
	if (fp->_ff&_FERR || _fpseek(fp)) {
		return (EOF);
	} else if (write(fileno(fp), s, 1) == 1) {
		return (c);
	} else {
		if (errno != EINTR)
			fp->_ff |= _FERR;
		return (EOF);
	}
}
