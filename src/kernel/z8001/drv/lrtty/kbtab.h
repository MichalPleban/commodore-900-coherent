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
 * Commodore 900 Keyboard driver.
 * This header file is used only by the
 * driver and defines the interface between
 * the keyboard driver and the table for
 * the particular keyboard.  This method enables
 * different keyboards to be supported by the same
 * source code.
 */

/*
 * Flags in the keyboard table.
 * KLOCK is needed only by keyboard in which the
 * keys don't look (i.e. send an up and down transition for
 * the shifting action).
 */
#define	KDUP	0x01		/* Duplicate key (used for `00') */
#define	KCAP	0x02		/* Use upper entry in CAPS LOCK mode */
#define	KP	0x04		/* This key is affected by keypad mode flag */
#define	KLOCK	0x08		/* Locking shift (done in software) */
#define	KINV	0x10		/* Disabled key */
#define	KC	0x20		/* Control key allowed */
#define	KSHIFT	0x40		/* A shift key (e.g. cntrl, ALT, SHIFT */
#define	KNL	0x80		/* Num lock - use upper entry */

/* Bits in k_control when KSHIFT is in effect */
#define	SS1	0x01		/* Shift key #1 (left) */
#define	SS2	0x02		/* Shift key #2 (right) */
#define	SNL	0x04		/* Num lock key */
#define	SCL	0x08		/* Caps lock key */
#define	SCT	0x10		/* Control key */
#define	SAL	0x20		/* ALT (or meta) key */

/* Character codes that are used from ascii */
#define	CESC	0033
#define	CDEL	0177
#define	ct(c)	((c)&~0140)

/* Extended character codes */
#include <kbchar.h>

/*
 * This structure is indexed by scan code.
 * It contains all information needed by the
 * keyboard driver state machine (see kbtab.h
 * for definitions).
 */
typedef	struct	KEY		{
	unsigned	char	k_flag;
	unsigned	char	k_lower;
	unsigned	char	k_upper;
	unsigned	char	k_control;
}	KEY;

extern	KEY	ktab[];
