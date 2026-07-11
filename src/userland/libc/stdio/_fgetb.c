/*
 * Standard I/O Library Internals
 * Buffered Input; read a bufferfull
 */

#include <stdio.h>
#include <errno.h>

int
_fgetb(fp)
register FILE	*fp;
{
	extern	int	_fputt();

	if (fflush(fp))
		return (EOF);
	if (stdout->_pt==&_fputt)	/* special kludge */
		fflush(stdout);
	errno = 0;
	if ((fp->_cc = -read(fileno(fp), fp->_dp, _ep(fp) - fp->_dp)) == 1) {
		if (errno != EINTR)
			fp->_ff |= _FERR;
		fp->_cc = 0;
		return (EOF);
	} else if (fp->_cc == 0) {
		fp->_ff |= _FEOF;
		return (EOF);
	} else {
		fp->_dp -= fp->_cc++;
		return (*fp->_cp++);
	}
}
