/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * creation of the DFA
 */
#include	"egrep.h"
#include	"newt.h"
#include	"dragon.h"
#include	"eclass.h"

int	uniq;
int	n_id;
int	n_ec;

static			bmsize;		/* size of bitmap `d_b' */
static struct dragon	d;		/* prototype dragon */

static struct dragon	*dragons;	/* head of the dragon list */

struct dragon	*member( ),
		*enter( );


/*
 * create the DFA start state
 */
struct dragon	*
initdfa( np)
struct newt	*np;
{

	if (d.d_s)
		free( (char *)d.d_s);
	d.d_s = malloc( n_id*sizeof( *d.d_s));
	if (d.d_s == NULL)
		nomem( );
	d.d_s[0] = np;
	d.d_s[1] = NULL;
	bmsize = (n_id+NBCHAR-1) / NBCHAR;
	if (d.d_b)
		free( (char *)d.d_b);
	d.d_b = malloc( bmsize);
	if (d.d_b == NULL)
		nomem( );
	dragons = NULL;
	++uniq;
	e_closure( );
	return (enter( np->n_fp));
}


/*
 * construct the DFA
 *	This task is done incrementally, after searching has started.
 * The current dragon `dp' needs a transition on the input `ec' given
 * the NFA `np'.  The new dragon is determined, entered into `dp->d_p',
 * and returned.  If the new dragon has never been encountered, it is
 * remembered.  A special case arises if the current dragon is an accept
 * state: all transitions loop to itself.
 *	Note that `ec' is an equivalence class, not a char.
 */
struct dragon	*
makedfa( dp, ec, np)
struct dragon	*dp;
struct newt	*np;
{
	register struct eclass	*ep;
	register struct dragon	*p;
	extern struct eclass	*eclasses;

	if (dp->d_success) {
		dp->d_p[ec] = dp;
		return (dp);
	}
	for (ep=eclasses; ep->e_class!=ec; ep=ep->e_next)
		;
	++uniq;
	gettrans( dp, ep);
	e_closure( );
	p = member( );
	if (p == NULL)
		p = enter( np->n_fp);
	dp->d_p[ec] = p;
	return (p);
}


/*
 * get transitions
 *	Construct in `d.d_s' the set of newts to which there is a
 * transition on `ep' from some newt in `dp->d_s'.
 */
static
gettrans( dp, ep)
struct dragon	*dp;
struct eclass	*ep;
{
	register struct newt	**pp,
				*np,
				*p,
				**npp;
	bool			eqtrans( );

	npp = d.d_s;

	pp = dp->d_s;
	while (np = *pp++)
		if ((p = np->n_cp)
		and (np->n_c != EPSILON)
		and (eqtrans( ep, np))) {
			*npp++ = p;
			p->n_uniq = uniq;
		}
	*npp = NULL;
}


/*
 * locate a dragon
 *	The set of newts given by `d.d_s' are sought in the DFA.  The
 * appropriate dragon is returned, else 0.  The use of bitmap `d_b' allows
 * comparing (unsorted) `d_s' sets in time O(N).  Hashing of `d_s' and
 * "self-organizing" the DFA also help to keep things fast.
 */
static struct dragon	*
member( )
{
	register struct dragon	*dp;
	register char	*p,
			*q;
	int		i;

	dp = dragons;

	do {
		if (dp->d_hash == d.d_hash) {
			p = dp->d_b;
			q = d.d_b;
			i = bmsize; do {
				if (*p++ != *q++)
					break;
			} while (--i);
			if (i == 0) {
				if (dp == dragons)
					break;
				if (dp->d_next)
					dp->d_next->d_last = dp->d_last;
				dp->d_last->d_next = dp->d_next;
				dragons->d_last = dp;
				dp->d_next = dragons;
				dp->d_last = NULL;
				dragons = dp;
				break;
			}
		}
	} while (dp = dp->d_next);

	return (dp);
}


/*
 * enter the dragon
 *	The prototype dragon `d' is used to create a dragon, which is
 * entered in the DFA.
 */
static struct dragon	*
enter( finalnp)
struct newt	*finalnp;
{
	register struct dragon	*p;
	register		i;

	p = malloc( sizeof *p);
	if (p == NULL)
		nomem( );
	p->d_success = FALSE;
	p->d_hash = d.d_hash;
	for (i=0; d.d_s[i]; ++i)
		if (d.d_s[i] == finalnp)
			p->d_success = TRUE;
	p->d_s = malloc( (i+1)*sizeof( *p->d_s));
	if (p->d_s == NULL)
		nomem( );
	for (i=0; p->d_s[i]=d.d_s[i]; ++i)
		;
	p->d_b = malloc( bmsize);
	if (p->d_b == NULL)
		nomem( );
	for (i=0; i<bmsize; ++i)
		p->d_b[i] = d.d_b[i];
	p->d_p = malloc( n_ec*sizeof( *p->d_p));
	if (p->d_p == NULL)
		nomem( );
	for (i=0; i<n_ec; ++i)
		p->d_p[i] = NULL;
	if (dragons)
		dragons->d_last = p;
	p->d_next = dragons;
	dragons = p;
	p->d_last = NULL;
	return (p);
}


/*
 * perform epsilon-closure
 *	Epsilon-transition `d.d_s' is computed and the result placed
 * in `d.d_s'.  Ensuring a newt is not already on the list requires
 * time O(N), thanks to `uniq'.
 */
static
e_closure( )
{
	register struct newt	**nqq,
				*np,
				*p;
	struct newt		**npp;

	d.d_hash = 0;
	for (nqq=d.d_s; *nqq++; )
		;
	--nqq;

	for (npp=d.d_s; npp<nqq; ) {
		np = *npp++;
		d.d_hash += (int)np;
		if ((p = np->n_cp)
		and (np->n_c == EPSILON)
		and (p->n_uniq != uniq)) {
			*nqq++ = p;
			p->n_uniq = uniq;
		}
		if ((p = np->n_ep)
		and (p->n_uniq != uniq)) {
			*nqq++ = p;
			p->n_uniq = uniq;
		}
	}
	*nqq = NULL;

	bmbuild( );
}


/*
 * build bitmap
 *	A bitmap of `d.d_s' is built in `d.d_b'.
 */
static
bmbuild( )
{
	register struct newt	*np;
	register char		*p;
	register		i;

	p = d.d_b;
	i = bmsize; do {
		*p++ = 0;
	} while (--i);
	i = 0;
	while (np = d.d_s[i++])
		bitset( np->n_id, d.d_b);
}
