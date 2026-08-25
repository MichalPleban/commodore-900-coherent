/*
 * velsnap.c - the model, packed into one block and put back.
 *
 * The editor's one-level undo keeps a PRE-IMAGE of the drawing.  As
 * three fixed arrays that was 19 248 bytes of bss -- a second model at
 * MAXOBJ, whether the drawing held 400 objects or none -- and this
 * machine allocates a process's data at exec, so an empty editor paid
 * all of it.  Real drawings run to a few dozen objects, so a snapshot
 * is one heap block sized to the DRAWING:
 *
 *	[n DOBJs][pp shorts of ppool][tp bytes of tpool]
 *
 * Every offset in it is even (sizeof(DOBJ) is, and the pool is
 * shorts), so USPP/USTP stay aligned.
 *
 * It lives in the library, in a member of its OWN, for two reasons.
 * It is a MODEL operation and nothing about it is graphical -- so a
 * plain test program can link it and prove a pack/restore round trip
 * without a window, which is not a thing the editor's own undo could
 * ever offer.  And a member is pulled only for a symbol something
 * references, so no other tool in the suite carries it.
 */
#include <stdio.h>
#include "vellum.h"

extern char	*malloc();

/* Byte copy (no memcpy in this libc's K&R corner). */
static
bmove(d, sp, n)
register char *d, *sp;
register int n;
{
	while ( n-- > 0 )
		*d++ = *sp++;
	return 0;
}

/* The size the live model packs into. */
usnapsize()
{
	return nobj * sizeof(DOBJ) + ppuse * 2 + tpuse;
}

/* The LIVE model into a fresh block of exactly that size, or 0.  The
 * caller keeps nobj/ppuse/tpuse alongside it: they are the block's
 * shape, and USPP/USTP need them to find the pools. */
char *
usnappack()
{
	register char *b;

	if ( (b = malloc((unsigned)usnapsize())) == (char *)0 )
		return (char *)0;
	bmove(b, (char *)obj, nobj * sizeof(DOBJ));
	bmove((char *)USPP(b, nobj), (char *)ppool, 2 * ppuse);
	bmove(USTP(b, nobj, ppuse), tpool, tpuse);
	return b;
}

/* ... and back over the live model.  n/pp/tp are the block's shape as
 * usnappack() left it. */
usnaprestore(b, n, pp, tp)
char *b;
{
	bmove((char *)obj, b, n * sizeof(DOBJ));
	bmove((char *)ppool, (char *)USPP(b, n), 2 * pp);
	bmove(tpool, USTP(b, n, pp), tp);
	nobj = n;
	ppuse = pp;
	tpuse = tp;
	return 0;
}
