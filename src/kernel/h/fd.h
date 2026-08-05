/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Open file descriptor.
 */
#ifndef	 FD_H
#define	 FD_H
#include <types.h>
#include <inode.h>

/*
 * File descriptor structure.
 */
typedef struct fd {
	char	 f_flag;		/* Flags */
	short	 f_refc;		/* Reference count */
	size_t	 f_seek;		/* Seek pointer */
	struct	 inode *f_ip;		/* Pointer to inode */
} FD;

/*
 * Flags (f_flag).  The low bits hold the IPR/IPW permission bits
 * from <inode.h>, so private flags start above them.
 */
#define	FFOPNP	0010			/* Open has not completed yet */

#ifdef	KERNEL
/*
 * Functions.
 */
extern	FD	*fdget();		/* fd.c */

#endif

#endif
