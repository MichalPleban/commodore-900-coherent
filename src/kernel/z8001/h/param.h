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
 * Configurable parameters.
 * Adjusting NINODE, NCLIST, NBUF, and ALLSIZE
 * all may cause us to run out of memory.
 * Be careful!
 */
#define NDRV	20			/* Number of major device entries */
#define NBUF	40			/* Size of buffer cache */
#define	NCLIST	32			/* Number of clists (NCPCL/256 per) */
#define NUFILE	20			/* Number of user open files */
#define NINODE	100			/* Size of in core inode table */
#define	ALLSIZE	10240			/* Size of alloc space */
#define NEXREAD	4			/* Read ahead */
