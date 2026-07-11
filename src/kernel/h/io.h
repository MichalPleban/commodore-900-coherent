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
