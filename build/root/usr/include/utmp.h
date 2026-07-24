/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Structure of the login records in `/etc/utmp'
 * as well as the cummulative records in
 * `/usr/adm/wtmp'.
 */

#ifndef DIRSIZ
#define	DIRSIZ	14
#endif
#ifndef	TYPES_H
#include <types.h>
#endif

struct	utmp {
	char	ut_line[8];		/* tty name */
	char	ut_name[DIRSIZ];	/* User name */
	time_t	ut_time;		/* time signed on */
};
