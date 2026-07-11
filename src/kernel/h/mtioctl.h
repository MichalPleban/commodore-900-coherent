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
 * Magnetic tape ioctl commands.
 */
#ifndef	 MTIOCTL_H
#define	 MTIOCTL_H

#define MTREWIND 0			/* Rewind */
#define	MTWEOF	 1			/* Write end of file mark */
#define MTRSKIP	 2			/* Record skip */
#define MTFSKIP	 3			/* File skip */
#define MTDEC	 4			/* DEC mode */
#define MTIBM	 5			/* IBM mode */
#define MT800	 6			/* 800 bpi */
#define MT1600	 7			/* 1600 bpi */
#define	MT6250	8			/* 6250 bpi */

#endif
