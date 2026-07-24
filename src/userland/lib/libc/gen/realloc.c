/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Change size of allocated block
 */

#include <stdio.h>
#include <malloc.h>

extern	alloc_t	*_a_block;

char *
realloc(cp, nsize)
register char	*cp;
unsigned int	nsize;
{
	register char	*np,
			*op;
	unsigned int	osize;

	op = cp - sizeof(alloc_t);
	osize = alength((alloc_t *)op) - sizeof(alloc_t);
	free(cp);
	/* try to align new block with old */
	/* by grabbing any free memory below old block */
	if ((char *)_a_block < op)
		op = malloc(op - (char *)_a_block - sizeof(alloc_t));
	else
		op = NULL;
	np = malloc(nsize);
	if (op != NULL)
		free(op);
	if (np == NULL || np == cp)
		return (np);
	if (osize > nsize)
		osize = nsize;
	for (op = np;  osize;  --osize)
		*op++ = *cp++;
	return (np);
}
