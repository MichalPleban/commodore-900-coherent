/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#line 1 "sh.y"

/*
 * The Bourne shell.
 * This shell is dedicated to Ciaran Gerald Aidan O'Donnell.
 * May he live a thousand minutes.  (long enough to fix up YACC).
 * It is also dedicated to Steve Bourne.
 * May he live a thousand seconds.
 */
#include "sh.h"

#define YYERROR	{yyerrflag=1; goto YYerract; }

extern	NODE	*node();

#include "y.tab.h"
#define YYCLEARIN yychar = -1000
#define YYERROK yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yyval, yylval;

#line 319 "sh.y"

/*
 * Create a node.
 */
NODE *
node(type, auxp, next)
NODE *auxp, *next;
{
	register NODE *np;

	np = (NODE *) balloc(sizeof (NODE));
	np->n_type = type;
	np->n_auxp = auxp;
	np->n_next = next;
	return (np);
}

#define NBPC 8
#define NKEY 8
static char keys[NKEY] = { 0 };
static int  keyi = NKEY * NBPC;

keyflush()
{
	register char *kp;

	for (kp = keys+NKEY; kp > keys; *--kp = 0);
	keyi = NKEY * NBPC;
}

keypop()
{
	register char	*kp;
	register int	km;

	if ((km = keyi++) >= NKEY * NBPC) {
		panic();
		NOTREACHED;
	}
	kp = keys + (km / NBPC);
	km = 1 << (km %= NBPC);
	keyflag = (*kp & km) ? 1 : 0;
	*kp &= ~km;
}

keypush()
{
	register char	*kp;
	register int	km;

	if ((km = --keyi) < 0) {
		panic();
		NOTREACHED;
	}
	if (keyflag) {
		kp = keys + (km / NBPC);
		km = 1 << (km %= NBPC);
		*kp |= km;
	}
}
/*
 * The following fragments might implement named pipes.
 * The token declaration goes in the header.
 * The nopen production should go with the others of its ilk.
 * The production fragment goes into argument:
%token _NOPEN _NCLOSE
nopen:	_NOPEN optnls ;

|	nopen pipe_command ')' {
		$$ = node(NRPIPE, $2, NULL);
	}
|	oparen pipe_command _NCLOSE {
		$$ = node(NWPIPE, $2, NULL);
	}
 *
 */
#ifdef YYTNAMES
struct yytname yytnames[30] =
{
	"$end", -1, 
	"error", -2, 
	"_NULL", 256, 
	"_DSEMI", 257, 
	"_ANDF", 258, 
	"_ORF", 259, 
	"_NAME", 260, 
	"_IORS", 261, 
	"_ASGN", 262, 
	"_CASE", 263, 
	"_DO", 264, 
	"_DONE", 265, 
	"_ELIF", 266, 
	"_ELSE", 267, 
	"_ESAC", 268, 
	"_FI", 269, 
	"_FOR", 270, 
	"_IF", 271, 
	"_IN", 272, 
	"_THEN", 273, 
	"_UNTIL", 274, 
	"_WHILE", 275, 
	"_OBRAC", 276, 
	"_CBRAC", 277, 
	"'\\n'", 10, 
	"';'", 59, 
	"'|'", 124, 
	"'('", 40, 
	"')'", 41, 
	"'&'", 38, 
	NULL,
} ;
#endif
unsigned yypdnt[74] = {
00, 01, 01, 02, 02, 02, 04, 06, 
07, 010, 011, 011, 012, 012, 013, 014, 
015, 016, 017, 020, 021, 03, 03, 03, 
03, 03, 022, 022, 022, 023, 023, 024, 
025, 026, 026, 027, 027, 027, 027, 030, 
031, 032, 033, 033, 033, 033, 033, 033, 
034, 034, 043, 043, 037, 037, 037, 044, 
045, 045, 036, 036, 042, 042, 042, 041, 
041, 040, 040, 035, 035, 035, 05, 05, 
046, 046, 
} ;
unsigned yypn[74] = {
02, 02, 00, 01, 02, 02, 02, 02, 
02, 02, 02, 02, 02, 03, 02, 02, 
02, 02, 02, 02, 02, 01, 02, 02, 
03, 03, 01, 03, 03, 03, 01, 02, 
00, 02, 01, 01, 01, 01, 01, 01, 
01, 01, 06, 06, 04, 06, 03, 03, 
02, 00, 02, 00, 03, 01, 00, 03, 
03, 01, 02, 00, 05, 02, 00, 01, 
00, 03, 02, 01, 01, 02, 01, 00, 
01, 02, 
} ;
unsigned yypgo[39] = {
00, 00, 02, 04, 014, 016, 056, 062, 
064, 066, 070, 072, 074, 076, 0100, 0102, 
0104, 0106, 0110, 0116, 0122, 0124, 0126, 0132, 
0134, 0136, 0154, 0156, 0160, 0162, 0166, 0172, 
0176, 0210, 0226, 0232, 0236, 0240, 0244, 
} ;
unsigned yygo[174] = {
0176030, 01, 0176030, 05, 01, 06, 015, 051, 
016, 052, 0176030, 072, 0176030, 037, 020, 056, 
023, 061, 032, 065, 033, 066, 034, 067, 
035, 070, 036, 071, 072, 0105, 0107, 0125, 
0111, 0130, 0127, 0147, 0142, 0157, 0143, 0160, 
0152, 0165, 0154, 0167, 0176030, 054, 0161, 0171, 
0176030, 0110, 0176030, 0144, 0176030, 0145, 0176030, 040, 
0176030, 0112, 0176030, 021, 0176030, 022, 0176030, 024, 
0176030, 041, 0176030, 042, 0176030, 0155, 0176030, 0153, 
021, 057, 022, 060, 0176030, 07, 024, 062, 
0176030, 010, 0176030, 011, 0176030, 012, 044, 0100, 
0176030, 043, 0176030, 044, 0176030, 045, 012, 046, 
030, 063, 031, 064, 044, 046, 0103, 0121, 
0121, 0121, 0176030, 0134, 0176030, 047, 0176030, 050, 
0176030, 0104, 0104, 0123, 0176030, 0117, 0123, 0141, 
0176030, 0113, 0153, 0166, 0176030, 0135, 037, 073, 
040, 074, 0106, 0124, 0144, 0161, 0176030, 075, 
042, 077, 0110, 0126, 0112, 0131, 0145, 0162, 
0155, 0170, 0171, 0172, 0176030, 076, 0172, 0173, 
0176030, 0146, 0121, 0140, 0176030, 0122, 0176030, 0136, 
0150, 0164, 0176030, 0137, 072, 0106, 0102, 0120, 
0104, 0120, 0116, 0133, 0176030, 055, 
} ;
unsigned yypa[124] = {
00, 02, 034, 040, 044, 046, 050, 054, 
062, 070, 074, 0122, 0124, 0126, 0154, 0202, 
0202, 0206, 0206, 0202, 0210, 0212, 0214, 0216, 
0220, 0220, 0202, 0202, 0202, 0202, 0202, 0210, 
0210, 0224, 0230, 0234, 0236, 0264, 0266, 0270, 
0272, 0274, 0276, 0300, 0302, 0304, 0310, 0312, 
0314, 0316, 0320, 0322, 0326, 0332, 0334, 0336, 
0340, 0342, 0202, 0344, 0350, 0354, 0356, 0362, 
0366, 0370, 0372, 0400, 0372, 0404, 0406, 0202, 
0436, 0446, 0454, 0460, 0464, 0466, 0470, 0474, 
0500, 0504, 0510, 0350, 0512, 0514, 0516, 0202, 
0524, 0526, 0530, 0532, 0536, 0542, 0546, 0552, 
0556, 0560, 0202, 0202, 0210, 0564, 0570, 0574, 
0576, 0602, 0202, 0474, 0202, 0604, 0612, 0614, 
0616, 0344, 0620, 0622, 0624, 0626, 0630, 0632, 
0634, 0436, 0636, 0644, 
} ;
unsigned yyact[422] = {
020002, 0176030, 02, 0177777, 04, 012, 020040, 0404, 
020040, 0405, 020040, 0406, 020040, 0407, 020040, 0416, 
020040, 0417, 020040, 0422, 020040, 0423, 020040, 0424, 
020040, 050, 03, 0176030, 040000, 0177777, 060000, 0176030, 
013, 012, 060000, 0176030, 020003, 0176030, 020001, 0176030, 
014, 012, 060000, 0176030, 015, 073, 016, 046, 
020025, 0176030, 017, 0402, 020, 0403, 020032, 0176030, 
023, 0174, 020036, 0176030, 025, 0404, 026, 0405, 
027, 0406, 030, 0407, 031, 0416, 032, 0417, 
033, 0422, 034, 0423, 035, 0424, 036, 050, 
060000, 0176030, 020005, 0176030, 020004, 0176030, 020040, 0404, 
020040, 0405, 020040, 0406, 020040, 0407, 020040, 0416, 
020040, 0417, 020040, 0422, 020040, 0423, 020040, 0424, 
020040, 050, 020027, 0176030, 020040, 0404, 020040, 0405, 
020040, 0406, 020040, 0407, 020040, 0416, 020040, 0417, 
020040, 0422, 020040, 0423, 020040, 0424, 020040, 050, 
020026, 0176030, 053, 012, 020107, 0176030, 020040, 0176030, 
020040, 0176030, 020050, 0176030, 020047, 0176030, 020051, 0176030, 
025, 0404, 060000, 0176030, 020100, 051, 020040, 0176030, 
020100, 0425, 020040, 0176030, 020037, 0176030, 025, 0404, 
026, 0405, 027, 0406, 030, 0407, 031, 0416, 
032, 0417, 033, 0422, 034, 0423, 035, 0424, 
036, 050, 020042, 0176030, 020043, 0176030, 020044, 0176030, 
020045, 0176030, 020046, 0176030, 020031, 0176030, 020030, 0176030, 
020110, 0176030, 020017, 0176030, 0101, 012, 020106, 0176030, 
020016, 0176030, 020033, 0176030, 020034, 0176030, 020020, 0176030, 
020035, 0176030, 0102, 0420, 060000, 0176030, 0103, 0420, 
020061, 0176030, 020006, 0176030, 020013, 0176030, 020012, 0176030, 
020022, 0176030, 020021, 0176030, 0107, 0421, 060000, 0176030, 
0111, 0410, 020073, 0176030, 020077, 0176030, 0114, 051, 
060000, 0176030, 0115, 0425, 060000, 0176030, 020041, 0176030, 
020111, 0176030, 053, 012, 0116, 073, 060000, 0176030, 
025, 0404, 020063, 0176030, 020102, 0176030, 0101, 012, 
020106, 0401, 020106, 0410, 020106, 0411, 020106, 0412, 
020106, 0413, 020106, 0414, 020106, 0415, 020106, 0421, 
020106, 0425, 020106, 051, 020040, 0176030, 020100, 0412, 
020100, 0413, 020100, 0415, 020040, 0176030, 053, 012, 
0127, 073, 020107, 0176030, 020100, 0411, 020040, 0176030, 
0132, 0411, 060000, 0176030, 020056, 0176030, 020057, 0176030, 
053, 012, 020104, 0176030, 025, 0404, 020066, 0176030, 
0101, 012, 020103, 0176030, 025, 0404, 020063, 0176030, 
020060, 0176030, 020101, 0176030, 020007, 0176030, 0142, 0412, 
0143, 0413, 020076, 0176030, 020014, 0176030, 020072, 0176030, 
020054, 0176030, 0101, 012, 020105, 0176030, 0150, 0174, 
020071, 0176030, 0151, 0414, 060000, 0176030, 0152, 0401, 
020065, 0176030, 0154, 051, 060000, 0176030, 020062, 0176030, 
0156, 0411, 060000, 0176030, 020100, 0415, 020040, 0176030, 
0163, 0415, 060000, 0176030, 020015, 0176030, 025, 0404, 
060000, 0176030, 020053, 0176030, 020100, 0401, 020100, 0414, 
020040, 0176030, 020052, 0176030, 020010, 0176030, 020011, 0176030, 
020075, 0176030, 020055, 0176030, 020070, 0176030, 020024, 0176030, 
020064, 0176030, 020023, 0176030, 020067, 0176030, 0142, 0412, 
0143, 0413, 020076, 0176030, 020074, 0176030, 
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

case 3: {

#line 46 "sh.y"

		sesp->s_node = NULL;
		reset(RCMD);
		NOTREACHED;
	}break;

case 4: {

#line 52 "sh.y"

		sesp->s_node = yypvt[-1].yu_node;
		reset(errflag ? RERR : RCMD);
		NOTREACHED;
	}break;

case 5: {

#line 57 "sh.y"

		keyflush();
		keyflag = 1;
		reset(RERR);
		NOTREACHED;
	}break;

case 10: {

#line 73 "sh.y"
	yyval.yu_nval = NWHILE;	}break;

case 11: {

#line 74 "sh.y"
	yyval.yu_nval = NUNTIL;	}break;

case 21: {

#line 94 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 22: {

#line 97 "sh.y"

		yyval.yu_node = node(NBACK, yypvt[-1].yu_node, NULL);
	}break;

case 23: {

#line 100 "sh.y"

		yyval.yu_node = yypvt[-1].yu_node;
	}break;

case 24: {

#line 103 "sh.y"

		yyval.yu_node = node(NBACK, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 25: {

#line 106 "sh.y"

		yyval.yu_node = node(NLIST, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 26: {

#line 112 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 27: {

#line 115 "sh.y"

		yyval.yu_node = node(NORF, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 28: {

#line 118 "sh.y"

		yyval.yu_node = node(NANDF, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 29: {

#line 124 "sh.y"

		yyval.yu_node = node(NPIPE, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 30: {

#line 127 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 31: {

#line 133 "sh.y"

		yyval.yu_node = node(NCOMS, yypvt[0].yu_node, NULL);
		keypop();
	}break;

case 32: {

#line 140 "sh.y"

		keypush();
		keyflag = 1;
	}break;

case 33: {

#line 147 "sh.y"

		if ((yypvt[-1].yu_node->n_type == NCTRL && yypvt[0].yu_node->n_type == NARGS)
		 || (yypvt[-1].yu_node->n_type == NARGS && yypvt[0].yu_node->n_type == NCTRL)) {
			YYERROR;
		}
		(yyval.yu_node = yypvt[-1].yu_node)->n_next = yypvt[0].yu_node;
	}break;

case 34: {

#line 154 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 35: {

#line 160 "sh.y"

		yyval.yu_node = node(NIORS, yypvt[0].yu_strp, NULL);
	}break;

case 36: {

#line 163 "sh.y"

		yyval.yu_node = node(NARGS, yypvt[0].yu_strp, NULL);
		keyflag = 0;
	}break;

case 37: {

#line 167 "sh.y"

		yyval.yu_node = node(NASSG, yypvt[0].yu_strp, NULL);
	}break;

case 38: {

#line 170 "sh.y"

		if ( ! keyflag) {
			YYERROR;
		}
		yyval.yu_node = node(NCTRL, yypvt[0].yu_node, NULL);
		keyflag = 0;
	}break;

case 39: {

#line 179 "sh.y"

		yyval.yu_strp = duplstr(strt, 0);
	}break;

case 40: {

#line 184 "sh.y"

		yyval.yu_strp = duplstr(strt, 0);
	}break;

case 41: {

#line 189 "sh.y"

		yyval.yu_strp = duplstr(strt, 0);
	}break;

case 42: {

#line 195 "sh.y"

		yyval.yu_node = node(NFOR, yypvt[-4].yu_strp, node(NFOR2, yypvt[-3].yu_node, node(NLIST, yypvt[-1].yu_node, NULL)));
		yyval.yu_node->n_next->n_next->n_next = yyval.yu_node->n_next;
	}break;

case 43: {

#line 199 "sh.y"

		yyval.yu_node = node(NCASE, yypvt[-4].yu_strp, yypvt[-1].yu_node);
	}break;

case 44: {

#line 202 "sh.y"

		yyval.yu_node = node(yypvt[-3].yu_nval, yypvt[-2].yu_node, node(NLIST, yypvt[-1].yu_node, NULL));
		yyval.yu_node->n_next->n_next = yyval.yu_node;
	}break;

case 45: {

#line 206 "sh.y"

		yyval.yu_node = node(NIF, node(NNULL, yypvt[-4].yu_node, yypvt[-2].yu_node), yypvt[-1].yu_node);
	}break;

case 46: {

#line 209 "sh.y"

		yyval.yu_node = node(NPARN, yypvt[-1].yu_node, NULL);
	}break;

case 47: {

#line 212 "sh.y"

		yyval.yu_node = node(NBRAC, yypvt[-1].yu_node, NULL);
	}break;

case 48: {

#line 218 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 49: {

#line 221 "sh.y"

		yyval.yu_node = node(NARGS, "\"$@\"", NULL);
	}break;

case 50: {

#line 227 "sh.y"

		yyval.yu_node = node(NARGS, yypvt[-1].yu_strp, yypvt[0].yu_node);
	}break;

case 51: {

#line 230 "sh.y"

		yyval.yu_node = NULL;
	}break;

case 52: {

#line 236 "sh.y"

		register NODE *np;

		for (np=yypvt[-2].yu_node; np->n_next; np=np->n_next);
		np->n_next = yypvt[0].yu_node;
		yyval.yu_node = yypvt[-2].yu_node;
	}break;

case 53: {

#line 243 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 54: {

#line 246 "sh.y"

		yyval.yu_node = NULL;
	}break;

case 55: {

#line 252 "sh.y"

		yyval.yu_node = node(NCASE2, yypvt[0].yu_node, yypvt[-2].yu_node);
	}break;

case 56: {

#line 258 "sh.y"

		yyval.yu_node = node(NCASE3, yypvt[-2].yu_strp, yypvt[0].yu_node);
	}break;

case 57: {

#line 261 "sh.y"

		yyval.yu_node = node(NCASE3, yypvt[0].yu_strp, NULL);
	}break;

case 58: {

#line 267 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 59: {

#line 270 "sh.y"

		yyval.yu_node = NULL;
	}break;

case 60: {

#line 276 "sh.y"

		yyval.yu_node = node(NIF, node(NULL, yypvt[-3].yu_node, yypvt[-1].yu_node), yypvt[0].yu_node);
	}break;

case 61: {

#line 279 "sh.y"

		yyval.yu_node = node(NELSE, yypvt[0].yu_node, NULL);
	}break;

case 62: {

#line 282 "sh.y"

		yyval.yu_node = NULL;
	}break;

case 63: {

#line 288 "sh.y"

		yyval.yu_node = yypvt[0].yu_node;
	}break;

case 64: {

#line 292 "sh.y"

		yyval.yu_node = NULL;
	}break;

case 65: {

#line 298 "sh.y"

		yyval.yu_node = node(NLIST, yypvt[-2].yu_node, yypvt[0].yu_node);
	}break;

case 66: {

#line 301 "sh.y"

		yyval.yu_node = yypvt[-1].yu_node;
	}break;

		}
		ip = &yygo[ yypgo[yypdnt[pno]] ];
		while( *ip!=*yys && *ip!=YYNOCHAR )
			ip += 2;
		yystate = ip[1];
		goto stack;
	}
}




