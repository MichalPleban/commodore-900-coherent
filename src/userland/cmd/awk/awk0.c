/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Definitions of externals.
 */

#include "awk.h"
#include "y.tab.h"

char	*parg;		/* Program argument pointer */
FILE	*pfp;		/* Program file pointer */
int	lexre;		/* On when yylex reading a regular expression */
char	wordbuf[NWORD];	/* yylex string and identifer buffer */
NODE	*codep;			/* Head of interpreted code tree */
int	beginflag;		/* On before files read */
int	endflag;		/* On after files read */
int	runflag;		/* On when running (vs. compiling) */
int	yflag;			/* `-y' option - dual case pattern matching */
int	nlskip;			/* Skip newlines until next token */
int	exitflag;		/* On if exit done */
int	lineno;			/* Current input line # */
char	*inline;		/* Input line */
jmp_buf	nextenv;		/* Environment for next */
jmp_buf	fwenv[NNEST];		/* For/while environment for break/continue */
int	fwlevel = -1;		/* For/while nesting level */
int	brlevel;		/* Brace nesting level */
int	outflag;		/* '>' is output vs. relational (kludge) */

TERM	*symtab[NHASH];		/* Heads of symbol table hash chains */
NODE	*tempnodes;		/* List of temporary nodes during running */
char	SNULL[] = "";		/* Awk's null STRING */
OFILE	files[NOFILE];		/* Open files -- names to prevent re-opening */
char	inbuf[BUFSIZ];		/* Buffer for all (prog, data) input */
char	outbuf[BUFSIZ];		/* Buffer for all output */

/*
 * Built-in variable values.
 */
NODE	*FILENAMEp;		/* Current input filename */
NODE	*NRp;			/* Current input record number */
NODE	*NFp;			/* Number of fields in this record */
NODE	*ORSp;			/* Output record separator */
NODE	*OFSp;			/* Output field separator */
NODE	*RSp;			/* Input record separator */
NODE	*FSp;			/* Input field separator */
NODE	*OFMTp;			/* Output format string (a la printf) */

/*
 * Built-in variables and
 * constants.
 */

/* Constants 0 and 1 */
TERM	xzero, xone;
NODE	xfield0;
