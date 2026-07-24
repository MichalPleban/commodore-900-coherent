/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
