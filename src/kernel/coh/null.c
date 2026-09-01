/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Null and memory driver.
 *  Minor device 0 is /dev/null
 *  Minor device 1 is physical memory
 *  Minor device 2 is kernel data
 *  Minor device 3 is /dev/zero
 *  Minor device 4 is the byte-wide I/O port space
 *  Minor device 5 is the word-wide I/O port space
 *
 * The port devices give a process the Z8001 `in'/`out' instructions:
 * the seek position is the 16 bit port address, and each transferred
 * datum steps it on by one port (minor 4) or two (minor 5), so that a
 * plain read(2) walks a run of registers.  A word port is presented in
 * the machine's own big endian order, and its address must be even.
 * This is the normal I/O space only; the special I/O space (the MMU)
 * is deliberately not reachable from a process.
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
int	iogetc();
int	ioputc();
int	in();
int	inb();
int	out();
int	outb();

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
	register unsigned port;
	unsigned w;

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

	case 3:
		while (ioputc(0, iop) >= 0)
			;
		n = 0;		/* ioputc consumed io_ioc itself */
		break;

	case 4:
		if (iop->io_seek < 0 || iop->io_seek > 0xFFFFL) {
			u.u_error = EINVAL;
			return;
		}
		port = iop->io_seek;
		while (iop->io_ioc != 0) {
			if (ioputc(inb(port), iop) < 0)
				break;
			++port;
		}
		n = 0;		/* ioputc consumed io_ioc itself */
		break;

	case 5:
		if (iop->io_seek < 0 || iop->io_seek > 0xFFFFL ||
		    (iop->io_seek & 1) != 0) {
			u.u_error = EINVAL;
			return;
		}
		port = iop->io_seek;
		while (iop->io_ioc >= 2) {
			w = in(port);
			if (ioputc(w>>8, iop) < 0 || ioputc(w, iop) < 0)
				break;
			port += 2;
		}
		n = 0;		/* ioputc consumed io_ioc itself */
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
	register unsigned port;
	int c, l;

	switch (minor(dev)) {
	case 0:
	case 3:
		n = iop->io_ioc;
		break;

	case 1:
		n = upcopy(iop->io_base, (long)iop->io_seek, iop->io_ioc);
		break;

	case 2:
		n = ukcopy(iop->io_base, kdaddr(iop->io_seek), iop->io_ioc);
		break;

	case 4:
		if (iop->io_seek < 0 || iop->io_seek > 0xFFFFL) {
			u.u_error = EINVAL;
			return;
		}
		port = iop->io_seek;
		while ((c = iogetc(iop)) >= 0)
			outb(port++, c);
		n = 0;		/* iogetc consumed io_ioc itself */
		break;

	case 5:
		if (iop->io_seek < 0 || iop->io_seek > 0xFFFFL ||
		    (iop->io_seek & 1) != 0) {
			u.u_error = EINVAL;
			return;
		}
		port = iop->io_seek;
		while (iop->io_ioc >= 2) {
			if ((c = iogetc(iop)) < 0 || (l = iogetc(iop)) < 0)
				break;
			out(port, (c<<8) | l);
			port += 2;
		}
		n = 0;		/* iogetc consumed io_ioc itself */
		break;

	default:
		u.u_error = ENXIO;
		return;
	}
	iop->io_ioc -= n;
	if (u.u_error == EFAULT)
		u.u_error = 0;
}
