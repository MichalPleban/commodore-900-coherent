/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
#define	ALLSIZE	32768			/* Size of alloc space (kalloc arena).
					 * MUST track the kernel's own param.h --
					 * this copy is the INSTALLED header, and it
					 * sat at 10240 for a kernel running 32768,
					 * which is a lie anything sizing itself from
					 * the arena would have believed. */
#define NEXREAD	4			/* Read ahead */
