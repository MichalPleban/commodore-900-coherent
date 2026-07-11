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
 * TTY structure, public portion.
 */
#ifndef TTY_H
#define TTY_H

#define	T_HILIM	01		/* Hi water mark wait */
#define	T_DRAIN	02		/* Drain wait */
#define	T_INPUT	04		/* Input wait */
#define T_IFULL 010		/* Input buffer full */
#define	T_INL	020		/* Insert newline */
#define	T_STOP	040		/* Stopped */
#define	T_HPCL	0100		/* Hang up dataset on last close */
#define	T_EXCL	0200		/* Exclusive use */
#define	T_TSTOP	0400		/* Tandem input stop */
#define	T_ISTOP	01000		/* Input overflow stop */
#define T_MODC  02000		/* Modem control */
#define T_CARR	04000		/* Carrier detect status */
#define	T_BRD	010000		/* Blocking read in CBREAK/RAW mode */
#define	T_HOPEN	020000		/* Hanging in open (for modem control) */
#define	T_UN1	040000		/* Unused bit 1 */
#define	T_UN0	0100000		/* Unused bit 0 */

/* don't reset these flags when flushing the input and output queues */
#define T_SAVE	 (T_HPCL|T_EXCL|T_MODC|T_CARR|T_HOPEN|T_BRD)

#define NMODC	0x80		/* Minor device modem control bit */
				/* Set for NO modem control       */

#ifdef KERNEL
#include <ktty.h>
#endif
#endif
