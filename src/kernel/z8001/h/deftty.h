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
 * Coherent - default tty settings.
 *	used by sys/drv/tty.c, src/cmd/getty.c, and src/cmd/login.c
 *	to initialize terminal characteristics.
 */
#ifndef DEFTTY_H
#define DEFTTY_H
#include <chars.h>

#define DEF_SG_ISPEED	B9600
#define DEF_SG_OSPEED	B9600
#define	DEF_SG_ERASE	BS
#define	DEF_SG_KILL	'@'
#define	DEF_SG_FLAGS	EVENP|ODDP|CRMOD|ECHO|XTABS|CRT
#define	DEF_T_INTRC	CTRLC
#define	DEF_T_QUITC	FS
#define	DEF_T_STARTC	CTRLQ
#define	DEF_T_STOPC	CTRLS
#define	DEF_T_EOFC	CTRLD
#define	DEF_T_BRKC	-1
#endif
