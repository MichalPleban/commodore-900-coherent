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
