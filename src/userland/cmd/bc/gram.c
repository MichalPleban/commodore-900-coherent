
#line 4 "gram.y"

#include <stdio.h>
#include "bc.h"
#define	YYTNAMES


static code	*breakloc	= NULL,	/* where to go on break statement */
		*contloc	= NULL;	/* where to go on continue statement */
static dicent	*retfrom	= NULL,	/* what function to return from */
		**dvec,			/* list of locals */
		**pardvec,		/* list of formal parameters */
		**autdvec;		/* list of automatic variables */
static int	ldvec,			/* length of dvec */
		lpardvec,		/* length of pardvec */
		lautdvec;		/* length of autdvec */

#include "yy.h"
#define YYCLEARIN yychar = -1000
#define YYERROK yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yyval, yylval;

#line 672 "gram.y"



/*
 *	Yyerror is the error routine called on a syntax error by
 *	yyparse.
 */

yyerror(m)
char *m;
{
	register struct yytname *ptr;

	fprintf(stderr,"%s", m);
	for (ptr = yytnames; ptr->tn_name != NULL; ++ptr)
		if (ptr->tn_val == yychar) {
			fprintf(stderr," at %s", ptr->tn_name);
			break;
		}
	fprintf(stderr,"\n");
	allok = FALSE;
}
#ifdef YYTNAMES
struct yytname yytnames[] =
{
	"$end", -1, 
	"error", -2, 
	"NUMBER", 256, 
	"STRING", 257, 
	"IDENTIFIER", 258, 
	"ADDAB", 259, 
	"AUTO", 260, 
	"BREAK", 261, 
	"CONTINUE", 262, 
	"DECR", 307, 
	"DEFINE", 264, 
	"DIVAB", 265, 
	"DO", 266, 
	"DOT", 267, 
	"ELSE", 268, 
	"EQP", 269, 
	"ERROR", 270, 
	"EXPAB", 271, 
	"FOR", 272, 
	"GEP", 273, 
	"GTP", 274, 
	"IBASE", 275, 
	"IF", 276, 
	"INCR", 306, 
	"LENGTH_", 278, 
	"LEP", 279, 
	"LTP", 280, 
	"MULAB", 281, 
	"NEP", 282, 
	"OBASE", 283, 
	"QUIT", 284, 
	"REMAB", 285, 
	"RETURN_", 286, 
	"SCALE_", 287, 
	"SQRT_", 288, 
	"SUBAB", 289, 
	"WHILE", 290, 
	"'='", 61, 
	"'+'", 43, 
	"'-'", 45, 
	"'*'", 42, 
	"'/'", 47, 
	"'%'", 37, 
	"'^'", 94, 
	"UMINUS", 308, 
	"'\\n'", 10, 
	"'('", 40, 
	"')'", 41, 
	"';'", 59, 
	"'$'", 36, 
	"'{'", 123, 
	"'}'", 125, 
	"','", 44, 
	"'['", 91, 
	"']'", 93, 
	NULL,
} ;
#endif
unsigned yypdnt[109] = {
00, 01, 01, 01, 01, 02, 02, 02, 
02, 02, 02, 02, 02, 02, 02, 02, 
02, 02, 02, 02, 02, 014, 014, 020, 
020, 05, 05, 013, 013, 021, 021, 011, 
012, 010, 06, 03, 022, 023, 023, 025, 
025, 026, 026, 027, 027, 024, 024, 015, 
015, 016, 016, 016, 016, 017, 017, 017, 
017, 017, 017, 017, 017, 017, 017, 017, 
017, 017, 017, 017, 017, 017, 017, 017, 
017, 017, 017, 030, 030, 037, 037, 035, 
035, 040, 040, 031, 033, 033, 033, 034, 
034, 034, 032, 032, 032, 032, 032, 032, 
036, 036, 036, 07, 07, 04, 04, 041, 
041, 041, 041, 041, 041, 
} ;
unsigned yypn[109] = {
02, 00, 02, 02, 03, 06, 012, 020, 
016, 02, 02, 02, 03, 02, 02, 03, 
02, 03, 03, 02, 01, 01, 01, 00, 
02, 00, 04, 00, 01, 01, 03, 00, 
00, 00, 00, 012, 02, 00, 01, 00, 
03, 01, 03, 01, 03, 00, 01, 01, 
01, 03, 04, 03, 03, 01, 01, 01, 
01, 04, 02, 02, 02, 02, 02, 02, 
02, 02, 02, 03, 03, 03, 03, 03, 
03, 03, 04, 01, 04, 01, 03, 00, 
01, 01, 03, 00, 01, 01, 01, 01, 
01, 01, 01, 01, 01, 01, 01, 01, 
01, 01, 01, 00, 03, 00, 03, 01, 
01, 01, 01, 01, 01, 
} ;
unsigned yypgo[34] = {
00, 00, 02, 020, 022, 024, 026, 034, 
042, 050, 072, 0106, 0112, 0144, 0176, 0220, 
0274, 0300, 0302, 0304, 0306, 0310, 0312, 0320, 
0322, 0362, 0364, 0370, 0372, 0432, 0434, 0436, 
0442, 0444, 
} ;
unsigned yygo[296] = {
0176030, 01, 01, 035, 0210, 0234, 0253, 0260, 
0262, 0270, 0263, 0271, 0310, 0311, 0176030, 0160, 
0176030, 036, 0176030, 0152, 0176030, 0247, 0205, 0231, 
0267, 0275, 0176030, 077, 0232, 0245, 0301, 0303, 
0176030, 0220, 0236, 0250, 0254, 0261, 0176030, 0146, 
0146, 0205, 0231, 0244, 0246, 0255, 0250, 0256, 
0261, 0267, 0271, 0276, 0304, 0306, 0311, 0312, 
0176030, 065, 0244, 0253, 0255, 0262, 0256, 0263, 
0274, 0301, 0306, 0310, 0176030, 0232, 0275, 0302, 
0176030, 0147, 05, 051, 07, 054, 010, 055, 
023, 072, 024, 073, 040, 0106, 041, 0116, 
050, 0137, 074, 0154, 0115, 0167, 0272, 0277, 
0305, 0307, 0176030, 037, 024, 074, 032, 0104, 
052, 0141, 066, 0150, 0122, 0174, 0124, 0176, 
0135, 0177, 0136, 0200, 0175, 0225, 0203, 0141, 
0207, 0233, 0275, 0150, 0176030, 0145, 01, 040, 
0105, 040, 0210, 040, 0253, 040, 0262, 040, 
0263, 040, 0273, 040, 0310, 040, 0176030, 075, 
01, 041, 030, 0101, 067, 0153, 0105, 041, 
0107, 0161, 0110, 0162, 0111, 0163, 0112, 0164, 
0113, 0165, 0114, 0166, 0155, 0221, 0210, 041, 
0217, 0235, 0232, 0221, 0237, 0251, 0253, 041, 
0262, 041, 0263, 041, 0273, 041, 0301, 0221, 
0310, 041, 0176030, 076, 0265, 0273, 0176030, 0105, 
0176030, 0151, 0176030, 042, 0176030, 0171, 0176030, 0242, 
0176030, 0265, 0224, 0243, 0264, 0272, 0176030, 0172, 
0176030, 0173, 011, 062, 020, 070, 030, 0102, 
067, 0102, 0107, 0102, 0110, 0102, 0111, 0102, 
0112, 0102, 0113, 0102, 0114, 0102, 0155, 0102, 
0217, 0102, 0232, 0102, 0237, 0102, 0301, 0102, 
0176030, 043, 0176030, 0123, 0123, 0175, 0176030, 0135, 
0176030, 044, 011, 063, 020, 071, 030, 0103, 
067, 0103, 0107, 0103, 0110, 0103, 0111, 0103, 
0112, 0103, 0113, 0103, 0114, 0103, 0155, 0103, 
0217, 0103, 0232, 0103, 0237, 0103, 0301, 0103, 
0176030, 045, 0176030, 0142, 0176030, 046, 0203, 0230, 
0176030, 0143, 0176030, 0144, 0221, 0237, 0176030, 0217, 
} ;
unsigned yypa[203] = {
00, 02, 070, 074, 0100, 0102, 0112, 0120, 
0120, 0126, 0140, 0144, 0146, 0150, 0154, 0160, 
0126, 0164, 0166, 0120, 0172, 0230, 0236, 0240, 
0242, 0274, 0276, 0330, 0332, 0334, 0336, 0340, 
0120, 0342, 0366, 0372, 0416, 0422, 0444, 0450, 
0120, 0452, 0454, 0276, 0506, 0510, 0512, 0516, 
0520, 0522, 0524, 0526, 0530, 0532, 0534, 0566, 
0620, 0622, 0624, 0626, 0120, 0630, 0632, 0650, 
0654, 0660, 0662, 0670, 0676, 0702, 0766, 0242, 
0242, 0242, 0242, 0242, 0242, 0120, 0770, 0772, 
0776, 01000, 0276, 01002, 0276, 01020, 01022, 01024, 
01026, 01030, 01032, 01034, 01036, 0276, 0276, 01040, 
01042, 01050, 01052, 01056, 01060, 01064, 0144, 01070, 
01074, 01076, 01102, 01106, 01140, 01142, 01174, 01176, 
01200, 01202, 01214, 01226, 01232, 01236, 01242, 01246, 
01250, 01254, 01260, 01262, 01266, 0276, 01270, 01272, 
01274, 01300, 01334, 01336, 01370, 0240, 01372, 0276, 
01374, 01456, 01460, 01462, 01464, 01466, 01470, 0242, 
01472, 01106, 01476, 01502, 01506, 01512, 01514, 01516, 
01520, 0144, 01142, 01522, 01524, 01530, 0532, 0242, 
01546, 01550, 01552, 01556, 01560, 01562, 0144, 01566, 
0144, 01570, 01606, 01374, 0532, 01560, 01560, 01612, 
01616, 0144, 01374, 01374, 01506, 0332, 01622, 0240, 
01626, 0144, 0120, 01630, 01560, 0534, 01714, 01716, 
01720, 01142, 01722, 01726, 0144, 0120, 01560, 01732, 
01374, 0144, 01734, 
} ;
unsigned yyact[990] = {
020001, 0176030, 02, 0177777, 04, 0400, 05, 0401, 
06, 0402, 07, 0405, 010, 0406, 011, 0463, 
012, 0410, 013, 0412, 014, 0413, 015, 0420, 
016, 0423, 017, 0424, 020, 0462, 021, 0426, 
022, 0433, 023, 0434, 024, 0436, 025, 0437, 
026, 0440, 027, 0442, 030, 055, 031, 012, 
032, 050, 033, 073, 034, 0173, 03, 0176030, 
040000, 0177777, 060000, 0176030, 047, 012, 060000, 0176030, 
020065, 0176030, 031, 012, 033, 073, 050, 044, 
060000, 0176030, 052, 050, 053, 0133, 020113, 0176030, 
031, 012, 033, 073, 060000, 0176030, 056, 0402, 
057, 0423, 060, 0433, 061, 0437, 060000, 0176030, 
064, 0402, 060000, 0176030, 020037, 0176030, 020066, 0176030, 
066, 050, 060000, 0176030, 020124, 075, 020127, 0176030, 
067, 050, 060000, 0176030, 020141, 0176030, 020125, 075, 
020130, 0176030, 04, 0400, 06, 0402, 011, 0463, 
014, 0413, 016, 0423, 020, 0462, 021, 0426, 
022, 0433, 025, 0437, 026, 0440, 030, 055, 
031, 012, 032, 050, 033, 073, 060000, 0176030, 
020126, 075, 020142, 050, 020131, 0176030, 020140, 0176030, 
020042, 0176030, 04, 0400, 06, 0402, 011, 0463, 
014, 0413, 057, 0423, 020, 0462, 021, 0426, 
060, 0433, 0100, 0437, 026, 0440, 030, 055, 
032, 050, 060000, 0176030, 020026, 0176030, 04, 0400, 
06, 0402, 011, 0463, 014, 0413, 016, 0423, 
020, 0462, 021, 0426, 022, 0433, 025, 0437, 
026, 0440, 030, 055, 032, 050, 060000, 0176030, 
020025, 0176030, 020027, 0176030, 020002, 0176030, 020003, 0176030, 
020024, 0176030, 0107, 053, 0110, 055, 0111, 052, 
0112, 057, 0113, 045, 0114, 0136, 031, 012, 
033, 073, 0115, 044, 060000, 0176030, 0117, 050, 
060000, 0176030, 0120, 0463, 0121, 0462, 0122, 075, 
020123, 0403, 020123, 0411, 020123, 0417, 020123, 0431, 
020123, 0435, 020123, 0441, 020067, 0176030, 0124, 075, 
060000, 0176030, 0125, 0403, 0126, 0463, 0127, 0411, 
0130, 0417, 0131, 0462, 0132, 0431, 0133, 0435, 
0134, 0441, 020070, 0176030, 0136, 050, 060000, 0176030, 
020004, 0176030, 020020, 0176030, 04, 0400, 0140, 0402, 
011, 0463, 014, 0413, 016, 0423, 020, 0462, 
021, 0426, 022, 0433, 025, 0437, 026, 0440, 
030, 055, 032, 050, 020117, 0176030, 020011, 0176030, 
020012, 0176030, 053, 0133, 020113, 0176030, 020127, 0176030, 
020130, 0176030, 020131, 0176030, 020074, 0176030, 020075, 0176030, 
020044, 0176030, 020041, 0176030, 04, 0400, 06, 0402, 
011, 0463, 014, 0413, 016, 0423, 020, 0462, 
021, 0426, 022, 0433, 025, 0437, 026, 0440, 
030, 055, 032, 050, 020033, 0176030, 04, 0400, 
06, 0402, 011, 0463, 014, 0413, 057, 0423, 
020, 0462, 021, 0426, 060, 0433, 0100, 0437, 
026, 0440, 030, 055, 032, 050, 020145, 0176030, 
020072, 0176030, 020073, 0176030, 020023, 0176030, 020013, 0176030, 
020057, 0176030, 0107, 053, 0110, 055, 0111, 052, 
0112, 057, 0113, 045, 0114, 0136, 020060, 0176030, 
0155, 050, 060000, 0176030, 020142, 050, 020131, 0176030, 
020102, 0176030, 0120, 0463, 0121, 0462, 020067, 0176030, 
0126, 0463, 0131, 0462, 020070, 0176030, 0156, 051, 
060000, 0176030, 04, 0400, 05, 0401, 06, 0402, 
07, 0405, 010, 0406, 011, 0463, 013, 0412, 
014, 0413, 015, 0420, 016, 0423, 017, 0424, 
020, 0462, 021, 0426, 022, 0433, 023, 0434, 
024, 0436, 025, 0437, 026, 0440, 027, 0442, 
030, 055, 031, 012, 032, 050, 033, 073, 
034, 0173, 0157, 0175, 060000, 0176030, 020015, 0176030, 
020016, 0176030, 0170, 0402, 020045, 0176030, 020100, 0176030, 
020076, 0176030, 0125, 0403, 0127, 0411, 0130, 0417, 
0132, 0431, 0133, 0435, 0134, 0441, 060000, 0176030, 
020132, 0176030, 020101, 0176030, 020135, 0176030, 020137, 0176030, 
020077, 0176030, 020134, 0176030, 020136, 0176030, 020133, 0176030, 
020021, 0176030, 052, 050, 0201, 0133, 020113, 0176030, 
020115, 0176030, 0202, 051, 060000, 0176030, 020121, 0176030, 
0203, 054, 020120, 0176030, 0204, 0135, 060000, 0176030, 
0206, 073, 060000, 0176030, 020035, 0176030, 0207, 054, 
020034, 0176030, 0210, 051, 060000, 0176030, 0211, 0415, 
0212, 0421, 0213, 0422, 0214, 0427, 0215, 0430, 
0216, 0432, 0107, 053, 0110, 055, 0111, 052, 
0112, 057, 0113, 045, 0114, 0136, 060000, 0176030, 
020014, 0176030, 04, 0400, 06, 0402, 011, 0463, 
014, 0413, 057, 0423, 020, 0462, 021, 0426, 
060, 0433, 0100, 0437, 026, 0440, 030, 055, 
032, 050, 020143, 0176030, 020111, 0176030, 020022, 0176030, 
020030, 0176030, 0111, 052, 0112, 057, 0113, 045, 
0114, 0136, 020107, 0176030, 0111, 052, 0112, 057, 
0113, 045, 0114, 0136, 020110, 0176030, 0114, 0136, 
020104, 0176030, 0114, 0136, 020105, 0176030, 0114, 0136, 
020106, 0176030, 0114, 0136, 020103, 0176030, 020017, 0176030, 
0222, 0133, 020053, 0176030, 0223, 051, 060000, 0176030, 
020046, 0176030, 0224, 054, 020051, 0176030, 020061, 0176030, 
020063, 0176030, 020064, 0176030, 0226, 051, 060000, 0176030, 
04, 0400, 06, 0402, 011, 0463, 014, 0413, 
016, 0423, 020, 0462, 021, 0426, 022, 0433, 
025, 0437, 026, 0440, 030, 055, 032, 050, 
0227, 0135, 060000, 0176030, 020071, 0176030, 04, 0400, 
0140, 0402, 011, 0463, 014, 0413, 016, 0423, 
020, 0462, 021, 0426, 022, 0433, 025, 0437, 
026, 0440, 030, 055, 032, 050, 060000, 0176030, 
020114, 0176030, 020040, 0176030, 04, 0400, 05, 0401, 
06, 0402, 07, 0405, 010, 0406, 011, 0463, 
013, 0412, 014, 0413, 015, 0420, 016, 0423, 
017, 0424, 020, 0462, 021, 0426, 022, 0433, 
023, 0434, 024, 0436, 025, 0437, 026, 0440, 
027, 0442, 030, 055, 031, 012, 032, 050, 
033, 073, 034, 0173, 060000, 0176030, 020151, 0176030, 
020152, 0176030, 020153, 0176030, 020150, 0176030, 020147, 0176030, 
020154, 0176030, 0236, 051, 060000, 0176030, 0240, 0135, 
060000, 0176030, 0241, 012, 020055, 0176030, 0170, 0402, 
060000, 0176030, 020062, 0176030, 020112, 0176030, 020116, 0176030, 
020122, 0176030, 020036, 0176030, 0246, 0414, 020031, 0176030, 
0107, 053, 0110, 055, 0111, 052, 0112, 057, 
0113, 045, 0114, 0136, 020146, 0176030, 020054, 0176030, 
020056, 0176030, 0252, 0173, 060000, 0176030, 020052, 0176030, 
020040, 0176030, 0254, 073, 060000, 0176030, 020005, 0176030, 
0107, 053, 0110, 055, 0111, 052, 0112, 057, 
0113, 045, 0114, 0136, 020144, 0176030, 0257, 012, 
060000, 0176030, 0264, 0404, 020047, 0176030, 0266, 0442, 
060000, 0176030, 0274, 050, 060000, 0176030, 020032, 0176030, 
04, 0400, 05, 0401, 06, 0402, 07, 0405, 
010, 0406, 011, 0463, 013, 0412, 014, 0413, 
015, 0420, 016, 0423, 017, 0424, 020, 0462, 
021, 0426, 022, 0433, 023, 0434, 024, 0436, 
025, 0437, 026, 0440, 027, 0442, 030, 055, 
031, 012, 032, 050, 033, 073, 034, 0173, 
0300, 0175, 060000, 0176030, 020006, 0176030, 020050, 0176030, 
020043, 0176030, 0304, 051, 060000, 0176030, 0305, 051, 
060000, 0176030, 020010, 0176030, 020007, 0176030, 
} ;
#include "action.h"
#define YYNOCHAR (-1000)
#define	yyerrok	yyerrflag=0
#define	yyclearin	yylval=YYNOCHAR
int yystack[YYMAXDEPTH];
YYSTYPE yyvstack[YYMAXDEPTH], *yyv;
int yychar;
int yydebug = 0;	/* No sir, not in the BSS */
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
		fprintf(stderr, "Stack overflow\n");
		exit(1);
	}
	*yys = yystate;
	*++yyv = yyval;
	if( yydebug )
		fprintf(stdout, "Stack state %d, char %d\n", yystate, yychar);

read:
	ip = &yyact[yypa[yystate]];
	if( ip[1] != YYNOCHAR ) {
		if( yychar == YYNOCHAR ) {
			yychar = yylex();
			if( yydebug )
				fprintf(stdout, "lex read char %d, val %d\n", yychar, yylval);
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
		if( yydebug )
			fprintf(stdout, "shift %d\n", yystate);
		if( yyerrflag )
			--yyerrflag;
		goto stack;

	case YYACCEPTACT:
		if( yydebug )
			fprintf(stdout, "accept\n");
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
				if( yydebug )
					fprintf(stderr, "error recovery leaves state %d, uncovers %d\n", *yys, yys[-1]);
				yys--;
				yyv--;
			}
			if( yydebug )
				fprintf(stderr, "no shift on error; abort\n");
			return(1);

		case 3:
			if( yydebug )
				fprintf(stderr, "Error recovery clobbers char %o\n", yychar);
			if( yychar==YYEOFVAL )
				return(1);
			yychar = YYNOCHAR;
			goto read;
		}

	case YYREDACT:
		pno = act&YYAMASK;
		if( yydebug )
			fprintf(stdout, "reduce %d\n", pno);
		yypvt = yyv;
		yyv -= yypn[pno];
		yys -= yypn[pno];
		yyval = yyv[1];
		switch(pno) {

case 2: {

#line 62 "gram.y"

		emitop(STOP);
		if (allok)
			interp();
		loc = cstream;
		allok = TRUE;
		breakloc = contloc = retfrom = NULL;
	}break;

case 3: {

#line 70 "gram.y"

		emitop(PGLSC);
		emitnum(&zero);
		emitop(LOAD);
		emitop(RETURN);
		emitid(retfrom);
		if (allok) {
			install(&retfrom->globalv.fvalue, pardvec, lpardvec,
				autdvec, lautdvec);
			mpfree(pardvec);
			mpfree(autdvec);
		}
		loc = cstream;
		allok = TRUE;
		breakloc = contloc = retfrom = NULL;
	}break;

case 4: {

#line 86 "gram.y"

		YYERROK;
		loc = cstream;
		allok = TRUE;
		breakloc = contloc = retfrom = NULL;
	}break;

case 5: {

#line 117 "gram.y"
					/* $6  */
			patch(yypvt[-3].location, yypvt[0].location);
		}break;

case 6: {

#line 133 "gram.y"
						/* $10 */
			patch(yypvt[-6].location, yypvt[-2].location);
			patch(yypvt[-3].location, loc);
			patch(yypvt[0].location, contloc);
			breakloc = yypvt[-4].location;			/* restore break */
			contloc = yypvt[-8].location;			/* restore continue */
		}break;

case 7: {

#line 159 "gram.y"
						/* $16 */
			patch(yypvt[-10].location, yypvt[-2].location);
			patch(yypvt[-7].location, loc);
			patch(yypvt[-3].location, yypvt[-11].location);
			patch(yypvt[0].location, contloc);
			breakloc = yypvt[-8].location;
			contloc = yypvt[-6].location;
		}break;

case 8: {

#line 182 "gram.y"
	/* $11 */
			patch(yypvt[-12].location, yypvt[-7].location);
			patch(yypvt[-10].location, loc);
			patch(yypvt[-8].location, yypvt[-3].location);
			patch(yypvt[-2].location, yypvt[-7].location);
			breakloc = yypvt[-11].location;
			contloc = yypvt[-9].location;
		}break;

case 9: {

#line 190 "gram.y"

		if (breakloc == NULL)
			gerror("Break not in loop");
		emitop(BRALW);
		emitaddr(breakloc);
	}break;

case 10: {

#line 196 "gram.y"

		if (contloc == NULL)
			gerror("Continue not in loop");
		emitop(BRALW);
		emitaddr(contloc);
	}break;

case 11: {

#line 202 "gram.y"

		if (retfrom == NULL)
			gerror("Return not in function");
		emitop(PGLSC);
		emitnum(&zero);
		emitop(LOAD);
		emitop(RETURN);
		emitid(retfrom);
	}break;

case 12: {

#line 211 "gram.y"

		if (retfrom == NULL)
			gerror("Return not in function");
		emitop(RETURN);
		emitid(retfrom);
	}break;

case 13: {

#line 217 "gram.y"

		emitop(POP);
	}break;

case 14: {

#line 220 "gram.y"

		emitop(PRNUM);
		emitop(PRNL);
	}break;

case 15: {

#line 224 "gram.y"

		emitop(PRNUM);
	}break;

case 16: {

#line 227 "gram.y"

		emitop(PRSTR);
		emitstr(yypvt[-1].svalue);
		emitop(PRNL);
	}break;

case 17: {

#line 232 "gram.y"

		emitop(PRSTR);
		emitstr(yypvt[-2].svalue);
	}break;

case 19: {

#line 237 "gram.y"

		return(0);
	}break;

case 25: {

#line 257 "gram.y"

		yyval.location = loc;
	}break;

case 26: {

#line 260 "gram.y"

		yyval.location = yypvt[-1].location;
		patch(yypvt[-2].location, loc);
	}break;

case 29: {

#line 274 "gram.y"

		emitop(POP);
	}break;

case 30: {

#line 277 "gram.y"

		emitop(POP);
	}break;

case 31: {

#line 284 "gram.y"

		emitop(BRALW);
		yyval.location = emitzap;
	}break;

case 32: {

#line 292 "gram.y"

		yyval.location = loc;
	}break;

case 33: {

#line 299 "gram.y"

		yyval.location = breakloc;
		breakloc = loc;
	}break;

case 34: {

#line 307 "gram.y"

		yyval.location = contloc;
		contloc = loc;
	}break;

case 36: {

#line 326 "gram.y"

		chkfunc(yypvt[0].dvalue);
		retfrom = yypvt[0].dvalue;
		ldvec = 0;
	}break;

case 37: {

#line 335 "gram.y"

		pardvec = NULL;
		lpardvec = 0;
	}break;

case 38: {

#line 339 "gram.y"

		pardvec = dvec;
		lpardvec = ldvec;
		locaddr(pardvec, lpardvec, 0);
		ldvec = 0;
	}break;

case 39: {

#line 349 "gram.y"

		autdvec = NULL;
		lautdvec = 0;
	}break;

case 40: {

#line 353 "gram.y"

		autdvec = dvec;
		lautdvec = ldvec;
		locaddr(autdvec, lautdvec, lpardvec);
	}break;

case 41: {

#line 362 "gram.y"

		dvec = (dicent **)mpalc(ldvec * sizeof (*dvec));
		dvec += ldvec;
		*--dvec = yypvt[0].dvalue;
	}break;

case 42: {

#line 367 "gram.y"

		*--dvec = yypvt[-2].dvalue;
	}break;

case 43: {

#line 374 "gram.y"

		if (yypvt[0].dvalue->localt != UNDEFINED)
			gerror("Attempt to redeclare %s", yypvt[0].dvalue->word);
		yypvt[0].dvalue->localt = SCALAR;
		++ldvec;
		/* $$ = $1 */
	}break;

case 44: {

#line 381 "gram.y"

		if (yypvt[-2].dvalue->localt != UNDEFINED)
			gerror("Attempt to redeclare %s", yypvt[-2].dvalue->word);
		yypvt[-2].dvalue->localt = ARRAY;
		++ldvec;
		/* $$ = $1 */
	}break;

case 49: {

#line 408 "gram.y"

		emitop(STORE);
	}break;

case 50: {

#line 411 "gram.y"

		emitop(yypvt[-1].opcode);
		emitop(STORE);
	}break;

case 51: {

#line 415 "gram.y"

		emitop(yypvt[-2].opcode);
	}break;

case 52: {

#line 418 "gram.y"

		emitop(yypvt[-1].opcode);
		emitop(yypvt[-2].opcode);
	}break;

case 53: {

#line 426 "gram.y"

		emitop(PGLSC);
		emitnum(yypvt[0].lvalue);
		emitop(LOAD);
	}break;

case 54: {

#line 431 "gram.y"

		emitop(PGLSC);
		emitnum(&dot);
		emitop(LOAD);
	}break;

case 55: {

#line 436 "gram.y"

		emitop(LOAD);
	}break;

case 57: {

#line 440 "gram.y"

		chkfunc(yypvt[-3].dvalue);
		emitop(CALL);
		emitid(yypvt[-3].dvalue);
		emitcnt(yypvt[-1].ivalue);
	}break;

case 58: {

#line 446 "gram.y"

		emitop(PRVAL);
		emitop(INC);
		emitop(STORE);
	}break;

case 59: {

#line 451 "gram.y"

		emitop(INC);
		emitop(yypvt[0].opcode);
	}break;

case 60: {

#line 455 "gram.y"

		emitop(PRVAL);
		emitop(DEC);
		emitop(STORE);
	}break;

case 61: {

#line 460 "gram.y"

		emitop(DEC);
		emitop(yypvt[0].opcode);
	}break;

case 62: {

#line 464 "gram.y"

		emitop(PRVAL);
		emitop(INC);
		emitop(STORE);
		emitop(DEC);
	}break;

case 63: {

#line 470 "gram.y"

		emitop(INC);
		emitop(yypvt[-1].opcode);
		emitop(DEC);
	}break;

case 64: {

#line 475 "gram.y"

		emitop(PRVAL);
		emitop(DEC);
		emitop(STORE);
		emitop(INC);
	}break;

case 65: {

#line 481 "gram.y"

		emitop(DEC);
		emitop(yypvt[-1].opcode);
		emitop(INC);
	}break;

case 66: {

#line 486 "gram.y"

		emitop(NEG);
	}break;

case 67: {

#line 489 "gram.y"

		emitop(EXP);
	}break;

case 68: {

#line 492 "gram.y"

		emitop(MUL);
	}break;

case 69: {

#line 495 "gram.y"

		emitop(DIV);
	}break;

case 70: {

#line 498 "gram.y"

		emitop(REM);
	}break;

case 71: {

#line 501 "gram.y"

		emitop(ADD);
	}break;

case 72: {

#line 504 "gram.y"

		emitop(SUB);
	}break;

case 74: {

#line 508 "gram.y"

		emitop(yypvt[-3].opcode);
	}break;

case 75: {

#line 515 "gram.y"

		sload(yypvt[0].dvalue);
	}break;

case 76: {

#line 518 "gram.y"

		aeload(yypvt[-3].dvalue);
	}break;

case 78: {

#line 526 "gram.y"

		arload(yypvt[-2].dvalue);
	}break;

case 79: {

#line 533 "gram.y"

		yyval.ivalue = 0;
	}break;

case 81: {

#line 542 "gram.y"

		yyval.ivalue = 1;
	}break;

case 82: {

#line 545 "gram.y"

		yyval.ivalue = yypvt[-2].ivalue + 1;
	}break;

case 83: {

#line 552 "gram.y"

		emitop(PRVAL);
	}break;

case 84: {

#line 559 "gram.y"

		yyval.opcode = SIBASE;
	}break;

case 85: {

#line 562 "gram.y"

		yyval.opcode = SOBASE;
	}break;

case 86: {

#line 565 "gram.y"

		yyval.opcode = SSCALE;
	}break;

case 87: {

#line 572 "gram.y"

		emitop(LIBASE);
		yyval.opcode = SIBASE;
	}break;

case 88: {

#line 576 "gram.y"

		emitop(LOBASE);
		yyval.opcode = SOBASE;
	}break;

case 89: {

#line 580 "gram.y"

		emitop(LSCALE);
		yyval.opcode = SSCALE;
	}break;

case 90: {

#line 588 "gram.y"

		yyval.opcode = ADD;
	}break;

case 91: {

#line 591 "gram.y"

		yyval.opcode = SUB;
	}break;

case 92: {

#line 594 "gram.y"

		yyval.opcode = MUL;
	}break;

case 93: {

#line 597 "gram.y"

		yyval.opcode = DIV;
	}break;

case 94: {

#line 600 "gram.y"

		yyval.opcode = REM;
	}break;

case 95: {

#line 603 "gram.y"

		yyval.opcode = EXP;
	}break;

case 96: {

#line 610 "gram.y"

		yyval.opcode = SQRT;
	}break;

case 97: {

#line 613 "gram.y"

		yyval.opcode = LENGTH;
	}break;

case 98: {

#line 616 "gram.y"

		yyval.opcode = SCALE;
	}break;

case 99: {

#line 627 "gram.y"

		emitop(BRALW);
		yyval.location = emitzap;
	}break;

case 100: {

#line 631 "gram.y"

		emitop(yypvt[-1].opcode);
		yyval.location = emitzap;
	}break;

case 101: {

#line 639 "gram.y"

		emitop(BRNEV);
		yyval.location = emitzap;
	}break;

case 102: {

#line 643 "gram.y"

		emitop(negate(yypvt[-1].opcode));
		yyval.location = emitzap;
	}break;

case 103: {

#line 651 "gram.y"

		yyval.opcode = BRLT;
	}break;

case 104: {

#line 654 "gram.y"

		yyval.opcode = BRLE;
	}break;

case 105: {

#line 657 "gram.y"

		yyval.opcode = BREQ;
	}break;

case 106: {

#line 660 "gram.y"

		yyval.opcode = BRGE;
	}break;

case 107: {

#line 663 "gram.y"

		yyval.opcode = BRGT;
	}break;

case 108: {

#line 666 "gram.y"

		yyval.opcode = BRNE;
	}break;

		}
		ip = &yygo[ yypgo[yypdnt[pno]] ];
		while( *ip!=*yys && *ip!=YYNOCHAR )
			ip += 2;
		yystate = ip[1];
		goto stack;
	}
}




