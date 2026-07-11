
#line 1 "/usr/src/cmd/expr.y"

/*
 * Expr.y, yacc grammar for expr. Designed so no stdio is called, which
 * makes the final object code about 1/3 smaller. To make, say
 *	yacc expr.y; cc -O -o expr y.tab.c;
 */
#include <ctype.h>
#include <stdio.h>

#define TRUE	(0 == 0)
#define FALSE	(0 != 0)
#define true(e)	(*(e) == '\0' || (isnum(e) && atol(e) == 0) ? FALSE : TRUE)

int	exstat;		/* exit status returned by main() */
char	*result;	/* final expr printed by main */
char	s0[] = "0";	/* `0' integer - `false' expr */
char	s1[] = "1";	/* `1' integer - `true' expr */

char	*regexp(),
	*arithop(),
	*relop(),
	*realloc(),
	*ltoa();
long	atol();


/*
 * The following names are used by the regular expression operator.
 */
#define BRSIZE	10			/* Length of brace list */
typedef	struct {
	char	*b_bp;			/* Ptr to start of string matched */
	char	*b_ep;			/* Ptr to end of string matched */
} BRACE;

#define CSNUL	000			/* End of expression */
#define CSSOL	001			/* Match start of line */
#define CSEOL	002			/* End of line */
#define CSOPR	003			/* \( */
#define CSCPR	004			/* \) */
#define CSBRN	005			/* Match nth brace */
#define CSDOT	006			/* Any character */
#define CMDOT	007			/* Stream of any characters */
#define CSCHR	010			/* Match given character */
#define CMCHR	011			/* Match stream of given characters */
#define CSCCL	014			/* Character class */
#define CMCCL	015			/* Stream of character class */
#define CSNCL	016			/* Not character class */
#define CMNCL	017			/* Stream of not char class */

#define	getx(c)		*e++
#define ungetx(c)	--e

BRACE	brlist[BRSIZE];			/* brace list */
int	brcount;			/* # of braces in reg_expr */
char	*codebuf;			/* Ptr to a compiled regular expr */
int	cbsiz = 512;			/* Initial size of codebuf */

char	*match();
char	*overflow();


#include "y.tab.h"
#define YYCLEARIN yychar = -1000
#define YYERROK yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yyval, yylval;

#line 137 "/usr/src/cmd/expr.y"





char	**av;		/* Global version of argv[] in main() */
int	avx;		/* Index into av[] */

main(argc, argv)
int argc;
char **argv;
{
	if (argc == 1)
		return (2);
	av = argv;
	yyparse();
	output(result, 1);
	output("\n", 1);
	return (exstat);
}


char *
arithop(op, e1, e2)
register int op;
register char *e1, *e2;
{
	register long v1, v2;

	if (!isnum(e1)) {
		avx -= 3;
		yyerror();
	}
	if (!isnum(e2)) {
		--avx;
		yyerror();
	}

	v1 = atol(e1);
	v2 = atol(e2);
	switch (op) {
	case '+':
		v1 += v2;
		break;
	case '-':
		v1 -= v2;
		break;
	case '*':
		v1 *= v2;
		break;
	case '/':
		v1 /= v2;
		break;
	case '%':
		v1 %= v2;
		break;
	}
	return (ltoa(v1));
}

char *
relop(op, e1, e2)
register int op;
register char *e1, *e2;
{
	register int cmp;
	register long v1, v2;

	if (!isnum(e1) || !isnum(e2))
		cmp = strcmp(e1, e2);
	else {
		v1 = atol(e1);
		v2 = atol(e2);
		cmp = (v1 > v2) ? 1 : (v1 == v2) ? 0 : -1;
	}
	switch (op) {
	case '<':
		return ((cmp < 0) ? s1 : s0);
	case '>':
		return ((cmp > 0) ? s1 : s0);
	case LE:
		return ((cmp <= 0) ? s1 : s0);
	case GE:
		return ((cmp >= 0) ? s1 : s0);
	case EQ:
		return ((cmp == 0) ? s1 : s0);
	case NEQ:
		return ((cmp != 0) ? s1 : s0);
	}
}


char *
regexp(e1, e2)
char *e1, *e2;
{
	register char *a = e1;
	register char *b;
	register BRACE *brp;

	codebuf = malloc(512);
	compile(e2);
	if (brcount > 0)	/* brcount is now the number of braces in e2 */
		brlist[brcount].b_bp = brlist[brcount].b_ep = NULL;
	if (codebuf[0] == CSSOL)
		b = match(a, codebuf + 1);
	else
		for ( ; *a != '\0'; ++a)
			if ((b = match(a, codebuf)) != NULL)
				break;
	if (b == NULL)
		return ("0");
	if (brcount == 0)
		return (ltoa((long)(b - a)));

	/* Remaining case is extraction of fields */
	for (a = e1, brp = brlist; (b = brp->b_bp) != NULL; ++brp)
		while (b < brp->b_ep)
			*a++ = *b++;
	*a = '\0';
	free (codebuf);
	return (e1);
}


isnum(e)
register char *e;
{
	register int c;

	if ((c = *e) == '-' || c == '+') 
		++e;
	while ((c = *e++) != '\0')
		if (!isdigit(c))
			return (FALSE);
	return (TRUE);
}

/*
 * Convert long to ascii. Return pointer to the necessary malloced storage.
 */
char *
ltoa(n)
register long n;
{
	char buf[12];
	register char *bp = buf;
	register char *ep;
	register char *e;

	e = ep = malloc(12);
	if (n < 0) {
		*ep++ = '-';
		n = -n;
	}
	do {
		*bp++ = (n % 10) + '0';
		n /= 10;
	} while (n > 0);
	while (bp > buf)
		*ep++ = *--bp;
	*ep = '\0';
	return (e);
}


/*
 * Compile the regular expression e into codebuf.
 * Invoke regerror() on a regular expression syntax error.
 */
compile(e)
register char *e;
{
	register int c;
	register char *cp, *lcp;
	int blevel, n, notflag, bstack[BRSIZE + 1];

	brcount = 0;
	blevel = 0;
	cp = &codebuf[0];
	if ((c = getx(c)) == '^') {
		*cp++ = CSSOL;
		c = getx(c);
	}
	while (c != '\0') {
		if (cp > &codebuf[cbsiz-4])
			cp = overflow(cp);
		switch (c) {
		case '*':
			regerror();
		case '.':
			if ((c = getx(c)) != '*') {
				*cp++ = CSDOT;
				continue;
			}
			*cp++ = CMDOT;
			c = getx(c);
			continue;
		case '$':
			if ((c = getx(c)) != '\0') {
				ungetx(c);
				c = '$';
				goto character;
			}
			*cp++ = CSEOL;
			continue;
		case '[':
			/*
			 * lcp[0] will contain C<S|M><C|N>CL. lcp[1] will be
			 * the number of chars in the class. These are followed
			 * by the members of the class singly enumerated.
			 * ']' is valid only at the start of the member list.
			 * '-' is valid only at the end of the member list.
			 */
			lcp = cp;
			if ((c = getx(c)) == '^')
				notflag = TRUE;
			else {
				notflag = FALSE;
				ungetx(c);
			}
			cp += 2;
			if ((c = getx(c)) == ']')
				*cp++ = c;
			else
				ungetx(c);
			while ((c = getx(c)) != ']') {
				if (c == '\0')
					regerror();
				if (c!='-' || cp==lcp+2) {
					if (cp >= &codebuf[cbsiz-4])
						cp = overflow(cp);
					*cp++ = c;
					continue;
				}

				/* c = '-' now. Lookahead at the next char */
				if ((c = getx(c)) == '\0')
					regerror();
				if (c == ']') {
					*cp++ = '-';
					ungetx(c);
					continue;
				}
				if ((n=cp[-1]) > c)
					regerror();
				while (++n <= c) {
					if (cp >= &codebuf[cbsiz-4])
						cp = overflow(cp);
					*cp++ = n;
				}
			}
			if ((c = getx(c)) == '*') {
				lcp[0] = (notflag) ? CMNCL : CMCCL;
				c = getx(c);
			}
			else
				lcp[0] = (notflag) ? CSNCL : CSCCL;
			if ((n=cp-(lcp+2)) > 255)
				regerror();
			*++lcp = n;
			continue;
		case '\\':
			switch (c = getx(c)) {
			case '\0':
				regerror();
			case '(':
				*cp++ = CSOPR;
				*cp++ = bstack[blevel++] = brcount++;
				c = getx(c);
				continue;
			case ')':
				if (blevel == 0)
					regerror();
				*cp++ = CSCPR;
				*cp++ = bstack[--blevel];
				c = getx(c);
				continue;
			default:
				if (isascii(c) && isdigit(c)) {
					*cp++ = CSBRN;
					*cp++ = c-'0' - 1;
					c = getx(c);
					continue;
				}
			}
		default:
		character:
			*cp++ = CSCHR;
			*cp++ = c;
			if ((c = getx(c)) == '*') {
				cp[-2] = CMCHR;
				c = getx(c);
			}
		}
	}
	*cp++ = CSNUL;
	return;
}

/*
 * Given a pointer to a compiled expression `cp' and a pointer to a line `lp',
 * return a ptr to the char following the last char of the match
 * if successful, NULL otherwise.
 */
char *
match(lp, cp)
register char *lp, *cp;
{
	register int n;
	char *llp, *lcp;

	for (;;) {
		switch (*cp++) {
		case CSNUL:
			return (lp);
		case CSEOL:
			if (*lp)
				return (NULL);
			return (lp);
		case CSOPR:
			brlist[*cp++].b_bp = lp;
			continue;
		case CSCPR:
			brlist[*cp++].b_ep = lp;
			continue;
		case CSBRN:
			n = *cp++;
			lcp = cp;
			cp = brlist[n].b_bp;
			n = brlist[n].b_ep - cp;
			if (n > strlen(lp))
				return (NULL);
			while (n-- > 0)
				if (*lp++ != *cp++)
					return (NULL);
			cp = lcp;
			continue;
		case CSDOT:
			if (*lp++ == '\0')
				return (NULL);
			continue;
		case CMDOT:
			llp = lp;
			while (*lp)
				lp++;
			goto star;
		case CSCHR:
			if (*cp++ != *lp++)
				return (NULL);
			continue;
		case CMCHR:
			llp = lp;
			while (*cp == *lp)
				lp++;
			cp++;
			goto star;
		case CSCCL:
			n = *cp++;
			while (*cp++ != *lp)
				if (--n == 0)
					return (NULL);
			lp++;
			cp += n-1;
			continue;
		case CMCCL:
			llp = lp;
			lcp = cp;
			while (*lp) {
				cp = lcp;
				n = *cp++;
				while (*cp++ != *lp)
					if (--n == 0)
						goto star;
				lp++;
			}
			cp = lcp + *lcp + 1;
			goto star;
		case CSNCL:
			if (*lp == '\0')
				return (NULL);
			n = *cp++;
			while (n--)
				if (*cp++ == *lp)
					return (NULL);
			lp++;
			continue;
		case CMNCL:
			llp = lp;
			lcp = cp;
			while (*lp) {
				cp = lcp;
				n = *cp++;
				while (n--) {
					if (*cp++ == *lp) {
						cp = lcp + *lcp + 1;
						goto star;
					}
				}
				lp++;
			}
			cp = lcp + *lcp + 1;
		star:
			do {
				if (lcp=match(lp, cp))
					return (lcp);
			} while (--lp >= llp);
			return (NULL);
		}
	}
}

/*
 * overflow enlarges codebuf by 128 bytes. The argument is a pointer
 * to a position in codebuf - the function returns a pointer with the same
 * relative position in the new buffer.
 */
char *
overflow(pc)
register char *pc;
{
	register int posn = pc - codebuf;

	if ((codebuf = realloc(codebuf, cbsiz += 128)) == NULL)
		regerror();
	return (codebuf + posn);
}

/*
 * An output function to avoid having to include stdio.
 */
output(s, fildes)
register char *s;
int fildes;
{
	register int len;

	len = strlen(s);
	if ((write(fildes, s, len)) != len)
		exit(3);
}


yylex()
{
	register int c;

	if ((yylval.str = av[++avx]) == NULL)
		return (EOF);
	if (av[avx][1] == '\0')
		switch (c = av[avx][0]) {
		case '{':
		case '}':
		case ',':
		case '|':
		case '&':
		case '<':
		case '>':
		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		case ':':
		case '!':
		case '(':
		case ')':
			return (c);
		default:
			return (STR);
		}
	if (av[avx][1] == '='  &&  av[avx][2] == '\0')
		switch (c = av[avx][0]) {
		case '<':
			return (LE);
		case '>':
			return (GE);
		case '=':
			return (EQ);
		case '!':
			return (NEQ);
		default:
			return (STR);
		}
	if (strcmp(yylval.str, "len") == 0)
		return (LEN);
	return (STR);
}

/*
 * The common code between yyerror() and regerror() (in regexp.c) is split
 * off into errexit().
 */
yyerror()
{
	output("expr: ", 2);
	errexit();
}
regerror()
{
	output("expr: regular expression ", 2);
	--avx;
	errexit();
}
errexit()
{
	output("syntax error at argument # ", 2);
	output(ltoa((long) avx), 2);
	output("\n", 2);
	exit(2);
}


#ifdef YYTNAMES
struct yytname yytnames[25] =
{
	"$end", -1, 
	"error", -2, 
	"STR", 256, 
	"'|'", 124, 
	"'&'", 38, 
	"'<'", 60, 
	"'>'", 62, 
	"LE", 258, 
	"GE", 259, 
	"EQ", 260, 
	"NEQ", 261, 
	"'+'", 43, 
	"'-'", 45, 
	"'*'", 42, 
	"'/'", 47, 
	"'%'", 37, 
	"':'", 58, 
	"'!'", 33, 
	"UMINUS", 262, 
	"LEN", 263, 
	"'('", 40, 
	"')'", 41, 
	"'{'", 123, 
	"','", 44, 
	"'}'", 125, 
	NULL,
} ;
#endif
unsigned yypdnt[24] = {
00, 01, 02, 02, 02, 02, 02, 02, 
02, 02, 02, 02, 02, 02, 02, 02, 
02, 02, 02, 02, 02, 02, 02, 02, 
} ;
unsigned yypn[24] = {
02, 01, 03, 02, 02, 02, 03, 03, 
03, 03, 03, 03, 03, 03, 03, 03, 
03, 03, 03, 03, 05, 07, 01, 01, 
} ;
unsigned yypgo[3] = {
00, 00, 02, 
} ;
unsigned yygo[46] = {
0176030, 010, 03, 012, 04, 013, 05, 014, 
06, 015, 07, 016, 020, 040, 021, 041, 
022, 042, 023, 043, 024, 044, 025, 045, 
026, 046, 027, 047, 030, 050, 031, 051, 
032, 052, 033, 053, 034, 054, 035, 055, 
037, 056, 057, 061, 0176030, 011, 
} ;
unsigned yypa[51] = {
00, 016, 020, 00, 00, 00, 00, 00, 
022, 026, 064, 066, 070, 072, 0132, 0172, 
00, 00, 00, 00, 00, 00, 00, 00, 
00, 00, 00, 00, 00, 00, 0176, 00, 
0200, 0234, 0266, 0304, 0322, 0340, 0356, 0374, 
0412, 0424, 0436, 0442, 0446, 0452, 0454, 00, 
0516, 0520, 0560, 
} ;
unsigned yyact[370] = {
02, 0400, 03, 055, 04, 041, 05, 0407, 
06, 050, 07, 0173, 01, 0176030, 020027, 0176030, 
020026, 0176030, 017, 0177777, 060000, 0176030, 020, 0174, 
021, 046, 022, 074, 023, 076, 024, 0402, 
025, 0403, 026, 0404, 027, 0405, 030, 053, 
031, 055, 032, 052, 033, 057, 034, 045, 
035, 072, 020001, 0176030, 020005, 0176030, 020004, 0176030, 
020003, 0176030, 020, 0174, 021, 046, 022, 074, 
023, 076, 024, 0402, 025, 0403, 026, 0404, 
027, 0405, 030, 053, 031, 055, 032, 052, 
033, 057, 034, 045, 035, 072, 036, 051, 
060000, 0176030, 020, 0174, 021, 046, 022, 074, 
023, 076, 024, 0402, 025, 0403, 026, 0404, 
027, 0405, 030, 053, 031, 055, 032, 052, 
033, 057, 034, 045, 035, 072, 037, 054, 
060000, 0176030, 040000, 0177777, 060000, 0176030, 020002, 0176030, 
021, 046, 022, 074, 023, 076, 024, 0402, 
025, 0403, 026, 0404, 027, 0405, 030, 053, 
031, 055, 032, 052, 033, 057, 034, 045, 
035, 072, 020023, 0176030, 022, 074, 023, 076, 
024, 0402, 025, 0403, 026, 0404, 027, 0405, 
030, 053, 031, 055, 032, 052, 033, 057, 
034, 045, 035, 072, 020022, 0176030, 030, 053, 
031, 055, 032, 052, 033, 057, 034, 045, 
035, 072, 020014, 0176030, 030, 053, 031, 055, 
032, 052, 033, 057, 034, 045, 035, 072, 
020015, 0176030, 030, 053, 031, 055, 032, 052, 
033, 057, 034, 045, 035, 072, 020016, 0176030, 
030, 053, 031, 055, 032, 052, 033, 057, 
034, 045, 035, 072, 020017, 0176030, 030, 053, 
031, 055, 032, 052, 033, 057, 034, 045, 
035, 072, 020020, 0176030, 030, 053, 031, 055, 
032, 052, 033, 057, 034, 045, 035, 072, 
020021, 0176030, 032, 052, 033, 057, 034, 045, 
035, 072, 020012, 0176030, 032, 052, 033, 057, 
034, 045, 035, 072, 020013, 0176030, 035, 072, 
020007, 0176030, 035, 072, 020010, 0176030, 035, 072, 
020011, 0176030, 020006, 0176030, 020, 0174, 021, 046, 
022, 074, 023, 076, 024, 0402, 025, 0403, 
026, 0404, 027, 0405, 030, 053, 031, 055, 
032, 052, 033, 057, 034, 045, 035, 072, 
057, 054, 060, 0175, 060000, 0176030, 020024, 0176030, 
020, 0174, 021, 046, 022, 074, 023, 076, 
024, 0402, 025, 0403, 026, 0404, 027, 0405, 
030, 053, 031, 055, 032, 052, 033, 057, 
034, 045, 035, 072, 062, 0175, 060000, 0176030, 
020025, 0176030, 
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

#line 92 "/usr/src/cmd/expr.y"
 result = yypvt[0].str; exstat = true(result) ? 0 : 1; }break;

case 2: {

#line 96 "/usr/src/cmd/expr.y"
 yyval.str = yypvt[-1].str; }break;

case 3: {

#line 98 "/usr/src/cmd/expr.y"
 yyval.str = ltoa((long)strlen(yypvt[0].str)); }break;

case 4: {

#line 99 "/usr/src/cmd/expr.y"
 yyval.str = true(yypvt[0].str) ? s0 : s1; }break;

case 5: {

#line 101 "/usr/src/cmd/expr.y"
 if (isnum(yypvt[0].str))
				yyval.str = ltoa(-atol(yypvt[0].str));
			else {
				--avx;
				yyerror();
			}
			}break;

case 6: {

#line 109 "/usr/src/cmd/expr.y"
 yyval.str = regexp(yypvt[-2].str, yypvt[0].str); }break;

case 7: {

#line 111 "/usr/src/cmd/expr.y"
 yyval.str = arithop('*', yypvt[-2].str, yypvt[0].str); }break;

case 8: {

#line 112 "/usr/src/cmd/expr.y"
 yyval.str = arithop('/', yypvt[-2].str, yypvt[0].str); }break;

case 9: {

#line 113 "/usr/src/cmd/expr.y"
 yyval.str = arithop('%', yypvt[-2].str, yypvt[0].str); }break;

case 10: {

#line 114 "/usr/src/cmd/expr.y"
 yyval.str = arithop('+', yypvt[-2].str, yypvt[0].str); }break;

case 11: {

#line 115 "/usr/src/cmd/expr.y"
 yyval.str = arithop('-', yypvt[-2].str, yypvt[0].str); }break;

case 12: {

#line 117 "/usr/src/cmd/expr.y"
 yyval.str = relop('<', yypvt[-2].str, yypvt[0].str); }break;

case 13: {

#line 118 "/usr/src/cmd/expr.y"
 yyval.str = relop('>', yypvt[-2].str, yypvt[0].str); }break;

case 14: {

#line 119 "/usr/src/cmd/expr.y"
 yyval.str = relop(LE, yypvt[-2].str, yypvt[0].str); }break;

case 15: {

#line 120 "/usr/src/cmd/expr.y"
 yyval.str = relop(GE, yypvt[-2].str, yypvt[0].str); }break;

case 16: {

#line 121 "/usr/src/cmd/expr.y"
 yyval.str = relop(EQ, yypvt[-2].str, yypvt[0].str); }break;

case 17: {

#line 122 "/usr/src/cmd/expr.y"
 yyval.str = relop(NEQ, yypvt[-2].str, yypvt[0].str); }break;

case 18: {

#line 124 "/usr/src/cmd/expr.y"
 yyval.str = true(yypvt[0].str) ? yypvt[-2].str : s0; }break;

case 19: {

#line 125 "/usr/src/cmd/expr.y"
 yyval.str = true(yypvt[-2].str) ? yypvt[-2].str : yypvt[0].str; }break;

case 20: {

#line 127 "/usr/src/cmd/expr.y"
 yyval.str = true(yypvt[-3].str) ? yypvt[-1].str : s0; }break;

case 21: {

#line 128 "/usr/src/cmd/expr.y"
 yyval.str = true(yypvt[-5].str) ? yypvt[-3].str : yypvt[-1].str; }break;

case 22: {

#line 130 "/usr/src/cmd/expr.y"
 yyval.str = yypvt[0].str; }break;

case 23: {

#line 131 "/usr/src/cmd/expr.y"
 yyerror(); }break;

		}
		ip = &yygo[ yypgo[yypdnt[pno]] ];
		while( *ip!=*yys && *ip!=YYNOCHAR )
			ip += 2;
		yystate = ip[1];
		goto stack;
	}
}




