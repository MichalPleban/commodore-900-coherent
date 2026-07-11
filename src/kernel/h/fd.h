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
