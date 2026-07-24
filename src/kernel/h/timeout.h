/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
