/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent I/O Library
 * Return 1 if file descriptor
 * is that of a terminal.
 */

#include <sgtty.h>

isatty(fd)
{
	struct sgttyb sgb;

	return (ioctl(fd, TIOCGETP, &sgb) >= 0);
}
