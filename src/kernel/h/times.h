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
