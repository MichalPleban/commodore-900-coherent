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
 * Null and memory driver.
 *  Minor device 0 is /dev/null
 *  Minor device 1 is physical memory
 *  Minor device 2 is kernel data
 */
#include <coherent.h>
#include <drvcon.h>
#include <errno.h>
#include <stat.h>
#include <uproc.h>

/*
 * Functions for configuration.
 */
int	nlread();
int	nlwrite();
int	nulldev();
int	nonedev();

/*
 * Configuration table.
 */
CON nlcon ={
	DFCHR,				/* Flags */
	0,				/* Major index */
	nulldev,			/* Open */
	nulldev,			/* Close */
	nulldev,			/* Block */
	nlread,				/* Read */
	nlwrite,			/* Write */
	nonedev,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	nulldev,			/* Load */
	nulldev,			/* Unload */
};

/*
 * Null/memory read routine.
 */
nlread(dev, iop)
dev_t dev;
register IO *iop;
{
	register unsigned n;

	switch (minor(dev)) {
	case 0:
		n = 0;
		break;

	case 1:
		n = pucopy((long)iop->io_seek, iop->io_base, iop->io_ioc);
		break;

	case 2:
		n = kucopy(kdaddr(iop->io_seek), iop->io_base, iop->io_ioc);
		break;

	default:
		u.u_error = ENXIO;
		return;
	}
	iop->io_ioc -= n;
	if (u.u_error == EFAULT)
		u.u_error = 0;
}

/*
 * Null/memory write routine.
 */
nlwrite(dev, iop)
dev_t dev;
register IO *iop;
{
	register unsigned n;

	switch (minor(dev)) {
	case 0:
		n = iop->io_ioc;
		break;

	case 1:
		n = upcopy(iop->io_base, (long)iop->io_seek, iop->io_ioc);
		break;

	case 2:
		n = ukcopy(iop->io_base, kdaddr(iop->io_seek), iop->io_ioc);
		break;

	default:
		u.u_error = ENXIO;
		return;
	}
	iop->io_ioc -= n;
	if (u.u_error == EFAULT)
		u.u_error = 0;
}
