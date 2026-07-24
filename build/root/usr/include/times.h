/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Structure returned by the `times' system call.
 */
#ifndef	 TIMES_H
#define	 TIMES_H

struct tbuffer {
	long	 tb_utime;		/* Process user time */
	long	 tb_stime;		/* Process system time */
	long	 tb_cutime;		/* Child user time */
	long	 tb_cstime;		/* Child system time */
};

#endif
