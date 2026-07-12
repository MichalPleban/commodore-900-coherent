
#line 19 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

#include "awk.h"


#include "y.tab.h"
#define YYCLEARIN yychar = -1000
#define YYERROK yyerrflag = 0
extern int yychar;
extern short yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yyval, yylval;
#ifdef YYTNAMES
struct yytname yytnames[67] =
{
	"$end", -1, 
	"error", -2, 
	"IF_", 256, 
	"WHILE_", 257, 
	"FOR_", 258, 
	"ELSE_", 259, 
	"BREAK_", 260, 
	"CONTINUE_", 261, 
	"NEXT_", 262, 
	"EXIT_", 263, 
	"IN_", 264, 
	"PRINT_", 265, 
	"PRINTF_", 266, 
	"BEGIN_", 267, 
	"END_", 268, 
	"FAPPEND_", 269, 
	"FOUT_", 270, 
	"ID_", 271, 
	"STRING_", 272, 
	"NUMBER_", 273, 
	"FUNCTION_", 274, 
	"SCON_", 294, 
	"ASADD_", 295, 
	"ASSUB_", 296, 
	"ASMUL_", 297, 
	"ASDIV_", 298, 
	"'='", 61, 
	"ASMOD_", 299, 
	"OROR_", 300, 
	"ANDAND_", 301, 
	"'~'", 126, 
	"NMATCH_", 302, 
	"EQ_", 303, 
	"NE_", 304, 
	"GE_", 305, 
	"'>'", 62, 
	"LE_", 306, 
	"'<'", 60, 
	"'+'", 43, 
	"'-'", 45, 
	"'*'", 42, 
	"'/'", 47, 
	"'%'", 37, 
	"'!'", 33, 
	"INC_", 307, 
	"DEC_", 308, 
	"'$'", 36, 
	"REEOL_", 309, 
	"REBOL_", 310, 
	"REANY_", 311, 
	"RECLASS_", 312, 
	"RECHAR_", 313, 
	"REOR_", 314, 
	"RECON_", 315, 
	"RECLOS_", 316, 
	"RENECL_", 317, 
	"REZOCL_", 318, 
	"'\\n'", 10, 
	"','", 44, 
	"'('", 40, 
	"')'", 41, 
	"'{'", 123, 
	"'}'", 125, 
	"';'", 59, 
	"'['", 91, 
	"']'", 93, 
	"'|'", 124, 
	NULL
} ;
#endif
unsigned yypdnt[99] = {
00, 01, 02, 02, 03, 03, 03, 05, 
05, 05, 05, 010, 07, 012, 07, 011, 
011, 011, 011, 011, 011, 011, 011, 011, 
013, 013, 013, 04, 015, 017, 015, 015, 
015, 015, 015, 015, 015, 015, 021, 015, 
024, 015, 025, 015, 015, 015, 015, 014, 
014, 016, 026, 026, 026, 026, 026, 026, 
030, 030, 030, 030, 030, 020, 020, 020, 
020, 032, 032, 031, 031, 027, 027, 06, 
06, 06, 06, 06, 06, 06, 06, 06, 
06, 06, 06, 06, 06, 06, 06, 06, 
06, 06, 06, 06, 06, 023, 023, 023, 
023, 022, 022  
} ;
unsigned yypn[99] = {
02, 01, 02, 01, 02, 02, 03, 01, 
01, 01, 03, 00, 04, 00, 04, 02, 
03, 03, 02, 01, 01, 01, 01, 01, 
01, 01, 01, 03, 05, 00, 010, 05, 
07, 011, 02, 02, 01, 02, 00, 04, 
00, 03, 00, 04, 02, 02, 01, 01, 
02, 01, 03, 03, 02, 02, 02, 02, 
01, 01, 01, 01, 01, 04, 02, 04, 
01, 01, 01, 01, 01, 01, 02, 03, 
03, 03, 02, 01, 01, 03, 03, 03, 
03, 03, 03, 03, 03, 03, 03, 03, 
03, 03, 01, 04, 01, 03, 03, 03, 
01, 01, 03  
} ;
unsigned yypgo[27] = {
00, 00, 02, 06, 010, 020, 022, 0132, 
0142, 0144, 0154, 0156, 0160, 0164, 0200, 0210, 
0212, 0234, 0236, 0246, 0254, 0256, 0260, 0262, 
0272, 0274, 0306  
} ;
unsigned yygo[200] = {
0176030, 017, 021, 064, 0176030, 020, 0176030, 021, 
00, 022, 021, 022, 023, 067, 0176030, 057, 
0176030, 023, 00, 024, 011, 036, 015, 044, 
016, 060, 021, 024, 041, 0135, 062, 060, 
070, 0155, 071, 0156, 074, 0161, 075, 0162, 
076, 0163, 077, 0164, 0100, 0165, 0101, 0166, 
0102, 0167, 0103, 0170, 0104, 0171, 0105, 0172, 
0106, 0173, 0107, 0174, 0123, 0201, 0125, 0201, 
0137, 0215, 0140, 0216, 0141, 0217, 0175, 0201, 
0176, 0201, 0234, 060, 0235, 060, 0236, 0247, 
0255, 0260, 0256, 060, 0257, 060, 0263, 060, 
0176030, 0122, 072, 0157, 073, 0160, 0121, 0177, 
0176030, 025, 0176030, 035, 035, 0134, 0133, 0204, 
0206, 0232, 0176030, 0212, 0176030, 0121, 0176030, 0213, 
062, 0153, 0176030, 061, 0234, 0245, 0235, 0246, 
0256, 0261, 0257, 0262, 0263, 0264, 0176030, 062, 
0216, 0235, 0250, 0256, 0260, 0263, 0176030, 0234, 
0176030, 0257, 012, 037, 013, 040, 014, 042, 
0141, 0220, 0222, 042, 0223, 042, 0225, 042, 
0237, 0250, 0176030, 026, 0176030, 0146, 0146, 0221, 
0150, 0227, 0203, 0230, 0176030, 0124, 0221, 0240, 
0227, 0244, 0176030, 0226, 0176030, 0147, 0176030, 0150, 
0176030, 027, 033, 0123, 0114, 0175, 0120, 0176, 
0176030, 0125, 0176030, 0120, 014, 043, 0222, 0241, 
0223, 0242, 0225, 0243, 0176030, 030, 0176030, 031  
} ;
unsigned yypa[181] = {
00, 034, 040, 042, 044, 050, 052, 054, 
060, 062, 0110, 0110, 0116, 062, 0132, 0206, 
0212, 0214, 0252, 0256, 0264, 0326, 0330, 0352, 
0354, 0356, 0360, 062, 062, 0362, 0400, 0402, 
0404, 062, 0406, 0410, 0412, 0454, 0460, 0464, 
0470, 0474, 0500, 0504, 0510, 0522, 0524, 0526, 
0530, 0572, 0576, 0654, 0660, 0662, 0664, 0666, 
062, 062, 0672, 0676, 062, 062, 062, 062, 
062, 062, 062, 062, 062, 062, 062, 062, 
0702, 0704, 0706, 0710, 062, 0712, 0714, 0716, 
062, 0672, 0720, 0760, 01010, 01014, 01056, 01060, 
01062, 01064, 01066, 0362, 01070, 01120, 01162, 062, 
062, 062, 01164, 01166, 01170, 01172, 062, 01174, 
062, 01206, 01210, 01212, 01214, 01216, 01254, 01310, 
01312, 01314, 01344, 01374, 01420, 01444, 01470, 01514, 
01524, 01534, 01536, 01540, 01542, 01602, 01706, 02012, 
02014, 02016, 02056, 062, 02060, 02110, 0362, 02112, 
02114, 02116, 02120, 02144, 02146, 02150, 02150, 02212, 
02254, 02300, 02312, 02312, 02324, 02312, 02326, 02300, 
02330, 02332, 02334, 02360, 0132, 0132, 062, 0110, 
02362, 02364, 02370, 02374, 02400, 02402, 02406, 02410, 
02452, 02456, 02460, 02462, 02464, 062, 0132, 0132, 
02150, 02466, 02470, 0132, 02472  
} ;
unsigned yyact[1340] = {
02, 0413, 03, 0414, 04, 0417, 05, 0420, 
06, 0421, 07, 0422, 010, 057, 011, 041, 
012, 0463, 013, 0464, 014, 044, 015, 050, 
016, 0173, 01, 0176030, 032, 012, 060000, 0176030, 
020007, 0176030, 020010, 0176030, 033, 0133, 020100, 0176030, 
020101, 0176030, 020102, 0176030, 034, 050, 020134, 0176030, 
020013, 0176030, 04, 0417, 05, 0420, 06, 0421, 
07, 0422, 010, 057, 011, 041, 012, 0463, 
013, 0464, 014, 044, 015, 050, 01, 0176030, 
04, 0417, 014, 044, 060000, 0176030, 04, 0417, 
05, 0420, 06, 0421, 014, 044, 041, 050, 
060000, 0176030, 045, 0400, 046, 0401, 047, 0402, 
050, 0404, 051, 0405, 052, 0406, 053, 0407, 
054, 0411, 055, 0412, 04, 0417, 05, 0420, 
06, 0421, 07, 0422, 010, 057, 011, 041, 
012, 0463, 013, 0464, 014, 044, 015, 050, 
016, 0173, 056, 073, 01, 0176030, 063, 0177777, 
060000, 0176030, 020001, 0176030, 02, 0413, 03, 0414, 
04, 0417, 05, 0420, 06, 0421, 07, 0422, 
010, 057, 011, 041, 012, 0463, 013, 0464, 
014, 044, 015, 050, 016, 0173, 020003, 0177777, 
01, 0176030, 065, 012, 060000, 0176030, 066, 012, 
016, 0173, 060000, 0176030, 070, 0454, 071, 0455, 
072, 0176, 073, 0456, 074, 0457, 075, 0460, 
076, 0461, 077, 076, 0100, 0462, 0101, 074, 
0102, 053, 0103, 055, 0104, 052, 0105, 057, 
0106, 045, 0107, 054, 020011, 0176030, 020132, 0176030, 
0110, 0447, 0111, 0450, 0112, 0451, 0113, 0452, 
0114, 075, 0115, 0453, 0116, 0463, 0117, 0464, 
020103, 0176030, 020114, 0176030, 020113, 0176030, 020104, 0176030, 
020015, 0176030, 0126, 0465, 0127, 0466, 0130, 0467, 
0131, 0470, 0132, 0471, 0133, 050, 060000, 0176030, 
020112, 0176030, 020064, 0176030, 020066, 0176030, 020103, 0176030, 
020076, 0176030, 070, 0454, 071, 0455, 072, 0176, 
073, 0456, 074, 0457, 075, 0460, 076, 0461, 
077, 076, 0100, 0462, 0101, 074, 0102, 053, 
0103, 055, 0104, 052, 0105, 057, 0106, 045, 
0136, 051, 060000, 0176030, 0137, 050, 060000, 0176030, 
0140, 050, 060000, 0176030, 0141, 050, 060000, 0176030, 
0142, 073, 060000, 0176030, 0143, 073, 060000, 0176030, 
0144, 073, 060000, 0176030, 0145, 073, 060000, 0176030, 
020050, 0415, 020050, 0416, 020050, 073, 020050, 0174, 
020046, 0176030, 020052, 0176030, 020056, 0176030, 020044, 0176030, 
070, 0454, 071, 0455, 072, 0176, 073, 0456, 
074, 0457, 075, 0460, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 0151, 073, 
060000, 0176030, 0152, 0175, 060000, 0176030, 045, 0400, 
046, 0401, 047, 0402, 050, 0404, 051, 0405, 
052, 0406, 053, 0407, 054, 0411, 055, 0412, 
04, 0417, 05, 0420, 06, 0421, 07, 0422, 
010, 057, 011, 041, 012, 0463, 013, 0464, 
014, 044, 015, 050, 016, 0173, 056, 073, 
020057, 0175, 01, 0176030, 040000, 0177777, 060000, 0176030, 
020002, 0176030, 020004, 0176030, 020005, 0176030, 0154, 012, 
060000, 0176030, 010, 057, 01, 0176030, 010, 057, 
01, 0176030, 020070, 0176030, 020071, 0176030, 020072, 0176030, 
020073, 0176030, 020074, 0176030, 020065, 0176030, 020067, 0176030, 
070, 0454, 071, 0455, 072, 0176, 073, 0456, 
074, 0457, 075, 0460, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 020105, 0176030, 
04, 0417, 05, 0420, 06, 0421, 07, 0422, 
010, 057, 011, 041, 012, 0463, 013, 0464, 
014, 044, 015, 050, 0200, 0135, 01, 0176030, 
0202, 051, 060000, 0176030, 04, 0417, 05, 0420, 
06, 0421, 07, 0422, 010, 057, 011, 041, 
012, 0463, 013, 0464, 014, 044, 0203, 054, 
015, 050, 020141, 0415, 020141, 0416, 020141, 051, 
020141, 073, 020141, 0174, 01, 0176030, 020026, 0176030, 
020025, 0176030, 020024, 0176030, 020023, 0176030, 020027, 0176030, 
0205, 057, 0126, 0465, 0127, 0466, 0130, 0467, 
0131, 0470, 0132, 0471, 0206, 0472, 0207, 0474, 
0210, 0475, 0211, 0476, 0133, 050, 060000, 0176030, 
070, 0454, 071, 0455, 072, 0176, 073, 0456, 
074, 0457, 075, 0460, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 0214, 051, 
060000, 0176030, 020107, 0176030, 020042, 0176030, 020043, 0176030, 
020054, 0176030, 020055, 0176030, 0222, 0415, 0223, 0416, 
0224, 073, 0225, 0174, 060000, 0176030, 020045, 0176030, 
020033, 0176030, 020060, 0176030, 020006, 0176030, 071, 0455, 
072, 0176, 073, 0456, 074, 0457, 075, 0460, 
076, 0461, 077, 076, 0100, 0462, 0101, 074, 
0102, 053, 0103, 055, 0104, 052, 0105, 057, 
0106, 045, 020111, 0176030, 072, 0176, 073, 0456, 
074, 0457, 075, 0460, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 020110, 0176030, 
020130, 0176030, 020131, 0176030, 074, 0457, 075, 0460, 
076, 0461, 077, 076, 0100, 0462, 0101, 074, 
0102, 053, 0103, 055, 0104, 052, 0105, 057, 
0106, 045, 020124, 0176030, 074, 0457, 075, 0460, 
076, 0461, 077, 076, 0100, 0462, 0101, 074, 
0102, 053, 0103, 055, 0104, 052, 0105, 057, 
0106, 045, 020125, 0176030, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 020126, 0176030, 
076, 0461, 077, 076, 0100, 0462, 0101, 074, 
0102, 053, 0103, 055, 0104, 052, 0105, 057, 
0106, 045, 020122, 0176030, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 020127, 0176030, 
076, 0461, 077, 076, 0100, 0462, 0101, 074, 
0102, 053, 0103, 055, 0104, 052, 0105, 057, 
0106, 045, 020123, 0176030, 0104, 052, 0105, 057, 
0106, 045, 020115, 0176030, 0104, 052, 0105, 057, 
0106, 045, 020116, 0176030, 020117, 0176030, 020120, 0176030, 
020121, 0176030, 070, 0454, 071, 0455, 072, 0176, 
073, 0456, 074, 0457, 075, 0460, 076, 0461, 
077, 076, 0100, 0462, 0101, 074, 0102, 053, 
0103, 055, 0104, 052, 0105, 057, 0106, 045, 
020012, 0176030, 04, 0417, 05, 0420, 06, 0421, 
07, 0422, 010, 057, 011, 041, 012, 0463, 
013, 0464, 014, 044, 015, 050, 020062, 0415, 
020062, 0416, 020062, 0454, 020062, 0455, 020062, 0176, 
020062, 0456, 020062, 0457, 020062, 0460, 020062, 0461, 
020062, 076, 020062, 0462, 020062, 074, 020062, 053, 
020062, 055, 020062, 052, 020062, 045, 020062, 012, 
020062, 054, 020062, 051, 020062, 0173, 020062, 073, 
020062, 0135, 020062, 0174, 01, 0176030, 04, 0417, 
05, 0420, 06, 0421, 07, 0422, 010, 057, 
011, 041, 012, 0463, 013, 0464, 014, 044, 
015, 050, 020063, 0415, 020063, 0416, 020063, 0454, 
020063, 0455, 020063, 0176, 020063, 0456, 020063, 0457, 
020063, 0460, 020063, 0461, 020063, 076, 020063, 0462, 
020063, 074, 020063, 053, 020063, 055, 020063, 052, 
020063, 045, 020063, 012, 020063, 054, 020063, 051, 
020063, 0173, 020063, 073, 020063, 0135, 020063, 0174, 
01, 0176030, 020016, 0176030, 020077, 0176030, 070, 0454, 
071, 0455, 072, 0176, 073, 0456, 074, 0457, 
075, 0460, 076, 0461, 077, 076, 0100, 0462, 
0101, 074, 0102, 053, 0103, 055, 0104, 052, 
0105, 057, 0106, 045, 020106, 0176030, 020133, 0176030, 
0126, 0465, 0127, 0466, 0130, 0467, 0131, 0470, 
0132, 0471, 0206, 0472, 0207, 0474, 0210, 0475, 
0211, 0476, 0133, 050, 0231, 051, 060000, 0176030, 
020014, 0176030, 020030, 0176030, 020032, 0176030, 020031, 0176030, 
0126, 0465, 0127, 0466, 0130, 0467, 0131, 0470, 
0132, 0471, 0207, 0474, 0210, 0475, 0211, 0476, 
0133, 050, 020017, 0176030, 020022, 0176030, 020075, 0176030, 
070, 0454, 071, 0455, 072, 0176, 073, 0456, 
074, 0457, 075, 0460, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 0233, 051, 
060000, 0176030, 070, 0454, 071, 0455, 072, 0176, 
073, 0456, 074, 0457, 075, 0460, 076, 0461, 
077, 076, 0100, 0462, 0101, 074, 0102, 053, 
0103, 055, 0104, 052, 0105, 057, 0106, 045, 
0236, 073, 060000, 0176030, 0237, 0410, 0110, 0447, 
0111, 0450, 0112, 0451, 0113, 0452, 0114, 075, 
0115, 0453, 0116, 0463, 0117, 0464, 020103, 0176030, 
0222, 0415, 0223, 0416, 0224, 073, 0225, 0174, 
060000, 0176030, 04, 0417, 05, 0420, 06, 0421, 
014, 044, 060000, 0176030, 020140, 0176030, 020051, 0176030, 
020142, 0176030, 020020, 0176030, 0126, 0465, 0127, 0466, 
0130, 0467, 0131, 0470, 0132, 0471, 0207, 0474, 
0210, 0475, 0211, 0476, 0133, 050, 020021, 0176030, 
020061, 0176030, 020047, 0176030, 0251, 073, 060000, 0176030, 
0252, 073, 060000, 0176030, 0253, 073, 060000, 0176030, 
020053, 0176030, 0254, 0403, 020034, 0176030, 020037, 0176030, 
070, 0454, 071, 0455, 072, 0176, 073, 0456, 
074, 0457, 075, 0460, 076, 0461, 077, 076, 
0100, 0462, 0101, 074, 0102, 053, 0103, 055, 
0104, 052, 0105, 057, 0106, 045, 0255, 073, 
060000, 0176030, 0233, 051, 060000, 0176030, 020135, 0176030, 
020136, 0176030, 020137, 0176030, 020035, 0176030, 020040, 0176030, 
020036, 0176030, 020041, 0176030  
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

#line 71 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		codep = yypvt[0].u_node;
	}break;

case 2: {

#line 77 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ALIST, yypvt[-1].u_node, yypvt[0].u_node);
	}break;

case 4: {

#line 84 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AROOT, NULL, yypvt[-1].u_node);
	}break;

case 5: {

#line 87 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AROOT, yypvt[-1].u_node, NULL);
	}break;

case 6: {

#line 90 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AROOT, yypvt[-2].u_node, yypvt[-1].u_node);
	}break;

case 7: {

#line 96 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ABEGIN);
	}break;

case 8: {

#line 99 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AEND);
	}break;

case 10: {

#line 103 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ARANGE, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 11: {

#line 109 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"
 lexre=1; }break;

case 12: {

#line 109 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		lexre = 0;
		yyval.u_node = yypvt[-1].u_node;
	}break;

case 13: {

#line 113 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyerrok;
		yyerror("Badly formed regular expression");
	}break;

case 14: {

#line 116 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = yypvt[0].u_node;
	}break;

case 15: {

#line 122 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		register NODE *np;

		for (np = yypvt[-1].u_node; np->n_O3!=NULL; np = np->n_O3)
			;
		np->n_O3 = yypvt[0].u_node;
		yyval.u_node = yypvt[-1].u_node;
	}break;

case 16: {

#line 130 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = yypvt[-1].u_node;
	}break;

case 17: {

#line 133 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AROR, yypvt[-2].u_node, yypvt[0].u_node, NULL);
	}break;

case 18: {

#line 136 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(yypvt[0].u_char, yypvt[-1].u_node, NULL, NULL);
	}break;

case 19: {

#line 139 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(yflag?ARDCLASS:ARCLASS, NULL, NULL, NULL);
		yyval.u_node->n_o1.n_charp = yypvt[0].u_charp;
	}break;

case 20: {

#line 143 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ARANY, NULL, NULL, NULL);
	}break;

case 21: {

#line 146 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ARBOL, NULL, NULL, NULL);
	}break;

case 22: {

#line 149 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AREOL, NULL, NULL, NULL);
	}break;

case 23: {

#line 152 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = cnode(yflag?ARDCHAR:ARCHAR, yypvt[0].u_char);
	}break;

case 24: {

#line 158 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = ARCLOS;
	}break;

case 25: {

#line 161 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = ARZOCL;
	}break;

case 26: {

#line 164 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = ARNECL;
	}break;

case 27: {

#line 170 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = yypvt[-1].u_node;
	}break;

case 28: {

#line 176 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AIF, yypvt[-2].u_node, yypvt[0].u_node, NULL);
	}break;

case 29: {

#line 179 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"
nlskip = 1;}break;

case 30: {

#line 179 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AIF, yypvt[-5].u_node, yypvt[-3].u_node, yypvt[0].u_node);
	}break;

case 31: {

#line 182 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AWHILE, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 32: {

#line 185 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFORIN, yypvt[-4].u_node, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 33: {

#line 188 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFOR, yypvt[-6].u_node, yypvt[-4].u_node, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 34: {

#line 191 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ABREAK);
	}break;

case 35: {

#line 194 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ACONTIN);
	}break;

case 38: {

#line 199 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"
outflag++;}break;

case 39: {

#line 199 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(APRINT, yypvt[-1].u_node, yypvt[0].u_node);
	}break;

case 40: {

#line 202 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"
outflag++;}break;

case 41: {

#line 202 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(APRINT, &xfield0, yypvt[0].u_node);
	}break;

case 42: {

#line 205 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"
outflag++;}break;

case 43: {

#line 205 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(APRINTF, yypvt[-1].u_node, yypvt[0].u_node);
	}break;

case 44: {

#line 208 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ANEXT);
	}break;

case 45: {

#line 211 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AEXIT);
	}break;

case 46: {

#line 214 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = NULL;
	}break;

case 48: {

#line 221 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		if (yypvt[-1].u_node!=NULL && yypvt[0].u_node!=NULL)
			yyval.u_node = node(ALIST, yypvt[-1].u_node, yypvt[0].u_node);
		else if (yypvt[-1].u_node != NULL)
			yyval.u_node = yypvt[-1].u_node;
		else
			yyval.u_node = yypvt[0].u_node;
	}break;

case 49: {

#line 232 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		nlskip = 1;
	}break;

case 50: {

#line 238 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AASGN, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 51: {

#line 241 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AASGN, yypvt[-2].u_node, node(yypvt[-1].u_char, yypvt[-2].u_node, yypvt[0].u_node));
	}break;

case 52: {

#line 244 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AASGN, yypvt[0].u_node, node(AADD, yypvt[0].u_node, &xone));
	}break;

case 53: {

#line 247 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AINCA, yypvt[-1].u_node);
	}break;

case 54: {

#line 250 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AASGN, yypvt[0].u_node, node(ASUB, yypvt[0].u_node, &xone));
	}break;

case 55: {

#line 253 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ADECA, yypvt[-1].u_node);
	}break;

case 56: {

#line 259 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = AADD;
	}break;

case 57: {

#line 262 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = ASUB;
	}break;

case 58: {

#line 265 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = AMUL;
	}break;

case 59: {

#line 268 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = ADIV;
	}break;

case 60: {

#line 271 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_char = AMOD;
	}break;

case 61: {

#line 277 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFIELD, yypvt[-1].u_node);
	}break;

case 62: {

#line 280 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFIELD, yypvt[0].u_node);
	}break;

case 63: {

#line 283 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AARRAY, yypvt[-3].u_node, yypvt[-1].u_node);
	}break;

case 70: {

#line 301 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ACONC, yypvt[-1].u_node, yypvt[0].u_node);
	}break;

case 71: {

#line 307 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = yypvt[-1].u_node;
	}break;

case 72: {

#line 310 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AANDAND, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 73: {

#line 313 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AOROR, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 74: {

#line 316 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ANOT, yypvt[0].u_node);
	}break;

case 77: {

#line 321 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AADD, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 78: {

#line 324 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ASUB, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 79: {

#line 327 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AMUL, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 80: {

#line 330 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ADIV, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 81: {

#line 333 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AMOD, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 82: {

#line 336 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AGT, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 83: {

#line 339 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ALT, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 84: {

#line 342 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AEQ, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 85: {

#line 345 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ANE, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 86: {

#line 348 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AGE, yypvt[-2].u_node,yypvt[0].u_node);
	}break;

case 87: {

#line 351 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ALE, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 88: {

#line 354 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AREMAT, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 89: {

#line 357 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ARENMAT, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

case 90: {

#line 360 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		if (brlevel)
			awkerr("Regular expression illegal in action");
		yyval.u_node = node(ARE, yypvt[0].u_node);
	}break;

case 91: {

#line 365 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFUNC, yypvt[-3].u_node, yypvt[-1].u_node);
	}break;

case 92: {

#line 368 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFUNC, yypvt[0].u_node, NULL);
	}break;

case 93: {

#line 374 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFAPP, yypvt[-1].u_node);
	}break;

case 94: {

#line 377 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFOUT, yypvt[-1].u_node);
	}break;

case 95: {

#line 380 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(AFPIPE, yypvt[-1].u_node);
	}break;

case 96: {

#line 383 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = NULL;
	}break;

case 97: {

#line 389 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = yypvt[0].u_node;
	}break;

case 98: {

#line 392 "C:/Users/micha/Documents/Coherent/Source/src/userland/cmd/awk/awk.y"

		yyval.u_node = node(ALIST, yypvt[-2].u_node, yypvt[0].u_node);
	}break;

		}
		ip = &yygo[ yypgo[yypdnt[pno]] ];
		while( *ip!=*yys && *ip!=YYNOCHAR )
			ip += 2;
		yystate = ip[1];
		goto stack;
	}
}




