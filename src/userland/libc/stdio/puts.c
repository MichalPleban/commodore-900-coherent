/*
 * Standard I/O Library
 * Put string to standard output
 * append '\n'
 */

#include <stdio.h>

void
puts(s)
register char	*s;
{
	for (;  *s;  s++)
		putchar(*s);
	putchar('\n');
}
