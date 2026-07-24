/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
 * Flags.  Only MFRON is stored persistently in m_flag; MFFORCE and MFRMT
 * are request bits passed to the mount() system call and are not retained.
 */
#define	MFRON	001			/* Read only file system */
#define	MFFORCE	002			/* Mount r/w even if dirty (mount -f) */
#define	MFRMT	004			/* Remount an already-mounted fs (mount -w) */

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
