/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Data structure for storing character codes and stream directives.
 */

/*
 * Stream directives.
 */
#define DNULL	0			/* Null */
#define DHMOV	-1			/* Move horizontally */
#define DVMOV	-2			/* Move vertically */
#define DTYPE	-3			/* Change type style */
#define DFONT	-4			/* Change font */
#define DPSZE	-5			/* Change pointsize */
#define DSPAR	-6			/* Space down and return */
#define DPADC	-7			/* Paddable character */
#define DHYPH	-8			/* Place to hyphenate */
#define	DHYPC	(-9)			/* Hyphen character */

/*
 * All characters and commands are stored in this structure.
 * To identify whether an element is a character or a command,
 * call the function ifcchar(element) which returns 1 if the
 * element is a character code and 0 if the element is a command.
 */
typedef	union	{
	struct	{			/* Structure containing character */
		int	c_code;		/* Character code */
		unsigned c_move;	/* Distance to move after char */
	};
	struct	{			/* Command with one long argument */
		int	c_code;		/* Type of command */
		int	c_iarg;		/* Long parameter */
	};
	struct	{			/* Commands with two short arguments */
		int	c_code;		/* Type of command */
		char	c_car1;		/* Argument 1 */
		char	c_car2;		/* Argument 2 */
	};
} CODE;

/*
 * Functions for determining whether an element is a character code
 * or a stream directive.
 */
#define ifcchar(c)	((c)>0)
#define ifcdirc(c)	((c)<0)

/*
 * Add a character given the font number and width.
 */
#define addchar(f, w) {							\
	chkcode();							\
	nlinptr->c_code = f;						\
	nlinptr++->c_move = w;						\
}

/*
 * Add a directive which takes an integer argument.
 */
#define addidir(d, i) {							\
	chkcode();							\
	nlinptr->c_code = d;						\
	nlinptr++->c_iarg = i;						\
}

/*
 * Add a directive which takes two character arguments.
 */
#define addcdir(d, c1, c2) {						\
	chkcode();							\
	nlinptr->c_code = d;						\
	nlinptr->c_car1 = c1;						\
	nlinptr++->c_car2 = c2;						\
}

/*
 * Make sure there is space to add another command.
 */
#define chkcode() {							\
	if (nlinptr >= &linebuf[LINSIZE])				\
		panic(lbomsg);						\
}

/*
 * Global variables.
 */
extern	CODE	codeval;		/* Diversion struct returned by getl */
extern	char	lbomsg[];		/* "Line buffer overflow" */
