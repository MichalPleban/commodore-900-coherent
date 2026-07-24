/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
