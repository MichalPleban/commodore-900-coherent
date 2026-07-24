/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 *	Definitions and declarations for make - Created due to the offended 
 *	sensitivities of all MWC, 1-2-85
 */

#include <stdio.h>
#include <ctype.h>
#include <types.h>
#include <stat.h>
#include <ar.h>

/*
 *	Make exit codes.
 */

#define	ALLOK	0	/* all ok, if -q option then all uptodate */
#define	ERROR	1	/* something went wrong */
#define	NOTUTD	2	/* with -q option, something is not uptodate */


#define	TRUE	(0 == 0)
#define	FALSE	(0 != 0)
#define	EOS	0200
#define	NUL	'\0'

/*
 * types
 */
#define	T_UNKNOWN	0
#define	T_NODETAIL	1
#define	T_DETAIL	2
#define	T_DONE		3

#define	NBACKUP	1024
#define	NMACRONAME	48
#define	NMACRO	100
#define	NTOKBUF	100
#define	LMACRO	(&macro[NMACRO])	/* lwa+1 of macro list */

#define	Streq(a,b)	(strcmp(a,b) == 0)

#define	REL	1	/* lseek argument for relative position */
#define	READ	0	/* open argument for reading */

FILE *fd;

int defining = 0;		/* nonzero => we are defining a macro */

time_t now;

unsigned char backup[NBACKUP];
int nbackup = 0;
int lastc;
int lineno;

char macroname[NMACRONAME+1];

struct token{
	struct token *nexttoken;
	char *value;
};

char *token;
char tokbuf[NTOKBUF];
int toklen;
char *tokp;

struct macro {
	char *name;
	char *value;
	int protected;
}	macro[NMACRO],
	*nmacro = macro;

char *deftarget = NULL;

char usage[] = "Usage: make [-isrntqpd] [-f file] [macro=value] [target]";

int iflag;			/* ignore command errors */
int sflag;			/* don't print command lines */
int rflag;			/* don't read built-in rules */
int nflag;			/* don't execute commands */
int tflag;			/* touch files rather than run commands */
int qflag;			/* zero exit status if file up to date */
int pflag;			/* print macro defns, target descriptions */
int dflag;			/* debug mode -- verbose printout */

struct sym{
	char *name;
	char *action;
	struct dep *deplist;
	int type;
	time_t moddate;
	struct sym *nextsym;
};
struct sym *sym = NULL;
struct sym *suffixes;
struct sym *deflt;

struct dep{
	struct sym *symbol;
	char *action;
	struct dep *nextdep;
};

struct stat statbuf;


/*
 *	The following global variables represent the macro
 *	variables.
 */

char	macvars[] = "@?<*";	/* list of macro variables */
char	*mvarval[4];		/* list of macro variable values */
int	mvarlen[4];		/* list of lengths of macro var. values */

/* Interesting function declarations */

char *malloc(), *realloc();

char *getdef(), *extend(), *expand(), *varexp();
struct token *listtoken();
struct sym *findsym(), *lookup();
struct dep *adddep();
