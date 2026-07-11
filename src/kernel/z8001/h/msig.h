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
 * Machine dependent signals.
 */
#define	SIGEPA	12			/* Extended processor trap (uni) */
#define	SIGPRV	13			/* Privileged instruction */
#define	SIGNVI	14			/* Non vectored interrupt */
#define	SIGNMI	15			/* Non-maskable interrupt (not passed) */
#define	SIGI16	16			/* Signal 16 */
#define	NSIG	16			/* Number of signals */

/*
 * Special arguments to signal.
 */
#define	SIG_DFL	((int(*)())0)		/* Default */
#define	SIG_IGN	((int(*)())1)		/* Ignore */
