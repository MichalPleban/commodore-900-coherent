/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
