/*
 * Standard I/O Library Internals
 * Terminal output (buffered by line)
 */

#include <stdio.h>

int
_fputt(c, fp)
unsigned char	c;
register FILE	*fp;
{
	fp->_cc = 0;
	if (fp->_cp==_ep(fp) && fflush(fp)
	 || (*fp->_cp++ = c) == '\n' && fflush(fp))
		return (EOF);
	return (c);
}
