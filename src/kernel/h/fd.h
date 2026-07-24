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
	char	 f_refc;		/* Reference count */
	size_t	 f_seek;		/* Seek pointer */
	struct	 inode *f_ip;		/* Pointer to inode */
} FD;

#ifdef	KERNEL
/*
 * Functions.
 */
extern	FD	*fdget();		/* fd.c */

#endif

#endif
