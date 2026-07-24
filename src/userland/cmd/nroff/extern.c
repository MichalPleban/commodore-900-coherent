/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Nroff/Troff.
 * Global variables.
 */
#include <stdio.h>
#include "roff.h"
#include "code.h"
#include "env.h"
#include "div.h"
#include "reg.h"
#include "str.h"

/*
 * Temp file.
 */
FILE *tmp;				/* Temp file pointer */
long tmpseek;				/* Pointer into temp file */

/*
 * Expressions.
 */
int experr;				/* Got an error */
int expmul;				/* Default unit multiplier */
int expdiv;				/* Default unit divisor */
char *expp;				/* Pointer in expression */

/*
 * Enviroments.
 */
ENV env;				/* Current enviroment */
int envstak[EVSSIZE];			/* Enviroment stack */
int envs;				/* Enviroment stack index */

/*
 * Process.
 */
int nbrflag;				/* Don't allow command to break */
int escflag;				/* Last character was escaped */
char *fonwid;				/* Width table for current font */
char *defwid;				/* Width table for default font */

/*
 * Miscellaneous.
 */
char miscbuf[MSCSIZE];			/* Miscellaneous buffer */
int pof;				/* Page offset */
int oldpof;				/* Old page offset */
unsigned pgl;				/* Page length */
unsigned pct;				/* Page counter */
unsigned npn;				/* Next page number */
char esc;				/* Escape character */
char endm[2];				/* End macro */
DIV *mdivp;				/* Pointer to main diversion */
DIV *cdivp;				/* Pointer to diversion stack */
STR *strp;				/* Input stack */
char lbomsg[] = "Line buffer overflow";	/* Message for chkcode macro */
char	curbold,			/* Current bold mode flag */
	curital;			/* Current italics mode flag */
int	curfont,			/* Current font type */
	curpsz;				/* Current point size */
int	mapfont [8] = {'R','I','B','S',0,0,0,0};

/*
 * A null string.
 */
char *null = "";

/*
 * These should be commented.
 */
CODE codeval;
int svs;
char endtrap[2];
int outflag;
int lastcon;
int nnnndn = 0;
REG *regt[RHTSIZE];
REG *nrpnreg;
REG *nrctreg;
REG *nrdlreg;
REG *nrdnreg;
REG *nrdwreg;
REG *nrdyreg;
REG *nrhpreg;
REG *nrlnreg;
REG *nrmoreg;
REG *nrnlreg;
REG *nrsbreg;
REG *nrstreg;
REG *nryrreg;
int envinit[ENVSIZE];
int byeflag;
int ifeflag;
int ntrflag;
int debflag;
char	hyphbuf[WORSIZE];
char	hletbuf[WORSIZE];
char	hindbuf[WORSIZE];
int	d00flag = 0;
char	diskbuf[DBFSIZE];
int	antflag = 0;
int	tntflag = 0;
extern	int	ntroff;			/* Initialized in tty.c */
int	nrorval;
int	arorval;
