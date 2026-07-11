#include <stdio.h>
#include "bc.h"


int	allok	    = TRUE,	/* True iff no outstanding errors */
	scale	    = 0;	/* scale register */
extern	int	ibase;		/* input base from libmp/min.c */
dicent	*dictionary = NULL;	/* root of string table */
code	cstream[MAXCODE],	/* code stream */
	*loc = cstream;		/* where next item of code stream goes */
mint	ten,			/* constant ten */
	maxsobase,		/* max small output base (16) */
	outbase;		/* output base */
rvalue	dot,			/* last number printed */
	zero;			/* constant zero */
FILE	*infile = stdin;	/* current input file */


init()
{
	mitom(10, &ten);
	mcopy(&ten, &outbase);
	mitom(16, &maxsobase);
	mitom(0, &dot.mantissa);
	dot.scale = 0;
	mitom(0, &zero.mantissa);
	zero.scale = 0;
}
