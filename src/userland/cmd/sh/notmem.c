/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Tell if this block of memory is NOT in the malloc arena.
 * The shell frees argument vectors wholesale; sfree()/vfree()
 * use this to skip static strings and automatic variables.
 * Walks the libc allocator's circular block list (<malloc.h>):
 * a pointer is malloc memory iff it is the user part &ap[1]
 * of some arena block ap currently marked used.
 */

#include <stdio.h>
#include <malloc.h>

extern	alloc_t	*_a_arena;

notmem(cp)
char *cp;
{
	register alloc_t *p;

	if (cp == NULL || _a_arena == NULL)
		return (1);
	p = _a_arena;
	do {
		if ((char *)&p[1] == cp)
			return (tstused(p) ? 0 : 1);
		p = (alloc_t *)next(p);
	} while (p != _a_arena);
	return (1);
}
