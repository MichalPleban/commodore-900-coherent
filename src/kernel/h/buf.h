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
 * Buffer header.
 */
#ifndef	 BUF_H
#define	 BUF_H
#include <types.h>

typedef struct buf {
	struct	 buf *b_actf;		/* First in queue */
	struct	 buf *b_actl;		/* Last in queue */
	GATE	 b_gate;		/* Gate */
	unsigned b_flag;		/* Flags */
	dev_t	 b_dev;			/* Device */
	daddr_t	 b_bno;			/* Block number */
	char	 b_req;			/* I/O type */
	char	 b_err;			/* Error */
	unsigned b_seqn;		/* Buffer sequence number */
	bold_t	 b_map;			/* Old map */
	vaddr_t  b_count;		/* Size of I/O */
	vaddr_t  b_resid;		/* Driver returns count here */
	char	 *b_vaddr;		/* Virtual address */
	paddr_t	 b_paddr;		/* Physical address */
} BUF;

/*
 * Flags (b_flags).
 */
#define BFNTP	0000001			/* Buffer not valid */
#define BFREQ	0000002			/* Buffer requested */
#define BFERR	0000004			/* Error */
#define BFMOD	0000010			/* Data has been modified */
#define BFASY	0000020			/* Asynchrous */
#define BFMAP	0000040			/* Buffer has been mapped */
#define BFRAW	0000100			/* Request is raw */
#define BFTAP	0000200			/* Request is to a tape */

#define BFBLK	0040000			/* Aligned on a block boundary */
#define BFIOC	0100000			/* Perform bounds checking */

/*
 * Requests.
 */
#define	BREAD	1			/* Read */
#define BWRITE	2			/* Write */

#ifdef KERNEL
/*
 * Functions.
 */
extern	BUF	 *bread();		/* bio.c */
extern	BUF	 *bclaim();		/* bio.c */
extern	BUF	 *vread();		/* fs3.c */
extern	BUF	 *aread();		/* fs3.c */
extern	daddr_t	 vmap();		/* fs3.c */
extern	int	 *lmap();		/* fs3.c */

#endif

#ifdef KERNEL
/*
 * Global variables.
 */
extern	unsigned bufseqn;		/* Buffer sequencer */
extern	int	 bufneed;		/* Buffer is needed */
extern	BUF	 swapbuf;		/* Buffer for swap I/O */
extern	BUF	 *bufl;			/* Buffer headers */

#endif

#endif
