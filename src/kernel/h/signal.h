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
 * Signals.
 */
#ifndef	 SIG_H
#define	 SIG_H
#include <msig.h>

#define SIGHUP	1			/* Hangup */
#define	SIGINT	2			/* Interrupt */
#define SIGQUIT	3			/* Quit */
#define SIGALRM	4			/* Alarm */
#define SIGTERM	5			/* Software termination signal */
#define SIGREST	6			/* Restart */
#define SIGSYS	7			/* Bad argument to system call */
#define	SIGPIPE	8			/* Write to pipe with no readers */
#define SIGKILL	9			/* Kill */
#define SIGTRAP	10			/* Breakpoint */
#define	SIGSEGV	11			/* Segmentation violation */

#ifndef	KERNEL
/*
 * For the benefit of user programmes.
 */
int	(*signal())();

#endif

#endif
