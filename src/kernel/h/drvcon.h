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
 * Device driver configuration.
 */
#ifndef	 DRVCON_H
#define	 DRVCON_H
#include <types.h>

/*
 * Device driver table.
 */
typedef struct drv {
	struct	 con *d_conp;		/* Pointer to configuration */
	struct	 seg *d_segp;		/* Segmentation containing driver */
	dmap_t	 d_map;			/* Segmentation map */
	int	 d_time;		/* Timeout is active */
	GATE	 d_gate;		/* Gate for loading */
} DRV;

/*
 * Driver interface entry.
 */
typedef struct con {
	int	c_flag;			/* Flags */
	int	c_mind;			/* Major index */
	int	(*c_open)();		/* Open */
	int	(*c_close)();		/* Close */
	int	(*c_block)();		/* Block */
	int	(*c_read)();		/* Read */
	int	(*c_write)();		/* Write */
	int	(*c_ioctl)();		/* Ioctl */
	int	(*c_power)();		/* Powerfail */
	int	(*c_timer)();		/* Timeout */
	int	(*c_load)();		/* Load */
	int	(*c_uload)();		/* Unload */
} CON;

/*
 * Flags.
 */
#define	DFBLK	0000001			/* Block device */
#define	DFCHR	0000002			/* Character device */
#define DFTAP	0000004			/* Tape */
#define	DFERR	0100000			/* Error */

#ifdef KERNEL
/*
 * Functions.
 */
extern	CON	*drvmap();		/* bio.c */

#endif

#ifdef KERNEL
/*
 * Global variables.
 */
extern	int	drvn;			/* Number of entries in table */
extern	DRV	drvl[];			/* Driver table */

#endif

#endif
