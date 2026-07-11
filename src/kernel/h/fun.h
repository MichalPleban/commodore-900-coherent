/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
/*
 * Coherent.
 * Some useful functions.
 */

/*
 * Number of elements in a structure.
 */
#define nel(a)		(sizeof(a)/sizeof((a)[0]))

/*
 * Character pointer to integer.
 */
#define cpi(p)		((char *)(p) - (char *)0)

/*
 * Round a number upwards to be a multiple of another.
 */
#define	roundu(n1, n2)	(((n1)+(n2)-1)/n2*n2)

/*
 * Offset in bytes of `m' in the structure `s'.
 */
#define offset(s, m)	((int) &(((struct s *) 0)->m))

/*
 * Add an unsigned number without overflow.
 */
#define	addu(v, n) {							\
	unsigned x;							\
									\
	x = v + (n);							\
	v = x>=v ? x : MAXU;						\
}

/*
 * Subtract an unsigned number without overflow.
 */
#define	subu(v, n) {							\
	unsigned x;							\
									\
	x = v - (n);							\
	v = x<=v ? x : 0;						\
};
