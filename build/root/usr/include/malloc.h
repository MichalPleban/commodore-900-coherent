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
 * Structure and macros used by malloc.
 * See malloc source for greater detail.
 */
#ifndef	MALLOC_H
#define	MALLOC_H MALLOC_H

#include <types.h>

typedef	struct	alloc_t	{
	struct	alloc_t	*a_next,
			*a_prev;
} alloc_t;

#define	_BIT_		0x00010000L
#define	next(p)		((vaddr_t)(p)->a_next & ~_BIT_)
#define	prev(p)		((p)->a_prev)
#define	tstused(p)	((vaddr_t)(p)->a_next & _BIT_)
#define	tstfree(p)	(!tstused(p))
#define	setused(p)	((p)->a_next = (vaddr_t)(p)->a_next | _BIT_)
#define	setfree(p)	((p)->a_next = (vaddr_t)(p)->a_next & ~_BIT_)
#define	alength(p)	((char *)(p)->a_next-(char *)(p))
#define	aligned(p)	(!((vaddr_t)(p) & _BIT_))

#endif
