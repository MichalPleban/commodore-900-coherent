/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
