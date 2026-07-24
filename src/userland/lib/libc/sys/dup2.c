/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Make a duplicate of `ofd' on
 * file descriptor `nfd', closing
 * `nfd' if necessary.
 */

dup2(ofd, nfd)
int ofd, nfd;
{
	return (dup(ofd|0100, nfd));
}
