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
 * Machine dependent types.
 */
#ifndef TYPES_H
#define TYPES_H	TYPES_H

/*
 * Mapping types.
 */
typedef	unsigned amap_t;		/* Auxiliary map */
typedef	unsigned aold_t;		/* Auxiliary map save */
typedef unsigned bmap_t;		/* Buffer map */
typedef unsigned bold_t;		/* Buffer map save */
typedef unsigned cold_t;		/* Clist map save */
typedef	char	*cmap_t;		/* Clist map */
typedef unsigned dmap_t;		/* Driver map */
typedef unsigned dold_t;		/* Driver map save */

/*
 * System types.
 */
typedef	unsigned comp_t;		/* Accounting */
typedef	long	 daddr_t;		/* Disk address */
typedef unsigned dev_t;			/* Device */
typedef unsigned ino_t;			/* Inode number */
typedef long	 paddr_t;		/* Physical memory address */
typedef unsigned saddr_t;		/* Segmenation address */
typedef unsigned sig_t;			/* Signal bits */
typedef long	 size_t;		/* Lengths */
typedef	long	 time_t;		/* Time */
typedef	unsigned long vaddr_t;		/* Virtual memory address */
typedef	char	 GATE[2];		/* Gate structure */

#endif
