/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Directory entry.
 */
#ifndef	 DIR_H
#define	 DIR_H
#include <types.h>

/*
 * Size of directory name.
 */
#define DIRSIZ	14

/*
 * Directory entry structure.
 */
struct direct {
	ino_t	 d_ino;			/* Inode number */
	char	 d_name[DIRSIZ];	/* Name */
};

#endif
