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
 * Timeout queue header.
 */
#ifndef	 TIMEOUT_H
#define	 TIMEOUT_H
#include <types.h>
#include <machine.h>

/*
 * Timer queue.
 */
typedef struct tim {
	struct	 tim *t_next;		/* Pointer to next */
	dmap_t	 t_dmap;		/* Mapping for function */
	int	 t_tinc;		/* Timeout increment */
	int	 (*t_func)();		/* Function to be called */
	char	 *t_farg;		/* Argument */
} TIM;

#ifdef	 KERNEL
/*
 * Global variables.
 */
extern	TIM	timl;			/* Start of timer queue */

#endif

#endif
