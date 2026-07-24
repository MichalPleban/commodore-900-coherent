/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Memory allocator
 * Maintains address order list of contiguous blocks, with discontinuities in
 * arena represented as used blocks.
 * Blocks are doubly linked, and marked busy by bit in first link.
 * Last block consists of header only, is always marked used, linked to first.
 * Allocated by circular first fit; coalesced when freed.
 */
#include <stdio.h>
#include <malloc.h>

#define	roundup(i,j)	(((i)+(j)-1)&~((j)-1))	/* j must be power of 2 */

alloc_t	*_a_arena = NULL,
	*_a_block = NULL;

char *
malloc(size)
unsigned int	size;
{
	register alloc_t *ap,
			 *bp;
	unsigned int	len;
	char		sbrked = 0;

	if (size == 0)
		return (NULL);
	size = roundup(size+sizeof(alloc_t), sizeof(alloc_t));

	ap = _a_block;
	for (;;) {	/* at most twice through */
		if (_a_arena != NULL) do {
			if (tstfree(ap) && size <= (len=alength(ap))) {
				if (size+sizeof(alloc_t) < len) { /* extra */
					bp = &((char *)ap)[size];
					bp->a_next = ap->a_next;
					bp->a_prev = ap;
					ap->a_next =
					ap->a_next->a_prev = bp;
				}
				_a_block = ap->a_next;
				setused(ap);
				return (&ap[1]);
			}
			ap = next(ap);
		} while (ap != _a_block);	/* scan whole arena */

		if (sbrked++)
			complain("Corrupt arena in malloc\n");
		ap = sbrk(len = roundup(size+sizeof(alloc_t), 1<<9));
		if ((int)ap == 0  ||  (int)ap == -1)
			return (NULL);
		if (_a_arena == NULL) {
			_a_arena = ap;
		} else if (ap == &(bp=prev(_a_arena))[1]) {	/* no gap */
			ap = bp;
		} else {	/* someone else also sbrk'd; mark his used */
			bp->a_next = ap;
			ap->a_prev = bp;
			setused(bp);
		}
		bp = &((char *)ap)[len-sizeof(alloc_t)];	/* end marker */
		ap->a_next = bp;
		bp->a_prev = ap;
		bp->a_next = _a_arena;
		_a_arena->a_prev = bp;
		setused(bp);
		{	register alloc_t *cp;
			if (tstfree(cp=ap->a_prev)) {	/* coalesce with prev */
				cp->a_next = bp;
				bp->a_prev = ap = cp;
			}
		}
	}
}

free(cp)
char	*cp;
{
	register alloc_t *ap = cp,
			 *pp,
			 *np;

	--ap;
	if ((unsigned)ap & 1
	 || ap < _a_arena
	 || prev(_a_arena) <= ap
	 || next(pp=prev(ap)) != ap
	 || prev(np=next(ap)) != ap
	 || tstfree(ap))
		complain("Bad pointer in free\n");
	setfree(ap);
	if (tstfree(np)) {	/* coalesce with next */
		ap->a_next = np = np->a_next;
		np->a_prev = ap;
	}
	if (tstfree(pp)) {	/* coalesce with prev */
		pp->a_next = np;
		np->a_prev = ap = pp;
	}
	_a_block = ap;
}

static
complain(plaint)
char	*plaint;
{
	write(2, plaint, strlen(plaint));
	abort();
}

/*
 * Tell if this block of memory is in the malloc arena.
 */
notmem(cp)
char	*cp;
{
	register alloc_t *ap = cp,
			 *pp,
			 *np;

	if (ap == NULL
	 || (unsigned)ap & 1
	 || --ap < _a_arena
	 || prev(_a_arena) <= ap
	 || next(pp=prev(ap)) != ap
	 || prev(np=next(ap)) != ap
	 || tstfree(ap))
		return (1);
	return (0);
}
