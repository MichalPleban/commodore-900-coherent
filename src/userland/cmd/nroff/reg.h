/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Registers.
 */

/*
 * Types of data in macro.
 */
#define MTEXT	0			/* Text */
#define MDIVN	1			/* Diversion */
#define MREQS	2			/* Request */

/*
 * Data descriptor in macro.
 */
typedef	union	mac	{
	struct	{			/* Text and diversions */
		union mac *m_next;	/* Pointer to next */
		int	m_type;		/* Type of data */
		int	m_size;		/* Count */
		char	*m_core;	/* Pointer in core */
		long	m_seek;		/* Seek pointer */
	};
	struct	{			/* Requests */
		union mac *m_next;	/* Pointer to next */
		int	m_type;		/* Type of data */
		int	m_size;		/* Count */
		int	(*m_func)();	/* Function for request */
	};
} MAC;

/*
 * Types of registers.
 */
#define RTEXT	0			/* Text register */
#define RNUMR	1			/* Number register */

/*
 * Register structure.
 */
typedef	union	reg	{
	struct	{			/* Text register */
		union reg *r_next;	/* Pointer to next register */
		int	r_type;		/* Register type */
		char	r_name[2];	/* Register name */
		int	r_maxh;		/* Maximum height in diversion */
		int	r_maxw;		/* Maximum width in diversion */
		MAC	r_macd;		/* Macro descriptor */
	};
	struct	{			/* Number register */
		union reg *r_next;	/* Pointer to next register */
		int	r_type;		/* Register type */
		char	r_name[2];	/* Register name */
		int	r_nval;		/* Value of register */
		int	r_incr;		/* Increment */
		char	r_form;		/* Format character */
	};
} REG;

/*
 * Hashing function for registers.
 */
#define hash(name)	((name[0]+name[1])%RHTSIZE)

/*
 * Global variables.
 */
extern	REG	*nrpnreg;		/* Current page number */
extern	REG	*nrctreg;		/* Character type */
extern	REG	*nrdlreg;		/* Max width of last diversion */
extern	REG	*nrdnreg;		/* Height of last diversion */
extern	REG	*nrdwreg;		/* Day of the week */
extern	REG	*nrdyreg;		/* Day of the month */
extern	REG	*nrhpreg;		/* Current horizontal place */
extern	REG	*nrlnreg;		/* Output line number */
extern	REG	*nrmoreg;		/* Month */
extern	REG	*nrnlreg;		/* Position of last printed line */
extern	REG	*nrsbreg;		/* Depth of string below base */
extern	REG	*nrstreg;		/* Height of string above base */
extern	REG	*nryrreg;		/* Year */
extern	REG	*regt[RHTSIZE];		/* Register hash table */
