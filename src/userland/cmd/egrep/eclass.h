/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


/*
 * equivalence class
 *	The set of ASCII chars is refined by the sets of chars used by
 * the regular expression.  The result is a set of equivalence classes:
 * disjoint sets of chars, whose union is the ASCII set.
 *	An eclass struct describes one equivalence class.  The structs
 * are linked, and headed by `eclasses'.
 */
struct eclass {
	char		e_c,		/* used if only one char in set */
			e_class,	/* ID # of this eclass */
			*e_b;		/* used if many chars in set */
	struct eclass	*e_next;	/* next eclass (or 0) */
};

extern int	n_ec;			/* # eclasses (used to set e_class) */
