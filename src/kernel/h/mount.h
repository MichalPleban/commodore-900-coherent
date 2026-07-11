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
 * Mount table.
 */
#ifndef	 MOUNT_H
#define	 MOUNT_H
#include <types.h>
#include <filsys.h>

/*
 * Mount table structure.
 */
typedef struct mount {
	struct	 mount *m_next;		/* Pointer to next */
	struct	 inode *m_ip;		/* Associated inode */
	dev_t	 m_dev;			/* Device */
	int	 m_flag;		/* Flags */
	GATE	 m_ilock;		/* Inode lock */
	GATE	 m_flock;		/* Free list lock */
	struct	 filsys m_super;	/* Super block */
} MOUNT;

/*
 * Flags.
 */
#define	MFRON	001			/* Read only file system */

#ifdef KERNEL
/*
 * Functions.
 */
MOUNT	*fsmount();			/* fs2.c */
MOUNT	*getment();			/* fs2.c */

#endif

#ifdef KERNEL
/*
 * Global variables.
 */
extern	MOUNT	*mountp;		/* Mount table */

#endif

#endif
