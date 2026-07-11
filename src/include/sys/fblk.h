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
