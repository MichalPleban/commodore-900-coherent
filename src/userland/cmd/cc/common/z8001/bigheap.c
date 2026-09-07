/*
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: MIT
 */
/*
 * common/z8001/bigheap.c
 * A heap for the compiler passes on the C900.
 *
 * A COHERENT process on the Z8001 grows its heap (brk) inside ONE 64 K data
 * segment, and what is left of that segment after a pass's own data is not
 * enough: cc2 wants about 75 K for the kernel's bio.c and cc0 more than the
 * segment for a 3000-line file.  A large-model link (ld -L) puts every data
 * MODULE that would straddle a segment boundary into a segment of its own,
 * so four modules holding one 60 K array each (arena0.c .. arena3.c) give a
 * pass 240 K of memory it can carve itself.  This is that carver: the K&R
 * first-fit allocator, ONE FREE LIST PER ARENA, replacing malloc/free/
 * calloc/realloc for the pass (a client's own definition wins over the
 * shared libc's).  It never calls sbrk, so the shared libc's own heap, which
 * stdio uses through the library's malloc, is untouched.
 *
 * One list per arena is not tidiness.  The K&R allocator keeps its list in
 * address order and coalesces a block with the one whose address follows it,
 * and it decides both by comparing pointers.  Every arena starts at the same
 * offset of a different segment, so with one list the end of arena 0 looks
 * adjacent to the start of arena 1 to any compare that sees offsets, and two
 * segments get merged into one block.  So a block is placed in an arena by
 * an integer range test on its whole address, and pointers are only ever
 * compared inside one arena, where every one lies in the same segment.
 * A block never straddles a segment either: each arena lies inside one.
 *
 * Small requests are served from the first arena that can, large ones (4 K
 * and up: the object writer's growing fixup and symbol arrays) from the LAST
 * that can, so the churn of small blocks does not fragment the space a large
 * array will want to grow into.
 */
#define	NARENA	4
#define	ARENA	60000
#define	BIG	4096

typedef long Align;
union header {
	struct {
		union header *ptr;	/* next free block */
		unsigned size;		/* in header units */
	} s;
	Align x;
};
typedef union header Header;

extern char bigarena0[], bigarena1[], bigarena2[], bigarena3[];

static Header base[NARENA];		/* each list's anchor */
static Header *freep[NARENA];
static unsigned long alo[NARENA], ahi[NARENA];	/* arena bounds as integers */
static int seeded;

char *malloc();
static afree();

static
seed()
{
	register Header *hp;
	register char *a;
	register int i;

	seeded = 1;
	for (i = 0; i < NARENA; ++i) {
		a = i == 0 ? bigarena0 : i == 1 ? bigarena1
		  : i == 2 ? bigarena2 : bigarena3;
		alo[i] = (unsigned long)a;
		ahi[i] = alo[i] + ARENA;
		base[i].s.ptr = freep[i] = &base[i];
		base[i].s.size = 0;
		/* start on a header boundary inside the array */
		a += sizeof(Header) - ((unsigned)a % sizeof(Header));
		hp = (Header *)a;
		hp->s.size = (ARENA - 2 * sizeof(Header)) / sizeof(Header);
		afree(i, (char *)(hp + 1));
	}
}

/*
 * First fit inside one arena, or NULL.
 */
static char *
amalloc(i, nunits) int i; register unsigned nunits;
{
	register Header *p, *prevp;

	prevp = freep[i];
	for (p = prevp->s.ptr; ; prevp = p, p = p->s.ptr) {
		if (p->s.size >= nunits) {
			if (p->s.size == nunits)
				prevp->s.ptr = p->s.ptr;
			else {
				p->s.size -= nunits;
				p += p->s.size;
				p->s.size = nunits;
			}
			freep[i] = prevp;
			return (char *)(p + 1);
		}
		if (p == freep[i])
			return (char *)0;
	}
}

char *
malloc(nbytes) unsigned nbytes;
{
	register unsigned nunits;
	register int i;
	register char *p;

	if (!seeded)
		seed();
	nunits = (nbytes + sizeof(Header) - 1) / sizeof(Header) + 1;
	if (nbytes >= BIG) {
		for (i = NARENA - 1; i >= 0; --i)
			if ((p = amalloc(i, nunits)) != (char *)0)
				return p;
	} else {
		for (i = 0; i < NARENA; ++i)
			if ((p = amalloc(i, nunits)) != (char *)0)
				return p;
	}
	return (char *)0;		/* the arenas are full */
}

/*
 * Return a block to its arena's list, coalescing with its neighbours.
 */
static
afree(i, ap) int i; char *ap;
{
	register Header *bp, *p;

	bp = (Header *)ap - 1;
	for (p = freep[i]; !(bp > p && bp < p->s.ptr); p = p->s.ptr)
		if (p >= p->s.ptr && (bp > p || bp < p->s.ptr))
			break;		/* at one end of the list */
	if (bp + bp->s.size == p->s.ptr) {
		bp->s.size += p->s.ptr->s.size;
		bp->s.ptr = p->s.ptr->s.ptr;
	} else
		bp->s.ptr = p->s.ptr;
	if (p + p->s.size == bp) {
		p->s.size += bp->s.size;
		p->s.ptr = bp->s.ptr;
	} else
		p->s.ptr = bp;
	freep[i] = p;
}

static
whose(ap) char *ap;
{
	register unsigned long a;
	register int i;

	a = (unsigned long)ap;
	for (i = 0; i < NARENA; ++i)
		if (a >= alo[i] && a < ahi[i])
			return i;
	return -1;
}

free(ap) char *ap;
{
	register int i;

	if (ap == (char *)0 || (i = whose(ap)) < 0)
		return;			/* not ours: nothing to do */
	afree(i, ap);
}

char *
calloc(n, m) unsigned n, m;
{
	register char *p, *q;
	register unsigned i;

	i = n * m;
	if ((p = malloc(i)) != (char *)0)
		for (q = p; i != 0; --i)
			*q++ = 0;
	return p;
}

char *
realloc(ap, nbytes) char *ap; unsigned nbytes;
{
	register char *p, *q, *r;
	register unsigned old, i;

	if (ap == (char *)0)
		return malloc(nbytes);
	old = (((Header *)ap - 1)->s.size - 1) * sizeof(Header);
	if ((p = malloc(nbytes)) == (char *)0)
		return p;
	i = old < nbytes ? old : nbytes;
	for (q = p, r = ap; i != 0; --i)
		*q++ = *r++;
	free(ap);
	return p;
}

/* end of common/z8001/bigheap.c */
