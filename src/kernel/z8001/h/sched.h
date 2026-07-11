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
 * Commodore scheduling parameters.
 * A tick is currently 1/100th of a second.
 * These values have not been particularly
 * well tuned.
 */
#ifndef	 SCHED_H
#define	 SCHED_H

/*
 * Update parameters.  All values are in ticks.  The processor value
 * update interval is always 1.
 */
#define	NCRTICK	10			/* Processor time slice */
#define	NSUTICK	20			/* Swap value update interval */
#define	NSRTICK	50			/* Swap run update interval */

/*
 * Values.
 */
#define CVNOSIG	256			/* Lower priorities can interrupt */
#define CVCLOCK	1			/* Core value update */
#define	SVCLOCK	16			/* Swap value update */
#define CVCHILD	32767			/* Initial child core value */
#define IVCHILD	16			/* Importance */
#define SVCHILD	4096			/* Initial child swap value */
#define RVCHILD	0			/* Response value */

/*
 * Swapper.
 */
#define	CVSWAP	256
#define	IVSWAP	0
#define	SVSWAP	0

/*
 * Waiting for block I/O to complete.
 */
#define CVBLKIO	32767
#define IVBLKIO	32767
#define	SVBLKIO	0

/*
 * Waiting for a gate to open.
 */
#define	CVGATE	16384
#define IVGATE	3
#define SVGATE	0

/*
 * Terminal output.
 */
#define	CVTTOUT	256
#define	IVTTOUT	0
#define SVTTOUT	0

/*
 * Waiting for free clists.
 */
#define CVCLIST	256
#define IVCLIST	0
#define SVCLIST	0

/*
 * Process trace.
 */
#define CVPTSET	256
#define IVPTSET	0
#define SVPTSET	0

/*
 * Process trace stop.
 */
#define CVPTRET	256
#define IVPTRET	0
#define SVPTRET	0

/*
 * Waiting for a pipe.
 */
#define CVPIPE	0
#define IVPIPE	0
#define SVPIPE	0

/*
 * Terminal input.
 */
#define CVTTIN	255
#define IVTTIN	1
#define SVTTIN	32767

/*
 * Pause.
 */
#define CVPAUSE	0
#define IVPAUSE	-64
#define SVPAUSE	0

/*
 * Wait.
 */
#define CVWAIT	128
#define IVWAIT	-128
#define SVWAIT	4096

#endif
