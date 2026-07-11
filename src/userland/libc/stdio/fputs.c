/*
 * Standard I/O Library
 * Put string to file
 */

#include <stdio.h>

void
fputs(s, fp)
register char	*s;
register FILE	*fp;
{
	for (;  *s;  s++)
		putc(*s, fp);
}
