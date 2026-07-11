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
 * Accounting structure.
 */
#ifndef	 ACCT_H
#define	 ACCT_H
#include <types.h>

struct acct {
	char	ac_comm[10];		/* Command name */
	comp_t	ac_utime;		/* User time */
	comp_t	ac_stime;		/* System time */
	comp_t	ac_etime;		/* Elapsed time */
	time_t	ac_btime;		/* Beginning time of process */
	short	ac_uid;			/* User id */
	short	ac_gid;			/* Group id */
	short	ac_mem;			/* Average memory usage */
	comp_t	ac_io;			/* Number of disk I/O blocks */
	dev_t	ac_tty;			/* Control typewriter */
	char	ac_flag;		/* Accounting flag */
};

/*
 * Flags (ac_flag).
 */
#define AFORK	01			/* Execute fork, but not exec */
#define ASU	02			/* Used super user privileges */

#endif
