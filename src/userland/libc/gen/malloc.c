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
	register unsigned int	len;
	char		sbrked = 0,
			*sbrk();

	if (size == 0)
		return (NULL);
	size = roundup(size + sizeof (alloc_t), sizeof (alloc_t));

	ap = _a_block;
	for (;;) {	/* at most twice through */
		if (_a_arena != NULL) do {
			if (tstfree(ap) && size <= (len=alength(ap))) {
				if (size + sizeof (alloc_t) < len) { /* slop */
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
			maunder("Corrupt arena in malloc\n");
		if (_a_arena != NULL
		 && sbrk(0) == &(bp=prev(_a_arena))[1]
		 && tstfree(prev(bp)))		/* free block at end of mem */
			len = roundup(size - alength(prev(bp)), 1<<9);
		else
			len = roundup(size + sizeof (alloc_t), 1<<9);
		if ((char*)(ap=sbrk(0))+len <= (char*)ap  /* wraparound */
		 || (ap=sbrk(len)) == NULL)	/* no space */
			return (NULL);
		if (_a_arena == NULL) {		/* first alloc */
			_a_arena = ap;
		} else if (ap != &bp[1]) {	/* discontiguous new block */
			register int tmp;

			tmp = roundup(size + sizeof(alloc_t), 1<<9);
			if (sbrk(tmp-len) == NULL)
				return (NULL);
			len = tmp;
			bp->a_next = ap;
			ap->a_prev = bp;
			setused(bp);
		} else if (tstfree(prev(bp))) {	/* extend free block */
			ap = prev(bp);
			len += alength(ap) + sizeof (alloc_t);
		} else {			/* new free block */
			ap = bp;
			len += sizeof (alloc_t);
		}
		bp = &((char *) ap)[len - sizeof (alloc_t)];	/* end mark */
		ap->a_next = bp;
		bp->a_prev = ap;
		bp->a_next = _a_arena;
		_a_arena->a_prev = bp;
		setused(bp);
	}
}

free(cp)
char	*cp;
{
	register alloc_t *ap = cp,
			 *pp,
			 *np;

	--ap;
	if (!aligned(ap)
	 || ap < _a_arena
	 || prev(_a_arena) <= ap
	 || next(pp=prev(ap)) != ap
	 || prev(np=next(ap)) != ap
	 || tstfree(ap))
		maunder("Bad pointer in free\n");
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
maunder(plaint)
char	*plaint;
{
	write(2, plaint, strlen(plaint));
	abort();
}
