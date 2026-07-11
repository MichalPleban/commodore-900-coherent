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
 * Super block.
 */
#ifndef	 FILSYS_H
#define  FILSYS_H
#include <types.h>
#include <fblk.h>

/*
 * Size definitions.
 */
#define NICINOD	100			/* Number of free in core inodes */
#define BSIZE	512			/* Block size */
#define INOPB	8			/* Number of inodes per block */
#define BOOTBI	0			/* Boot block index */
#define SUPERI	1			/* Super block index */
#define INODEI	2			/* Inode block index */
#define BADFIN	1			/* Bad block inode number */
#define ROOTIN	2			/* Root inode number */

/*
 * Super block.
 */
struct filsys {
	unsigned short s_isize;		/* Firt block not in inode list */
	daddr_t	 s_fsize;		/* Size of entire volume */
	short	 s_nfree;		/* Number of addresses in s_free */
	daddr_t	 s_free[NICFREE];	/* Free block list */
	short	 s_ninode;		/* Number of inodes in s_inode */
	ino_t	 s_inode[NICINOD];	/* Free inode list */
	char	 s_flock;		/* Not used */
	char	 s_ilock;		/* Not used */
	char	 s_fmod;		/* Super block modified flag */
	char	 s_ronly;		/* Mounted read only flag */
	time_t	 s_time;		/* Last super block update */
	daddr_t	 s_tfree;		/* Total free blocks */
	ino_t	 s_tinode;		/* Total free inodes */
	short	 s_m;			/* Interleave factor */
	short	 s_n;			/* Interleave factor */
	char	 s_fname[6];		/* File system name */
	char	 s_fpack[6];		/* File system pack name */
	long	 s_unique;		/* Unique number */
};

/*
 * Functions.
 */
#define iblockn(ino)	(INODEI+(ino-1)/INOPB)
#define iblocko(ino)	((ino-1)%INOPB)

#endif
