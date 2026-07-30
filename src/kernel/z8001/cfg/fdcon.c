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
extern	CON	lpcon[];		/* line printer */

/*
 * Device table.  Slot 9 (pseudo-terminals) is filled at run time by
 * `load /drv/pty' - the pty driver is a loadable to keep it out of the
 * 64K resident code segment; both pty sides share its one major (the minor
 * selects master/slave), so slot 10 is free.
 */
DRV drvl[16] ={
	{nlcon},	{ctcon},	{wdcon},	{lpcon},
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
