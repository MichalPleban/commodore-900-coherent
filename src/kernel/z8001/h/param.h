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
#define	ALLSIZE	40960			/* Size of alloc space (kalloc arena: PROC/
					 * SEG structs etc.).  Was 10240 -- only
					 * ~20 processes, so the GUI hit a hard wall
					 * at ~4 terminals regardless of the ~800 KB
					 * of free process RAM.  40 KB allows ~4x the
					 * processes; the arena is a one-time slice of
					 * the 1 MB, so this is cheap. */
#define NEXREAD	4			/* Read ahead */
