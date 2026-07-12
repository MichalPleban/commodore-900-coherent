
#line 1 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"

/*
 * Set return status based on
 * various specified conditions,
 * mostly related to files.
 * Used mostly in shell files.
 */

#include <stdio.h>
#include <stat.h>
#include <access.h>
#include "testnode.h"

#define	NPRIM	(sizeof(prims)/sizeof(prims[0]))
#define	NFNAME	500		/* size of filename buffer */

#include "y.tab.h"
#define YYCLEARIN yychar = -1000
#define YYERROK yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yyval, yylval;

#line 64 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"

struct	prim	{
	char	*p_name;
	int	p_lval;
}	prims[] = {
	"-r", _R,
	"-w", _W,
	"-f", _F,
	"-d", _D,
	"-s", _S,
	"-t", _T,
	"-z", _Z,
	"-n", _N,
	"-eq", _EQ,
	"-ne", _NE,
	"-gt", _GT,
	"-ge", _GE,
	"-lt", _LT,
	"-le", _LE,
	"-o", OR,
	"-a", AND,
};


char	**gav;
int	gac;

struct	stat	sb;

NODE	*code;

char	*next();
NODE	*bnode();
NODE	*lnode();
long	atol();
int	xr();
int	xw();
int	xf();
int	xd();
int	xs();
int	xt();
int	xz();
int	xn();
int	xseq();
int	xsneq();
int	xeq();
int	xne();
int	xgt();
int	xge();
int	xlt();

main(argc, argv)
char *argv[];
{
	gav = argv+1;
	gac = argc-1;
	if (argv[0][0]=='[' && argv[0][1]=='\0') {
		if (strcmp(argv[gac], "]") != 0)
			tsterr("unbalanced [..]");
		gac--;
	}
	if (gac == 0)
		exit(1);
	yyparse();
	exit(!execute(code));
}

/*
 * Lexical analyser
 */
yylex()
{
	register char *ap;
	register struct prim *pp;

	if ((ap = next()) == NULL)
		return ('\n');
	if (ap[1] == '\0')
		if (ap[0]=='(' || ap[0]==')' || ap[0]=='!')
			return (ap[0]);
	if (ap[0]=='!' && ap[1]=='=' && ap[2]=='\0')
		return (SNEQ);
	if (ap[0]=='=' && ap[1]=='\0')
		return (SEQ);
	if (*ap == '-')
		for (pp = prims; pp < &prims[NPRIM]; pp++)
			if (strcmp(pp->p_name, ap) == 0)
				return (pp->p_lval);
	yylval.fname = ap;
	return (STR);
}

yyerror()
{
	fprintf(stderr, "Test expression syntax error\n");
	usage();
}

/*
 * Return the next argument from the arg list.
 */
char *
next()
{
	if (gac < 1)
		return (NULL);
	gac--;
	return (*gav++);
}

/*
 * Build an expression tree node (non-leaf)
 */
NODE *
bnode(op, left, right)
int op;
NODE *left, *right;
{
	register NODE *np;
	char *malloc();

	if ((np = (NODE *)malloc(sizeof (NODE))) == NULL)
		tsterr("Out of space");
	np->n_un.n_op = op;
	np->n_left = left;
	np->n_right = right;
	return (np);
}

/*
 * Build a leaf node in expression tree.
 */
NODE *
lnode(fn, str1, str2)
int (*fn)();
char *str1, *str2;
{
	register NODE *np;
	char *malloc();

	if ((np = (NODE *)malloc(sizeof (NODE))) == NULL)
		tsterr("Out of space");
	np->n_left = np->n_right = NULL;
	np->n_un.n_fun = fn;
	np->n_s1 = str1;
	np->n_s2 = str2;
	return (np);
}

/*
 * Execute compiled code.
 */
execute(np)
register NODE *np;
{
	if (np->n_left != NULL)
		switch (np->n_un.n_op) {
		case AND:
			if (execute(np->n_left) && execute(np->n_right))
				return (1);
			return (0);

		case OR:
			if (execute(np->n_left) || execute(np->n_right))
				return (1);
			return (0);

		case '!':
			return (!execute(np->n_left));

		default:
			tsterr("Panic: bad tree (op %d)", np->n_un.n_op);
		}
	else
		return ((*np->n_un.n_fun)(np));
	/* NOTREACHED */
}

/*
 * Check to see if the file exists
 * and if readable.
 */
xr(np)
NODE *np;
{
	return (access(np->n_s1, AREAD) >= 0);
}

/*
 * Check if the file exists and is
 * writeable.
 */
xw(np)
NODE *np;
{
	return (access(np->n_s1, AWRITE) >= 0);
}

/*
 * Check if the file exists and is not
 * a directory.
 */
xf(np)
NODE *np;
{
	return (stat(np->n_s1, &sb)>=0 && (sb.st_mode&S_IFMT)!=S_IFDIR);
}

/*
 * Check to see if the file exists
 * and is a directory.
 */
xd(np)
NODE *np;
{
	return (stat(np->n_s1, &sb)>=0 && (sb.st_mode&S_IFMT)==S_IFDIR);
}

/*
 * Check to see if the file exists
 * and has a non-zero size.
 */
xs(np)
NODE *np;
{
	return (stat(np->n_s1, &sb)>=0 && sb.st_size>0);
}

/*
 * Check to see if the file
 * descriptor is associated
 * with a terminal.
 */
xt(np)
NODE *np;
{
	return (isatty(atoi(np->n_s1)));
}

/*
 * True if the length of the given
 * string is zero.
 */
xz(np)
NODE *np;
{
	return (np->n_s1[0] == '\0');
}

/*
 * True if the length of the given
 * string is non-zero.
 */
xn(np)
NODE *np;
{
	return (np->n_s1[0] != '\0');
}

/*
 * True if the two strings are
 * lexicographically equal.
 */
xseq(np)
register NODE *np;
{
	return (strcmp(np->n_s1, np->n_s2) == 0);
}

/*
 * True if the two strings are
 * lexicographically unequal.
 */
xsneq(np)
register NODE *np;
{
	return (strcmp(np->n_s1, np->n_s2) != 0);
}

/*
 * True if the two numbers are
 * equal.
 */
xeq(np)
register NODE *np;
{
	return (atol(np->n_s1) == atol(np->n_s2));
}

/*
 * True if the two numbers are
 * not equal.
 */
xne(np)
register NODE *np;
{
	return (atol(np->n_s1) != atol(np->n_s2));
}

/*
 * True if the first number is
 * greater than the second.
 */
xgt(np)
register NODE *np;
{
	return (atol(np->n_s1) > atol(np->n_s2));
}

/*
 * True if the first number is
 * greater than or equal to the second.
 */
xge(np)
register NODE *np;
{
	return (atol(np->n_s1) >= atol(np->n_s2));
}

/*
 * True if the first number is
 * less than the second.
 */
xlt(np)
register NODE *np;
{
	return (atol(np->n_s1) < atol(np->n_s2));
}

/*
 * True if the first number is
 * less than or equal to the second.
 */
xle(np)
register NODE *np;
{
	return (atol(np->n_s1) <= atol(np->n_s2));
}

/*
 * Error messages.
 */
/* VARARGS */
tsterr(x)
{
	fprintf(stderr, "test: %r\n", &x);
	exit(1);
}

usage()
{
	fprintf(stderr, "Usage: test expression\n");
	exit(1);
}
#ifdef YYTNAMES
struct yytname yytnames[25] =
{
	"$end", -1, 
	"error", -2, 
	"OR", 256, 
	"AND", 257, 
	"'!'", 33, 
	"_R", 258, 
	"_W", 259, 
	"_F", 260, 
	"_D", 261, 
	"_S", 262, 
	"_T", 263, 
	"_Z", 264, 
	"_N", 265, 
	"SEQ", 266, 
	"SNEQ", 267, 
	"_EQ", 268, 
	"_NE", 269, 
	"_GT", 270, 
	"_GE", 271, 
	"_LT", 272, 
	"_LE", 273, 
	"STR", 274, 
	"'\\n'", 10, 
	"'('", 40, 
	"')'", 41, 
	NULL
} ;
#endif
unsigned yypdnt[24] = {
00, 01, 02, 02, 02, 02, 02, 02, 
02, 02, 02, 02, 02, 02, 02, 02, 
02, 02, 02, 02, 02, 02, 02, 02  
} ;
unsigned yypn[24] = {
02, 02, 03, 02, 03, 03, 02, 02, 
02, 02, 02, 02, 01, 02, 02, 03, 
03, 03, 03, 03, 03, 03, 03, 01  
} ;
unsigned yypgo[3] = {
00, 00, 02  
} ;
unsigned yygo[12] = {
0176030, 014, 01, 016, 013, 037, 041, 055, 
042, 056, 0176030, 015  
} ;
unsigned yypa[47] = {
00, 00, 030, 034, 040, 044, 050, 054, 
060, 064, 070, 00, 0112, 0116, 0126, 0130, 
0132, 0134, 0136, 0140, 0142, 0144, 0146, 0150, 
0154, 0160, 0164, 0170, 0174, 0200, 0204, 0210, 
0220, 00, 00, 0224, 0226, 0230, 0232, 0234, 
0236, 0240, 0242, 0244, 0246, 0250, 0254  
} ;
unsigned yyact[174] = {
01, 041, 02, 0402, 03, 0403, 04, 0404, 
05, 0405, 06, 0406, 07, 0407, 010, 0410, 
011, 0411, 012, 0422, 013, 050, 060000, 0176030, 
017, 0422, 060000, 0176030, 020, 0422, 060000, 0176030, 
021, 0422, 060000, 0176030, 022, 0422, 060000, 0176030, 
023, 0422, 060000, 0176030, 024, 0422, 020014, 0176030, 
025, 0422, 060000, 0176030, 026, 0422, 060000, 0176030, 
027, 0412, 030, 0413, 031, 0414, 032, 0415, 
033, 0416, 034, 0417, 035, 0420, 036, 0421, 
020027, 0176030, 040, 0177777, 060000, 0176030, 041, 0400, 
042, 0401, 043, 012, 060000, 0176030, 020003, 0176030, 
020006, 0176030, 020007, 0176030, 020010, 0176030, 020011, 0176030, 
020012, 0176030, 020013, 0176030, 020015, 0176030, 020016, 0176030, 
044, 0422, 060000, 0176030, 045, 0422, 060000, 0176030, 
046, 0422, 060000, 0176030, 047, 0422, 060000, 0176030, 
050, 0422, 060000, 0176030, 051, 0422, 060000, 0176030, 
052, 0422, 060000, 0176030, 053, 0422, 060000, 0176030, 
041, 0400, 042, 0401, 054, 051, 060000, 0176030, 
040000, 0177777, 060000, 0176030, 020001, 0176030, 020017, 0176030, 
020020, 0176030, 020021, 0176030, 020022, 0176030, 020023, 0176030, 
020024, 0176030, 020025, 0176030, 020026, 0176030, 020002, 0176030, 
042, 0401, 020004, 0176030, 020005, 0176030  
} ;
#include "action.h"
#define YYNOCHAR (-1000)
#define	yyerrok	yyerrflag=0
#define	yyclearin	yylval=YYNOCHAR
int yystack[YYMAXDEPTH];
YYSTYPE yyvstack[YYMAXDEPTH], *yyv;
int yychar;

#ifdef YYDEBUG
int yydebug = 1;	/* No sir, not in the BSS */
#include <stdio.h>
#endif

short yyerrflag;
int *yys;

yyparse()
{
	register YYSTYPE *yypvt;
	int act;
	register unsigned *ip, yystate;
	int pno;
	yystate = 0;
	yychar = YYNOCHAR;
	yyv = &yyvstack[-1];
	yys = &yystack[-1];

stack:
	if( ++yys >= &yystack[YYMAXDEPTH] ) {
		write(2, "Stack overflow\n", 15);
		exit(1);
	}
	*yys = yystate;
	*++yyv = yyval;
#ifdef YYDEBUG
	if( yydebug )
		fprintf(stdout, "Stack state %d, char %d\n", yystate, yychar);
#endif

read:
	ip = &yyact[yypa[yystate]];
	if( ip[1] != YYNOCHAR ) {
		if( yychar == YYNOCHAR ) {
			yychar = yylex();
#ifdef YYDEBUG
			if( yydebug )
				fprintf(stdout, "lex read char %d, val %d\n", yychar, yylval);
#endif
		}
		while (ip[1]!=YYNOCHAR) {
			if (ip[1]==yychar)
				break;
			ip += 2;
		}
	}
	act = ip[0];
	switch( act>>YYACTSH ) {
	case YYSHIFTACT:
		if( ip[1]==YYNOCHAR )
			goto YYerract;
		if( yychar != -1 )
			yychar = YYNOCHAR; /* dont throw away EOF */
		yystate = act&YYAMASK;
		yyval = yylval;
#ifdef YYDEBUG
		if( yydebug )
			fprintf(stdout, "shift %d\n", yystate);
#endif
		if( yyerrflag )
			--yyerrflag;
		goto stack;

	case YYACCEPTACT:
#ifdef YYDEBUG
		if( yydebug )
			fprintf(stdout, "accept\n");
#endif
		return(0);

	case YYERRACT:
	YYerract:
		switch (yyerrflag) {
		case 0:
			yyerror("Syntax error");

		case 1:
		case 2:

			yyerrflag = 3;
			while( yys >= & yystack[0] ) {
				ip = &yyact[yypa[*yys]];
				while( ip[1]!=YYNOCHAR )
					ip += 2;
				if( (*ip&~YYAMASK) == (YYSHIFTACT<<YYACTSH) ) {
					yystate = *ip&YYAMASK;
					goto stack;
				}
#ifdef YYDEBUG
				if( yydebug )
					fprintf(stderr, "error recovery leaves state %d, uncovers %d\n", *yys, yys[-1]);
#endif
				yys--;
				yyv--;
			}
#ifdef YYDEBUG
			if( yydebug )
				fprintf(stderr, "no shift on error; abort\n");
#endif
			return(1);

		case 3:
#ifdef YYDEBUG
			if( yydebug )
				fprintf(stderr, "Error recovery clobbers char %o\n", yychar);
#endif
			if( yychar==YYEOFVAL )
				return(1);
			yychar = YYNOCHAR;
			goto read;
		}

	case YYREDACT:
		pno = act&YYAMASK;
#ifdef YYDEBUG
		if( yydebug )
			fprintf(stdout, "reduce %d\n", pno);
#endif
		yypvt = yyv;
		yyv -= yypn[pno];
		yys -= yypn[pno];
		yyval = yyv[1];
		switch(pno) {

case 1: {

#line 36 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 code = yypvt[-1].nodeptr; return; }break;

case 2: {

#line 40 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = yypvt[-1].nodeptr; }break;

case 3: {

#line 41 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = bnode('!', yypvt[0].nodeptr, NULL); }break;

case 4: {

#line 42 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = bnode(OR, yypvt[-2].nodeptr, yypvt[0].nodeptr); }break;

case 5: {

#line 43 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = bnode(AND, yypvt[-2].nodeptr, yypvt[0].nodeptr); }break;

case 6: {

#line 44 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xr, yypvt[0].fname, NULL); }break;

case 7: {

#line 45 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xw, yypvt[0].fname, NULL); }break;

case 8: {

#line 46 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xf, yypvt[0].fname, NULL); }break;

case 9: {

#line 47 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xd, yypvt[0].fname, NULL); }break;

case 10: {

#line 48 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xs, yypvt[0].fname, NULL); }break;

case 11: {

#line 49 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xt, yypvt[0].fname, NULL); }break;

case 12: {

#line 50 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xt, "1", NULL); }break;

case 13: {

#line 51 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xz, yypvt[0].fname, NULL); }break;

case 14: {

#line 52 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xn, yypvt[0].fname, NULL); }break;

case 15: {

#line 53 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xseq, yypvt[-2].fname, yypvt[0].fname); }break;

case 16: {

#line 54 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xsneq, yypvt[-2].fname, yypvt[0].fname); }break;

case 17: {

#line 55 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xeq, yypvt[-2].fname, yypvt[0].fname); }break;

case 18: {

#line 56 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xne, yypvt[-2].fname, yypvt[0].fname); }break;

case 19: {

#line 57 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xgt, yypvt[-2].fname, yypvt[0].fname); }break;

case 20: {

#line 58 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xge, yypvt[-2].fname, yypvt[0].fname); }break;

case 21: {

#line 59 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xlt, yypvt[-2].fname, yypvt[0].fname); }break;

case 22: {

#line 60 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xle, yypvt[-2].fname, yypvt[0].fname); }break;

case 23: {

#line 61 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/test/test.y"
 yyval.nodeptr = lnode(xn, yypvt[0].fname, NULL); }break;

		}
		ip = &yygo[ yypgo[yypdnt[pno]] ];
		while( *ip!=*yys && *ip!=YYNOCHAR )
			ip += 2;
		yystate = ip[1];
		goto stack;
	}
}




