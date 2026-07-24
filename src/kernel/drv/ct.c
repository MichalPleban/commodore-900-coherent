/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
