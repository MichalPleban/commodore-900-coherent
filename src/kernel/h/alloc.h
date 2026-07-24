/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Allocator.
 */
#ifndef	 ALLOC_H
#define	 ALLOC_H

/*
 * Structure for allocator.
 */
typedef struct all {
	union {
		char	*a_link;
		char	a_free[2];
	};			/* anonymous: was named `a_union' (see NOTE) */
	char	a_data[];
} ALL;

#if 0
/*
 * Portable defines for the allocator.
 */
#define align(p)	((ALL *)NULL + ((p) - (ALL *)NULL))
#define link(p)		(align((p)->a_link))
#define	tstfree(p)	((p)->a_link == (char *) link(p))
#define setfree(p)	((p)->a_link = (char *) link(p))
#define setused(p)	((p)->a_link = (char *) link(p) + 1)

#endif

#ifdef	KERNEL
/*
 * Functions and externals.
 */
extern	char	*alloc();
extern	ALL	*setarena();

#endif

#endif
