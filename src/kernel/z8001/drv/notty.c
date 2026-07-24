/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * "No-tty" console driver.
 *
 * A drop-in replacement for the keyboard/video console drivers
 * (drv/lrtty and drv/hrtty -- both `kvcon', major index 8) for
 * machines with no display.  It presents the console at the same
 * major index, but instead of driving a keyboard and video card it
 * redirects all console I/O to the FIRST serial port, i.e. altty[0]
 * of the async line driver (al.c).
 *
 * This driver owns no hardware of its own.  al.c is linked into the
 * kernel and initialises the SCC at boot (alload), so here we simply
 * forward open/close/read/write/ioctl to the al entry points with the
 * minor device forced to the first line (and its modem-control bit
 * cleared, so the console never blocks waiting for carrier).  Load the
 * `al' driver for the remaining serial ports as usual; opening the
 * console then shares the same line -- and TTY struct -- as the first
 * serial device.
 *
 * The al entry points are resolved from the kernel symbol table when
 * this loadable is linked with `ld -k' against the symboled kernel.
 */
#include <coherent.h>
#include <drvcon.h>
#include <io.h>
#include <tty.h>
#include <stat.h>

#define	CONSPORT	0		/* first serial line (altty[0]) */

/*
 * al.c entry points (kernel symbols).
 */
int	alopen();
int	alclose();
int	alread();
int	alwrite();
int	alioctl();
int	nulldev();
int	nonedev();

/*
 * Console entry points.
 */
int	ntopen();
int	ntclose();
int	ntread();
int	ntwrite();
int	ntioctl();

/*
 * Configuration table.  Named `kvcon' with major index 8 so this
 * driver is interchangeable with the lrtty/hrtty console drivers:
 * build and load exactly one of the three.
 */
CON kvcon = {
	DFCHR,				/* Flags */
	8,				/* Major index */
	ntopen,				/* Open */
	ntclose,			/* Close */
	nonedev,			/* Block */
	ntread,				/* Read */
	ntwrite,			/* Write */
	ntioctl,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	nulldev,			/* Load */
	nulldev,			/* Unload */
};

/*
 * Force the request onto the first serial line, discarding whatever
 * minor (and modem-control bit) the console node carried, and hand
 * off to the async line driver.
 */
ntopen(dev, m)
dev_t dev;
int m;
{
	alopen(makedev(major(dev), CONSPORT), m);
}

ntclose(dev, m)
dev_t dev;
int m;
{
	alclose(makedev(major(dev), CONSPORT), m);
}

ntread(dev, iop)
dev_t dev;
IO *iop;
{
	alread(makedev(major(dev), CONSPORT), iop);
}

ntwrite(dev, iop)
dev_t dev;
IO *iop;
{
	alwrite(makedev(major(dev), CONSPORT), iop);
}

ntioctl(dev, com, vec)
dev_t dev;
int com;
struct sgttyb *vec;
{
	alioctl(makedev(major(dev), CONSPORT), com, vec);
}
