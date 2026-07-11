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
 * Typewriter structure.
 */
#ifndef	 TTY_H
#define	 TTY_H
#include <types.h>
#include <clist.h>
#include <sgtty.h>

#define	NCIB	128		/* Input buffer */
#define	OHILIM	128		/* Output buffer hi water mark */
#define	OLOLIM	40		/* Output buffer lo water mark */
#define	IHILIM	512		/* Input buffer hi water mark */
#define	ILOLIM	40		/* Input buffer lo water mark */
#define	ITSLIM	(IHILIM-(IHILIM/4))	/* Input buffer tandem stop mark */
#define	ESC	'\\'		/* Some characters */
#define	CTRLD	04
#define	CTRLC	03
#define	CTRLU	025
#define	FS	034
#define	CTRLS	023
#define	CTRLQ	021
#define	DEL	0177

typedef struct tty {
	CQUEUE	t_oq;		/* Output queue */
	CQUEUE	t_iq;		/* Input queue */
	char	*t_ddp;		/* Device specific */
	int	(*t_start)();	/* Start function */
	int	(*t_param)();	/* Load parameters function */
	char	t_dispeed;	/* Default input speed */
	char	t_dospeed;	/* Default output speed */
	int	t_open;		/* Open count */
	int	t_flags;	/* Flags */
	char	t_nfill;	/* Number of fill characters */
	char	t_fillb;	/* The fill character */
	int	t_ibx;		/* Input buffer index */
	char	t_ib[NCIB];	/* Input buffer */
	int	t_hpos;		/* Horizontal position */
	int	t_opos;		/* Original horizontal position */
	struct	sgttyb t_sgttyb;/* Stty/gtty information */
	struct	tchars t_tchars;/* Tchars information */
	int	t_group;	/* Process group */
	int	t_escape;	/* Pending escape count */
} TTY;

/*
 * Flags.
 */
#define	T_HILIM	01		/* Hi water mark wait */
#define	T_DRAIN	02		/* Drain wait */
#define	T_INPUT	04		/* Input wait */
#define	T_INL	020		/* Insert newline */
#define	T_STOP	040		/* Stopped */
#define	T_HPCL	0100		/* Hang up dataset on last close */
#define	T_EXCL	0200		/* Exclusive use */
#define	T_TSTOP	0400		/* Tandem input stop */
#define	T_ISTOP	01000		/* Input overflow stop */
#define	T_USER0	010000		/* User bit 0 */
#define	T_USER1	020000		/* User bit 1 */
#define	T_USER2	040000		/* User bit 2 */
#define	T_USER3	0100000		/* User bit 3 */

/*
 * Test macros.
 * Assume `tp' holds a TTY pointer.
 *	  `c'  a character.
 * Be very careful if you work on the
 * tty driver that this is true.
 */
#define	ISINTR	(tp->t_tchars.t_intrc  == c)
#define	ISQUIT	(tp->t_tchars.t_quitc  == c)
#define	ISEOF	(tp->t_tchars.t_eofc   == c)
#define	ISBRK	(tp->t_tchars.t_brkc   == c)
#define	ISSTART	(tp->t_tchars.t_startc == c)
#define	ISSTOP	(tp->t_tchars.t_stopc  == c)
#define	ISCRMOD	((tp->t_sgttyb.sg_flags&CRMOD) != 0)
#define	ISXTABS	((tp->t_sgttyb.sg_flags&XTABS) != 0)
#define	ISRIN	((tp->t_sgttyb.sg_flags&RAWIN) != 0)
#define	ISROUT	((tp->t_sgttyb.sg_flags&RAWOUT)!= 0)
#define	ISECHO	((tp->t_sgttyb.sg_flags&ECHO)  != 0)
#define	ISCRT	((tp->t_sgttyb.sg_flags&CRT)   != 0)
#define	ISCBRK	((tp->t_sgttyb.sg_flags&CBREAK)!= 0)
#define	ISTAND	((tp->t_sgttyb.sg_flags&TANDEM)!= 0)
#define	ISBBYB	((tp->t_sgttyb.sg_flags&(RAWIN|CBREAK)) != 0)
#define	ISERASE	(tp->t_sgttyb.sg_erase == c)
#define	ISKILL	(tp->t_sgttyb.sg_kill  == c)
#endif
