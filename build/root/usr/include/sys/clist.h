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
 * Character list.
 */
#ifndef	 CLIST_H
#define	 CLIST_H
#include <types.h>
#include <machine.h>

/*
 * Character list structure.
 */
typedef struct clist {
	cmap_t	cl_fp;			/* Pointer to next */
	char	cl_ch[NCPCL];		/* Characters */
} CLIST;

/*
 * Character queue structure.
 */
typedef struct cqueue {
	int	cq_cc;			/* Character count */
	cmap_t	cq_ip;			/* Input pointer */
	int	cq_ix;			/* Input index */
	cmap_t	cq_op;			/* Output pointer */
	int	cq_ox;			/* Output index */
} CQUEUE;

#ifdef KERNEL
extern	int	cltwant;		/* A wanted flag */
extern	cmap_t	cltfree;		/* Character free list */

#endif

#endif
