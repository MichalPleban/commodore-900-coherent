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
 * Disk free block.
 */
#ifndef	 FBLK_H
#define	 FBLK_H
#include <types.h>

/*
 * Number of free blocks in free list.
 */
#define NICFREE	64

/*
 * Free list block structure.
 */
struct fblk {
	short	 df_nfree;		/* Number of free blocks */
	daddr_t	 df_free[NICFREE];	/* Free blocks */
};

#endif
