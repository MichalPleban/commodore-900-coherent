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
 * Device table.
 */
DRV drvl[16] ={
	{nlcon},	{ctcon},	{wdcon},	{lpcon},
	{NULL},		{alcon},	{NULL},		{NULL},		/* slot 6 was DTC hdcon (not in production) */
	{NULL},		{NULL},		{NULL},		{NULL},
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
