/*
 * Standard I/O Library
 * Write out any unwritten data in buffer
 */

#include <stdio.h>
#include <errno.h>

int
fflush(fp)
register FILE	*fp;
{
	register int	cc;

	fp->_cc = 0;
	errno = 0;
	if (fp->_ff&_FERR) {
		return (EOF);
	} else if ((cc = fp->_cp - fp->_dp) <= 0
	 || write(fileno(fp), fp->_dp, cc) == cc
	 || errno == EINTR) {
		if (cc < 0)
			;
		else if (fp->_cp == _ep(fp))
			fp->_dp = fp->_cp = fp->_bp;
		else
			fp->_dp = fp->_cp;
		return (0);
	} else {
		fp->_ff |= _FERR;
		return (EOF);
	}
}
