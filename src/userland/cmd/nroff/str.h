/*
 * Nroff/Troff.
 * Input stack.
 */
#define SREQS	0			/* Request */
#define SSINP	1			/* Insertion from standard input */
#define SFILE	2			/* File I/O */
#define SCORE	3			/* Core I/O */
#define SCTEX	4			/* Chained text */
#define SCDIV	5			/* Chained diversion */

/*
 * Used for stacking input sources.
 */
typedef union str {
	struct	{			/* Used for insertions from stdin */
		union str *s_next;	/* Pointer to next in stack */
		int	s_type;		/* Type of input */
		int	s_eoff;		/* End of file flag */
		int	s_clnc;		/* Current line number */
		int	s_nlnc;		/* Last line number */
		int	s_argc;		/* Number of arguments */
		char	*s_argp[ARGSIZE];	/* Pointer to arguments */
		char	*s_abuf;	/* Arg buffer space */
	};
	struct	{			/* File I/O */
		union str *s_next;	/* Pointer to next in stack */
		int	s_type;		/* Type of input */
		int	s_eoff;		/* End of file flag */
		int	s_clnc;		/* Current line number */
		int	s_nlnc;		/* Last line number */
		int	s_argc;		/* Number of arguments */
		char	*s_argp[ARGSIZE];	/* Pointer to arguments */
		char	*s_abuf;	/* Arg buffer space */
		FILE	*s_fp;		/* File pointer */
	};
	struct	{			/* Core I/O */
		union str *s_next;	/* Pointer to next in stack */
		int	s_type;		/* Type of input */
		int	s_eoff;		/* End of file flag */
		int	s_clnc;		/* Current line number */
		int	s_nlnc;		/* Last line number */
		int	s_argc;		/* Number of arguments */
		char	*s_argp[ARGSIZE];	/* Pointer to arguments */
		char	*s_abuf;	/* Arg buffer space */
		char	*s_cp;		/* Pointer in core */
		char	*s_srel;	/* Released when unstacked */
	};
	struct	{			/* Chained I/O */
		union str *s_next;	/* Pointer to next in stack */
		int	s_type;		/* Type of input */
		int	s_eoff;		/* End of file flag */
		int	s_clnc;		/* Current line number */
		int	s_nlnc;		/* Last line number */
		int	s_argc;		/* Number of arguments */
		char	*s_argp[ARGSIZE];	/* Pointer to arguments */
		char	*s_abuf;	/* Arg buffer space */
		char	*s_bufp;	/* Pointer to buffer */
		char	*s_bufend;	/* End of buffer in core */
		union mac *s_macp;	/* Next in chain list */
		int	s_disk;		/* Data is on disk */
		char	*s_sp;		/* Position in core */
		long	s_seek;		/* Seek position */
		int	s_n;		/* Counter (count down) */
	};
} STR;

/*
 * Global variables.
 */
extern	STR	*strp;			/* Input stack */
