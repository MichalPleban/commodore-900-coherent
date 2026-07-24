/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#define	SEG0	((uint *)0x3a000000L)
#define	SEG1	((uint *)0x3b000000L)
#define	XMAX	1024
#define	YMAX	800
#define	YSPLIT	512
#define	BPERSL	(XMAX / bPERB)
#define	WPERSL	(XMAX / bPERW)
