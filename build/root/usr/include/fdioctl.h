/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Floppy disk ioctl commands (wd driver).
 *
 * Recreation of the historical <fdioctl.h>, which is absent from the
 * recovered sources; the FDFORMAT value is taken from the shipped
 * /etc/fdformat binary (see disasm/fdformat.asm).
 */
#ifndef	FDIOCTL_H
#define	FDIOCTL_H

#define	FDFORMAT	0x181		/* format the entire floppy */

#endif
