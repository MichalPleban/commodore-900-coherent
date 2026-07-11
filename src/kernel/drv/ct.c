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
 * Coherent
 * Console terminal driver.
 */
#include <coherent.h>
#include <drvcon.h>
#include <errno.h>
#include <proc.h>
#include <stat.h>
#include <uproc.h>

/*
 * Functions for configuration.
 */
int	ctopen();
int	ctclose();
int	ctread();
int	ctwrite();
int	ctioctl();
int	nulldev();
int	nonedev();

/*
 * Configuration table.
 */
CON ctcon ={
	DFCHR,				/* Flags */
	1,				/* Major index */
	ctopen,				/* Open */
	ctclose,			/* Close */
	nulldev,			/* Block */
	ctread,				/* Read */
	ctwrite,			/* Write */
	ctioctl,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	nulldev,			/* Load */
	nulldev,			/* Unload */
};

/*
 * Open.
 */
ctopen(dev, m)
dev_t dev;
{
	register dev_t ttdev;

	if ((ttdev=SELF->p_ttdev) == NODEV) {
		u.u_error = ENXIO;
		return;
	}
	dopen(ttdev, m, DFCHR);
}

/*
 * Close.
 */
ctclose(dev)
dev_t dev;
{
	dclose(SELF->p_ttdev);
}

/*
 * Read.
 */
ctread(dev, iop)
dev_t dev;
IO *iop;
{
	dread(SELF->p_ttdev, iop);
}

/*
 * Write.
 */
ctwrite(dev, iop)
dev_t dev;
IO *iop;
{
	dwrite(SELF->p_ttdev, iop);
}

/*
 * Ioctl.
 */
ctioctl(dev, com, vec)
dev_t dev;
struct sgttyb *vec;
{
	dioctl(SELF->p_ttdev, com, vec);
}
