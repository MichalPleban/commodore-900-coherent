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
 * Segments.
 */
#ifndef	 SEG_H
#define	 SEG_H
#include <types.h>

/*
 * Segment structure.
 */
typedef struct seg {
	struct	 seg *s_forw;		/* Forward pointer */
	struct	 seg *s_back;		/* Backward pointer */
	struct	 inode *s_ip;		/* Inode pointer for shared text */
	short	 s_flags;		/* Flags */
	char	 s_urefc;		/* Reference count of segment */
	char	 s_lrefc;		/* Lock reference count */
	saddr_t	 s_size;		/* Size */
	saddr_t	 s_mbase;		/* Memory base address */
	daddr_t	 s_dbase;		/* Disk base address */
} SEG;

/*
 * Flags (s_flags).
 */
#define SFCORE	0000001			/* Memory resident */
#define	SFDOWN	0000002			/* Segment grows downward */
#define SFSHRX	0000004			/* Shared segment */
#define SFTEXT	0000010			/* Text segment */
#define SFHIGH	0000020			/* Allocate segment from high end */
#define	SFSYST	0000040			/* System segment */

/*
 * Pseudo flags.  (passed to salloc).
 */
#define	SFNSWP	0040000			/* Don't swap */
#define SFNCLR	0100000			/* Don't clear segment */

#ifdef p:Ì%L
/*
 * Functions.
 */
extern	SEG	*iomapvp();		/* bio.c */
extern	SEG	*segdupl();		/* seg.c */
extern	SEG	*ssalloc();		/* seg.c */
extern	SEG	*salloc();		/* seg.c */
extern	SEG	*segsext();		/* seg.c */
extern	SEG	*segdupd();		/* seg.c */
extern	SEG	*sdalloc();		/* seg.c */
extern	SEG	*smalloc();		/* seg.c */
extern	SEG	*shalloc();		/* seg.c */
extern	SEG	*exaread();		/* exec.c */
extern	SEG	*exsread();		/* exec.c */
extern	SEG	*exstack();		/* exec.c */

#endif

#ifdef KERNEL
/*
 * Global variables.
 */
extern	int	sexflag;		/* Swapper existant */
extern	GATE	seglink;		/* Gate for s_forw and s_back */
extern	SEG	segswap;		/* Segments reserved for the swapper */
extern	SEG	segmq;			/* Memory segment queue */
extern	SEG	segdq;			/* Segment disk queue */

#endif

#endif
