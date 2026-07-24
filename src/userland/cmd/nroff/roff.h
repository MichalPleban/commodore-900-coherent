/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Header file.
 */

/*
 * ASCII characters.  (this should be in ascii.h).
 */
#define BEL	007
#define SOH	001

/*
 * Parameters determining sizes of buffers.
 */
#define LINSIZE	400			/* Size of line buffer */
#define WORSIZE	200			/* Size of word buffer */
#define ENVSIZE	3			/* Number of enviroments */
#define EVSSIZE	20			/* Pushdown stack for enviroments */
#define TABSIZE	20			/* Maximum number of tab stops */
#define ARGSIZE	10			/* Maximum number of arguments */
#define ABFSIZE	128			/* Size of argument buffer */
#define MSCSIZE	128			/* Size of miscellaneous buffer */
#define CBFSIZE	128			/* Size of char buffer */
#define DBFSIZE	512			/* Size of disk buffer */
#define RHTSIZE	128			/* Size of register hash table */
#define ASCSIZE	128			/* Size of ASCII character set */

/*
 * Types of justification.
 */
#define NJUS	0			/* None (marks end of table) */
#define LJUS	1			/* Left justification */
#define CJUS	2			/* Centre justification */
#define RJUS	3			/* Right justification */
#define FJUS	4			/* Filled justification */

/*
 * Unit measures.
 */
#define SMINCH	((long) sinmul)		/* Inch (mul) */
#define SDINCH	((long) sindiv)		/* Inch (div) */
#define SMCENT	((long) 50*sinmul)	/* Centimetre (mul) */
#define SDCENT	((long) 127*sindiv)	/* Centimetre (div) */
#define SMPICA	((long) sinmul)		/* Pica (mul) */
#define SDPICA	((long) 6*sindiv)	/* Pica (div) */
#define SMEMSP	((long) newpsz*semmul)	/* Em space (mul) */
#define SDEMSP	((long) semdiv)		/* Em space (div) */
#define SMENSP	((long) newpsz*senmul)	/* En space (mul) */
#define SDENSP	((long) sendiv)		/* En space (div) */
#define SMVEMS	((long) newpsz)		/* Vertical em space (mul) */
#define SDVEMS	((long) 1)		/* Vertical em space  (div) */
#define SMNARS	((long) snrmul)		/* Narrow space (mul) */
#define SDNARS	((long) snrdiv)		/* Narrow space (div) */
#define SMPOIN	((long) sinmul)		/* Point (mul) */
#define SDPOIN	((long) 72*sindiv)	/* Point (div) */
#define SMVLSP	((long) vls)		/* Line space (mul) */
#define SDVLSP	((long) 1)		/* Line space (div) */
#define SMDIGW	((long) sdwmul)		/* Digit width (mul) */
#define SDDIGW	((long) sdwdiv)		/* Digit width (div) */
#define SMHRES	((long) shrmul)		/* Horizontal resolution (mul) */
#define SDHRES	((long) shrdiv)		/* Horizontal resolution (div) */
#define SMVRES	((long) svrmul)		/* Vertical resolution (mul) */
#define SDVRES	((long) svrdiv)		/* Vertical resolution (div) */
#define SMUNIT	((long) 1)		/* Unit (mul) */
#define SDUNIT	((long) 1)		/* Unit (div) */

/*
 * Temp file.
 */
extern	FILE	*tmp;			/* Temp file pointer */
extern	long	tmpseek;		/* Pointer into temp file */
extern  char	*nalloc();
extern  char	*findreg();
extern  char	*makereg();
extern  char	*allstr();
extern  char	*getnreg();
extern  char	*nextarg();
extern  char	*duplstr();
extern	char	*index();

/*
 * Expressions.
 */
extern	int	experr;			/* Got an error */
extern	int	expmul;			/* Default unit multiplier */
extern	int	expdiv;			/* Default unit divisor */
extern	char	*expp;			/* Pointer in expression */

/*
 * Process.
 */
extern	int	nbrflag;		/* Don't allow command to break */
extern	int	escflag;		/* Last character was escaped */
extern	int	outflag;		/* Have output a line */
extern	char	*null;			/* Pointer to a null string */

/*
 * Miscellaneous.
 */
extern	char	miscbuf[MSCSIZE];	/* Miscellaneous buffer */
extern	int	pof;			/* Page offset */
extern	int	oldpof;			/* Old page offset */
extern	unsigned pgl;			/* Page length */
extern	unsigned pct;			/* Page counter */
#define  pno	(nrpnreg->r_nval)
extern	unsigned npn;			/* Next page number */
extern	char	esc;			/* Escape character */
extern	char	endtrap[2];		/* End macro */
extern	char	curbold,		/* Current bold mode */
		curital;		/* Current italics mode */
extern	int	curfont,		/* Current font type */
		curpsz;			/* Current point size */

/*
 * Device dependent variables.
 */
extern	int	ntroff;			/* Type of programme */
extern	long	sinmul;			/* Multiplier for inch */
extern	long	sindiv;			/* Divisor for inch */
extern	long	semmul;			/* Multiplier for em space */
extern	long	semdiv;			/* Divisor for em space */
extern	long	senmul;			/* Multiplier for en space */
extern	long	sendiv;			/* Divisor for en space */
extern	long	swdmul;			/* Multiplier for width tables */
extern	long	swddiv;			/* Divisor for width tables */

/*
 * These should be commented.
 */
extern	int	svs;
extern	int	lastcon;
extern	int	nnnndn;
extern long shrmul;			/* Horizontal resolution (mul) */
extern long shrdiv;			/* Horizontal resolution (div) */
extern long svrmul;			/* Vertical resolution (mul) */
extern	long	svrdiv ;		/* Vertical resolution (div) */

typedef struct {
	char	o_digit;			/* Offset for digit */
	char	o_state;			/* Next state */
} ROM;
extern	ROM	romtab[10];
extern	char	trantab[ASCSIZE];
extern char esctab[];
extern int asctab[];
extern int byeflag;
typedef struct {
	char q_name[2];
	int (*q_func)();
} REQ;
extern REQ reqtab[];
#define INFINITY	32767
extern long sdwmul;
extern long sdwdiv, snrmul, snrdiv;
typedef struct {
	char f_name[2];
	int f_font;
	char f_bold;
	char f_ital;
} FTB;
extern FTB fontab[];
extern int ifeflag;
extern int ntrflag;
#if RSX
#define TMACDIR	"[102,17]"
#else
#define	TMACDIR	"/usr/lib/"
#endif
extern int debflag;
extern	int	d00flag;
extern	char	diskbuf[DBFSIZE];	/* Disk buffer for temp file */
#define NROFF	1			/* Nroff programme */
#define TROFF	2			/* Troff programme */
extern	int	antflag;
extern	int	tntflag;
extern	int	nrorval;
extern	int	arorval;
