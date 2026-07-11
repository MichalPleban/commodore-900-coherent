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
 * Terminal ioctl.
 */
#ifndef	 SGTTY_H
#define	 SGTTY_H
#include <types.h>

/*
 * Ioctl functions.
 */
#define	TIOCSETP	0100		/* Terminal set modes (old stty) */
#define	TIOCGETP	0101		/* Terminal get modes (old gtty) */
#define	TIOCSETC	0102		/* Set characters */
#define	TIOCGETC	0103		/* Get characters */
#define	TIOCSETN	0104		/* No delay or output flushed */
#define	TIOCEXCL	0105		/* Set exclusive use */
#define	TIOCNXCL	0106		/* Set non exclusive use */
#define	TIOCHPCL	0107		/* Hang up on last close */
#define	TIOCFLUSH	0110		/* Flush all characters in queue */

#define	TIOCQUERY	0120		/* Test if any characters */

#define	TIOCGETF	0200		/* Get function keys (dev. dep.) */
#define	TIOCSETF	0201		/* Set function keys */

/*
 * Compatibility with gtty and stty.
 */
#define	stty(u,v)	ioctl(u,TIOCSETP,v)
#define	gtty(u,v)	ioctl(u,TIOCGETP,v)

/*
 * Structure for TIOCSETP/TIOCGETP
 */
struct sgttyb {
	char	sg_ispeed;		/* Input speed */
	char	sg_ospeed;		/* Output speed */
	char	sg_erase;		/* Character erase */
	char	sg_kill;		/* Line kill character */
	int	sg_flags;		/* Flags */
};

/*
 * Structure for TIOCSETC/TIOCGETC
 */
struct tchars {
	char	t_intrc;		/* Interrupt */
	char	t_quitc;		/* Quit */
	char	t_startc;		/* Start output */
	char	t_stopc;		/* Stop output */
	char	t_eofc;			/* End of file */
	char	t_brkc;			/* Input delimiter */
};

/*
 * Overlying structure for ioctl.
 */
union ioctl {
	struct	sgttyb;
	struct	tchars;
};

/*
 * Bits from old stty/gtty modes.
 */
#define	EVENP	01		/* Allow even parity */
#define	ODDP	02		/* Allow odd parity */
#define CRMOD	04		/* Map '\r' to '\n' */
#define	ECHO	010		/* Echo input characters */
#define	LCASE	020		/* Lowercase mapping on input */
#define	CBREAK	040		/* Each input character causes wakeup */
#define	RAWIN	0100		/* 8-bit input raw */
#define	RAWOUT	0200		/* 8-bit output raw */
#define	TANDEM	0400		/* flow control protocol */
#define	XTABS	01000		/* Expand tabs to spaces */
#define	CRT	02000		/* CRT character erase */

/*
 * Compatibility.
 */
#define	RAW	(RAWIN|RAWOUT)	/* Raw mode */

/*
 * Names for terminal speeds.
 */
#define	B0	0		/* Undefined */
#define	B50	1		/* 50 bps */
#define	B75	2		/* 75 bps */
#define	B110	3		/* 110 bps */
#define	B134	4		/* 134.5 bps (IBM 2741) */
#define	B150	5		/* 150 bps */
#define	B200	6		/* 200 bps */
#define	B300	7		/* 300 bps */
#define	B600	8		/* 600 bps */
#define	B1200	9		/* 1200 bps */
#define	B1800	10		/* 1800 bps */
#define	B2000	11		/* 2000 bps */
#define	B2400	12		/* 2400 bps */
#define	B3600	13		/* 3600 bps */
#define	B4800	14		/* 4800 bps */
#define	B7200	15		/* 7200 bps */
#define	B9600	16		/* 9600 bps */
#define	B19200	17		/* 19200 bps */
#define	EXTA	18		/* External A (DH-11) */
#define	EXTB	19		/* External B (DH-11) */

#endif
