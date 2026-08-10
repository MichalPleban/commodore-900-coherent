/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series Z8001
 * Configuration file for floppy disk root
 * Hard disks are Western Digital and sasi controller based.
 */
#include <coherent.h>
#include <drvcon.h>
#include <mtype.h>
#include <stat.h>

extern	CON	nlcon[];		/* Null device */
extern	CON	ctcon[];		/* Console terminal */
extern	CON	alcon[];		/* Asynchronous line */
extern	CON	wdcon[];		/* Western Digital hard disk */

/*
 * Device table.  Slots 3 and 9 are filled at run time by `/etc/load':
 * slot 3 (line printer) is loaded by init from the icode argv (/drv/lp),
 * slot 9 (pseudo-terminals) at GUI start-up (/drv/pty) - both are
 * loadables to keep them out of the 64K resident code segment.
 */
DRV drvl[16] ={
	{nlcon},	{ctcon},	{wdcon},	{NULL},		/* slot 3 = /drv/lp (loadable) */
	{NULL},		{alcon},	{NULL},		{NULL},		/* slot 6 was DTC hdcon (not in production) */
	{NULL},		{NULL},		{NULL},		{NULL},		/* slot 9 = /drv/pty (loadable) */
	{NULL},		{NULL},		{NULL},		{NULL},
};

/*
 * Time.
 */
TIME timer ={
	0,				/* Initial time */
	0,				/* Ticks */
#ifdef EST
	5*60,				/* Eastern */
#else
	6*60,				/* Central */
#endif
	1,				/* Daylight saving time */
};

/*
 * Devices and sizes.
 */
dev_t	rootdev = makedev(2, 48);	/* Root device = WD/SASI floppy fd1 (drive 3) */
dev_t	pipedev = makedev(2, 48);	/* Pipe device */
dev_t	swapdev = makedev(2, 3);	/* Swap device = WD hard disk hd3 (same as wdcon) */
daddr_t	swapbot = 7001;			/* Swap base */
daddr_t	swaptop = 10335;		/* Swap end */
int	ronflag	= 0;			/* Not read only */
int	drvn	= 16;			/* Maximum number of devices */
int	mactype	= M_Z8001;		/* Machine type */
