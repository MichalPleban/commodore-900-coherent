/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Some useful and miscellaneous things.
 */
#define	 KERNEL
#include <types.h>
#include <timeout.h>
#include <machine.h>
#include <param.h>
#include <fun.h>

/*
 * Null
 */
#ifndef	NULL		/* machine.h doesn't have any ideas */
#define NULL	0
#endif

/*
 * Storage management functions.
 */
extern	char		*alloc();
#define	kalloc(n)	alloc(allkp, n)
#define kfree(p)	free(p)

/*
 * Functions for copying between kernel and segments.
 */
#define kscopy(k, s, o, n)	kpcopy(k, ctob((paddr_t)s->s_mbase)+o, n)
#define skcopy(s, o, k, n)	pkcopy(ctob((paddr_t)s->s_mbase)+o, k, n)

/*
 * Time of day structure.
 */
typedef struct TIME {
	time_t	t_time;			/* Time and date */
	int	t_tick;			/* Clock ticks into this second */
	int	t_zone;			/* Time zone */
	int	t_dstf;			/* Daylight saving time used */
} TIME;

/*
 * General global variables.
 */
extern	int	 debflag;		/* General debug flag */
extern	int	 alcflag;		/* Service alarms */
extern	int	 batflag;		/* Turn on clock flag */
extern	int	 outflag;		/* Device timeouts */
extern	int	 ttyflag;		/* Console is present */
extern	int	 mactype;		/* Machine type */
extern	unsigned utimer;		/* Unsigned timer */
extern	TIM	stimer;			/* Swap timer */
extern	unsigned msize;			/* Memory size in K */
extern	unsigned asize;			/* Alloc size in bytes */
extern	TIME	 timer;			/* Current time */
extern	char	 *icodep;		/* Init code start */
extern	int	 icodes;		/* Init code size */
extern	dev_t	 rootdev;		/* Root device */
extern	dev_t	 swapdev;		/* Swap device */
extern	dev_t	 pipedev;		/* Pipe device */
extern	saddr_t	 corebot;		/* Bottom of core */
extern	saddr_t	 coretop;	 	/* Top of core */
extern	daddr_t	 swapbot;		/* Bottom of swap */
extern	daddr_t	 swaptop;		/* Top of swap */
extern	paddr_t	 blockp;		/* Base of buffers */
extern	paddr_t	 clistp;		/* Base of clists */
extern	struct	 all *allkp;		/* Alloc space */
