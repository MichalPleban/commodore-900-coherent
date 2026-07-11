/*
 * allocate and clear
 */

#include <stdio.h>

char *
calloc(items, size)
unsigned int	items;
register unsigned int	size;
{
	register char	*bp,
			*cp;

	size *= items;
	if ((bp=malloc(size)) != NULL)
		for (cp = bp;  size;  --size)
			*cp++ = 0;
	return (bp);
}
