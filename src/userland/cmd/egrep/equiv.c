/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * equivalence classes
 */
#include	<ctype.h>
#include	"egrep.h"
#include	"newt.h"
#include	"eclass.h"


char		etab[NCHARS];		/* map ASCII to eclass # */
struct eclass	*eclasses;		/* head of eclass list */

bool		intersect( );
struct eclass	*neweclass( );


/*
 * check equivalence
 *	A set of eclasses must exist such that their union equals the
 * chars in `np'.  If not, the equivalence relation must be "refined".
 */
equiv( np)
register struct newt	*np;
{
	register struct eclass	*ep,
				*p;
	struct eclass		e;

	if (eclasses == NULL) {
		eclasses = neweclass( );
		eclasses->e_c = 0;
		eclasses->e_b = newbits( TRUE);
		eclasses->e_next = NULL;
		eclasses->e_class = n_ec++;
	}

	for (ep=eclasses; ep; ep=ep->e_next) {
		if (not intersect( np, ep, &e))
			continue;
		p = neweclass( );
		*p = e;
		p->e_class = n_ec++;
		p->e_next = eclasses;
		eclasses = p;
	}
}


/*
 * is there a transition?
 *	Return TRUE if there is a transition from `np' on the chars given
 * by `ep'.
 */
bool
eqtrans( ep, np)
register struct eclass	*ep;
register struct newt	*np;
{
	register	i;

	if (ep->e_b) {
		if (np->n_b) {
			for (i=0; i<NCHARS/NBCHAR; ++i)
				if (ep->e_b[i] & np->n_b[i])
					return (TRUE);
		}
		else
			return (bittst( np->n_c, ep->e_b));
	}
	else {
		if (np->n_b)
			return (bittst( ep->e_c, np->n_b));
		else
			return (ep->e_c == np->n_c);
	}
	return (FALSE);
}


/*
 * intersect char sets
 *	If the intersection of `np' and `ep0' is a proper subset of `ep0',
 * then store the intersection in `ep1', store the difference of
 * `ep0' - `ep1' in `ep0' and return TRUE.
 */
static bool
intersect( np, ep0, ep1)
register struct newt	*np;
register struct eclass	*ep0;
struct eclass		*ep1;
{
	register	i;
	bool		classcheck( );

	if (ep0->e_b == NULL)
		return (FALSE);

	if (np->n_b == NULL) {
		i = np->n_c;
		if (not bittst( i, ep0->e_b))
			return (FALSE);
		bitclr( i, ep0->e_b);
		ep1->e_c = i;
		etab[i] = n_ec;
		ep1->e_b = NULL;
	}
	else {
		if (not classcheck( np->n_b, ep0->e_b))
			return (FALSE);
		ep1->e_b = newbits( FALSE);
		for (i=0; i<NCHARS/NBCHAR; ++i) {
			ep1->e_b[i] = ep0->e_b[i] & np->n_b[i];
			ep0->e_b[i] &= ~np->n_b[i];
		}
		for (i=0; i<NCHARS; ++i)
			if (bittst( i, ep1->e_b))
				etab[i] = n_ec;
		ep1->e_c = 0;
	}

	return (TRUE);
}


/*
 * check intersection
 *	Specific check for char sets represented as bitmaps.  If the
 * intersection of `p' and `q' is a proper subset of `q' return TRUE.
 * Non-modifying nature of this routine is optimized for the "-y" option,
 * where intersections of [Aa] and [Bb] are commonplace.
 */
static bool
classcheck( p, q)
register char	*p,
		*q;
{
	register	i;
	bool		k1,
			k2;

	k1 = FALSE;
	k2 = FALSE;

	i = NCHARS / NBCHAR;
	do {
		if (*q & *p)
			k1 = TRUE;
		if (*q++ & ~*p++)
			k2 = TRUE;
	} while (--i);

	return (k1 && k2);
}


/*
 * allocate new eclass
 */
static struct eclass	*
neweclass( )
{
	register struct eclass	*ep;

	ep = malloc( sizeof *ep);
	if (ep == NULL)
		nomem( );
	return (ep);
}
