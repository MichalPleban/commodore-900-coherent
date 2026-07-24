/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * I/O template.
 */
#ifndef	 IO_H
#define	 IO_H
#include <types.h>

/*
 * Structure used to store parameters for I/O.
 */
typedef struct io {
	int	 io_seg;		/* Space */
	unsigned io_ioc;		/* Count */
	size_t	 io_seek;		/* Seek posiion */
	char	 *io_base;		/* Virtual base */
	paddr_t	 io_phys;		/* Physical base */
} IO;

/*
 * Types of space I/O operaion is being performed from.
 */
#define IOSYS	0			/* System */
#define IOUSR	1			/* User */
#define IOPHY	2			/* Physical */

#endif
