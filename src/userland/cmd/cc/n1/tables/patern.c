#ifdef vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif
#if !TINY
#define fl(f,l)	, f, l
#else
#define fl(f,l)	/* f, l */
#endif
extern char macros[];
/* prefac.f */
#define	WORD	(FS16|FU16)		
#define	UWORD	(FU16)			
#define	INT	(FS16|FU16)		
#define	BYTE	(FS8|FU8)		
#define	LONG	(FS32|FU32)		
#define	FLT	(FF32)		
#define	DBL	(FF64)			
#define	LPTX	(FLPTR|FLPTB)		
#define	SPTX	(FSPTR|FSPTB)		
#define	PTR	(FLPTR|FLPTB|FSPTR|FSPTB)
#define	NFLT	(BYTE|WORD|LONG|LPTX)	
#define	ANYT	(NFLT|FF32|FF64)
#define	PREL	(PEQ|PNE|PGT|PGE|PLT|PLE|PUGT|PUGE|PULT|PULE)
#define	PSREL	(PGT|PGE|PLT|PLE)		
#define	PUREL	(PUGT|PUGE|PULT|PULE)		
#define	PEREL	(PEQ|PNE)			
#define	PNEREL	(PGT|PGE|PLT|PLE|PUGT|PUGE|PULT|PULE)	
/* aadd.t */
PAT p1[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  2,  1, &macros[   0] fl(1,8) },
 {  2,  2, ANYR, NONE, NONE, TEMP,  1,  2,  2,  2, &macros[  14] fl(1,17) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  2,  3, &macros[  27] fl(1,27) },
 {  3,  4, ANYR, NONE, NONE, TEMP,  3,  4,  2,  1, &macros[  37] fl(1,40) },
 {  3,  4, ANYR, NONE, NONE, TEMP,  1,  4,  2,  1, &macros[  47] fl(1,50) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  5,  2,  1, &macros[  54] fl(1,67) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  6,  2,  1, &macros[  78] fl(1,78) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  7,  2,  1, &macros[  94] fl(1,96) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  8,  2,  1, &macros[ 120] fl(1,108) }
};
/* aand.t */
PAT p2[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  2,  1, &macros[ 151] fl(2,6) },
 {  2,  2, ANYR, NONE, NONE, TEMP,  1,  2,  2,  2, &macros[ 157] fl(2,15) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  2,  3, &macros[ 164] fl(2,25) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  5,  2,  1, &macros[ 178] fl(2,38) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  6,  2,  1, &macros[ 185] fl(2,49) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  7,  2,  1, &macros[ 192] fl(2,64) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  8,  2,  1, &macros[ 200] fl(2,73) }
};
/* add.t */
PAT p3[] = {
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  5,  0, &macros[ 211] fl(3,18) },
 {  5,  1, ANYL, ANYL, NONE, TEMP,  4,  1,  5,  0, &macros[ 211] fl(3,22) },
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  6,  0, &macros[ 217] fl(3,30) },
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  2,  1, &macros[ 223] fl(3,38) },
 {  4,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  2,  2, &macros[ 228] fl(3,46) },
 {  4,  9, ANYR, ANYR, NONE, TEMP,  4,  9,  2,  3, &macros[ 231] fl(3,68) },
 {  5,  9, ANYL, ANYL, NONE, TEMP,  4,  9,  2,  3, &macros[ 231] fl(3,72) },
 {  6,  4, ANYR, ANYR, NONE, TEMP,  4,  4,  2,  1, &macros[ 234] fl(3,91) },
 {  5,  4, ANYL, ANYL, NONE, TEMP,  4,  4,  2,  1, &macros[ 234] fl(3,95) },
 {  3,  4, PAIR, LOTEMP, NONE, TEMP,  4,  1,  7,  4, &macros[ 239] fl(3,113) },
 {  7,  4, ANYL, LOTEMP, NONE, TEMP,  4,  1,  7,  4, &macros[ 239] fl(3,117) },
 {  3,  4, PAIR, LOTEMP, NONE, TEMP,  4,  1,  8,  4, &macros[ 249] fl(3,139) },
 {  7,  4, ANYL, LOTEMP, NONE, TEMP,  4,  1,  8,  4, &macros[ 249] fl(3,143) }
};
/* adiv.t */
PAT p4[] = {
 {  3, 10,  RR0, NONE, NONE,   R1,  1,  1,  9,  1, &macros[ 260] fl(4,6) },
 {  3, 10,  RQ0, NONE, NONE,   R3,  1,  1,  2,  3, &macros[ 276] fl(4,18) },
 {  3, 11,  RR0, NONE, NONE,   R1,  1,  1,  2,  1, &macros[ 297] fl(4,27) },
 {  3, 12,  RQ0, NONE, NONE,  RR2,  1,  3,  9,  3, &macros[ 306] fl(4,39) },
 {  3, 12,  RQ0, NONE, NONE,  RR2,  1,  3, 10,  3, &macros[ 324] fl(4,49) },
 {  3, 13,  RQ0, NONE, NONE,  RR2,  1,  3,  2,  3, &macros[ 372] fl(4,66) },
 {  3,  1,  RR0, NONE, NONE,   R1,  1,  5,  2,  1, &macros[ 379] fl(4,82) },
 {  3,  1,  RR0, NONE, NONE,   R1,  1,  6,  2,  1, &macros[ 405] fl(4,92) }
};
/* aft.t */
PAT p5[] = {
 {  3,  1, ANYR, NONE, NONE, TEMP, 10,  1,  5,  0, &macros[ 429] fl(5,7) },
 {  7,  1, ANYL, NONE, NONE, TEMP, 10,  1,  5,  0, &macros[ 429] fl(5,11) },
 {  3,  1, ANYR, NONE, NONE, TEMP, 10,  5,  5,  0, &macros[ 439] fl(5,24) },
 {  7,  1, ANYL, NONE, NONE, TEMP, 10,  5,  5,  0, &macros[ 439] fl(5,28) },
 {  3,  1, ANYR, NONE, NONE, TEMP, 10,  6,  5,  0, &macros[ 452] fl(5,36) },
 {  7,  1, ANYL, NONE, NONE, TEMP, 10,  6,  5,  0, &macros[ 452] fl(5,40) },
 {  3,  4, ANYR, NONE, NONE, TEMP,  3,  4,  9,  1, &macros[ 463] fl(5,50) },
 {  7,  4, ANYL, NONE, NONE, TEMP,  3,  4,  9,  1, &macros[ 463] fl(5,54) },
 {  3,  4, ANYR, NONE, NONE, TEMP,  1,  4,  9,  1, &macros[ 473] fl(5,65) },
 {  7,  4, ANYL, NONE, NONE, TEMP,  1,  4,  9,  1, &macros[ 473] fl(5,69) },
 {  3,  3, ANYR, NONE, NONE, TEMP, 10,  3,  5,  0, &macros[ 492] fl(5,86) },
 {  7,  3, ANYL, NONE, NONE, TEMP, 10,  3,  5,  0, &macros[ 492] fl(5,90) }
};
/* amul.t */
PAT p6[] = {
 {  3,  1,  RR0, NONE, NONE,   R1,  1,  1,  2,  1, &macros[ 509] fl(6,6) },
 {  3,  3,  RQ0, NONE, NONE,  RR2,  1,  3,  2,  3, &macros[ 516] fl(6,16) },
 {  3,  1,  RR0, NONE, NONE,   R1,  1,  5,  2,  1, &macros[ 523] fl(6,31) },
 {  3,  1,  RR0, NONE, NONE,   R1,  1,  6,  2,  1, &macros[ 531] fl(6,39) }
};
/* and.t */
PAT p7[] = {
 {  8,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  2,  1, &macros[ 539] fl(7,5) },
 {  8,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  2,  2, &macros[ 542] fl(7,12) },
 {  6,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  2,  3, &macros[ 545] fl(7,20) },
 {  6,  4, ANYR, ANYR, NONE, TEMP,  4,  9, 11,  3, &macros[ 556] fl(7,32) },
 {  6,  4, ANYR, ANYR, NONE, TEMP,  4,  9,  2,  3, &macros[ 558] fl(7,39) }
};
/* aor.t */
PAT p8[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  2,  1, &macros[ 560] fl(8,6) },
 {  2,  2, ANYR, NONE, NONE, TEMP,  1,  2,  2,  2, &macros[ 566] fl(8,15) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  2,  3, &macros[ 573] fl(8,25) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  5,  2,  1, &macros[ 584] fl(8,37) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  6,  2,  1, &macros[ 591] fl(8,48) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  7,  2,  1, &macros[ 598] fl(8,63) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  8,  2,  1, &macros[ 604] fl(8,72) }
};
/* arem.t */
PAT p9[] = {
 {  3, 10,  RR0, NONE, NONE,   R0,  1,  1,  9,  1, &macros[ 611] fl(9,5) },
 {  3, 10,  RQ0, NONE, NONE,   R1,  1,  1,  2,  3, &macros[ 627] fl(9,17) },
 {  3, 11,  RR0, NONE, NONE,   R0,  1,  1,  2,  1, &macros[ 645] fl(9,26) },
 {  3, 12,  RQ0, NONE, NONE,  RR0,  1,  3,  9,  3, &macros[ 654] fl(9,37) },
 {  3, 12,  RQ0, NONE, NONE,  RR0,  1,  3, 10,  3, &macros[ 672] fl(9,48) },
 {  3, 13,  RQ0, NONE, NONE,  RR0,  1,  3,  2,  3, &macros[ 710] fl(9,63) },
 {  3,  1,  RR0, NONE, NONE,   R0,  1,  5,  2,  1, &macros[ 717] fl(9,77) },
 {  3,  1,  RR0, NONE, NONE,   R0,  1,  6,  2,  1, &macros[ 743] fl(9,87) }
};
/* ashl.t */
PAT p10[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  9,  1, &macros[ 767] fl(10,6) },
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  4,  1, &macros[ 773] fl(10,15) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  9,  1, &macros[ 779] fl(10,25) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  4,  1, &macros[ 785] fl(10,32) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  5,  9,  1, &macros[ 791] fl(10,46) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  6,  9,  1, &macros[ 807] fl(10,54) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  5,  4,  1, &macros[ 824] fl(10,62) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  6,  4,  1, &macros[ 831] fl(10,70) }
};
/* ashr.t */
PAT p11[] = {
 {  1, 11, ANYR, NONE, NONE, TEMP,  1, 11,  9,  1, &macros[ 838] fl(11,7) },
 {  1, 10, ANYR, NONE, NONE, TEMP,  1, 10,  9,  1, &macros[ 844] fl(11,16) },
 {  1, 11, ANYR, NONE, NONE, TEMP,  1, 11,  4,  1, &macros[ 846] fl(11,25) },
 {  1, 10, ANYR, NONE, NONE, TEMP,  1, 10,  4,  1, &macros[ 854] fl(11,35) },
 {  3, 13, ANYR, NONE, NONE, TEMP,  1, 13,  9,  1, &macros[ 861] fl(11,47) },
 {  3, 12, ANYR, NONE, NONE, TEMP,  1, 12,  9,  1, &macros[ 867] fl(11,54) },
 {  3, 13, ANYR, NONE, NONE, TEMP,  1, 13,  4,  1, &macros[ 869] fl(11,61) },
 {  3, 12, ANYR, NONE, NONE, TEMP,  1, 12,  4,  1, &macros[ 877] fl(11,69) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  5,  9,  1, &macros[ 884] fl(11,81) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  6,  9,  1, &macros[ 898] fl(11,89) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  5,  4,  1, &macros[ 908] fl(11,97) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  6,  4,  1, &macros[ 919] fl(11,106) }
};
/* assign.t */
PAT p12[] = {
 {  9,  1, NONE, NONE, NONE, NONE,  1,  1, 12,  0, &macros[ 931] fl(12,18) },
 {  9,  2, NONE, NONE, NONE, NONE,  1,  2, 12,  0, &macros[ 934] fl(12,23) },
 {  9,  9, NONE, NONE, NONE, NONE,  1,  9, 12,  0, &macros[ 937] fl(12,33) },
 {  9,  1, NONE, NONE, NONE, NONE,  3,  1, 13,  1, &macros[ 944] fl(12,44) },
 {  9,  1, NONE, NONE, NONE, NONE,  1,  1,  3,  1, &macros[ 944] fl(12,48) },
 {  9,  1, NONE, NONE, NONE, NONE,  1,  1,  9,  1, &macros[ 944] fl(12,52) },
 {  9,  2, NONE, NONE, NONE, NONE,  3,  2, 13,  2, &macros[ 948] fl(12,58) },
 {  9,  2, NONE, NONE, NONE, NONE,  1,  2,  3,  2, &macros[ 948] fl(12,62) },
 {  9,  2, NONE, NONE, NONE, NONE,  1,  2,  9,  2, &macros[ 948] fl(12,66) },
 {  9,  1, NONE, NONE, NONE, NONE,  1,  2, 12,  1, &macros[ 952] fl(12,76) },
 {  9,  1, NONE, NONE, NONE, NONE,  1,  2,  9,  1, &macros[ 954] fl(12,88) },
 {  9, 14, NONE, NONE, NONE, NONE,  3, 14,  2, 14, &macros[ 956] fl(12,103) },
 {  9, 14, NONE, NONE, NONE, NONE,  1, 14,  3, 14, &macros[ 960] fl(12,114) },
 {  9, 14, NONE, NONE, NONE, NONE,  1, 14,  9, 14, &macros[ 962] fl(12,119) },
 {  3, 15,  RQ0, NONE, NONE,  RQ0,  1, 15,  4, 15, &macros[ 973] fl(12,131) },
 { 10,  1, ANYR, NONE, ANYR, TEMP,  1,  5,  4,  1, &macros[ 983] fl(12,148) },
 { 11,  1, ANYL, NONE, ANYL, TEMP,  1,  5,  4,  1, &macros[ 983] fl(12,152) },
 { 10,  1, ANYR, NONE, ANYR, TEMP,  1,  6,  4,  1, &macros[ 985] fl(12,161) },
 { 11,  1, ANYL, NONE, ANYL, TEMP,  1,  6,  4,  1, &macros[ 985] fl(12,165) },
 { 10,  1, ANYR, NONE, ANYR, TEMP,  1,  5,  4,  2, &macros[ 987] fl(12,180) },
 { 11,  1, ANYL, NONE, ANYL, TEMP,  1,  5,  4,  2, &macros[ 987] fl(12,184) },
 { 10,  1, ANYR, NONE, ANYR, TEMP,  1,  6,  4,  2, &macros[ 989] fl(12,193) },
 { 11,  1, ANYL, NONE, ANYL, TEMP,  1,  6,  4,  2, &macros[ 989] fl(12,197) },
 { 10,  1, ANYR, NONE, ANYR, TEMP,  1,  1,  4,  1, &macros[ 991] fl(12,210) },
 { 11,  1, ANYL, NONE, ANYL, TEMP,  1,  1,  4,  1, &macros[ 991] fl(12,214) },
 { 10,  2, ANYR, NONE, ANYR, TEMP,  1,  2,  4,  2, &macros[ 999] fl(12,222) },
 { 11,  2, ANYL, NONE, ANYL, TEMP,  1,  2,  4,  2, &macros[ 999] fl(12,226) },
 { 12, 14, ANYR, NONE, ANYR, TEMP,  1, 14,  4, 14, &macros[1005] fl(12,237) },
 { 11, 14, ANYL, NONE, ANYL, TEMP,  1, 14,  4, 14, &macros[1005] fl(12,241) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  7,  2,  1, &macros[1007] fl(12,253) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  8,  2,  1, &macros[1017] fl(12,263) }
};
/* asub.t */
PAT p13[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  2,  1, &macros[1029] fl(13,6) },
 {  2,  2, ANYR, NONE, NONE, TEMP,  1,  2,  2,  2, &macros[1035] fl(13,15) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  2,  3, &macros[1042] fl(13,25) },
 {  3,  4, ANYR, NONE, NONE, TEMP,  3,  4,  2,  1, &macros[1048] fl(13,37) },
 {  3,  4, ANYR, NONE, NONE, TEMP,  1,  4,  2,  1, &macros[1051] fl(13,46) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  5,  2,  1, &macros[1057] fl(13,58) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  6,  2,  1, &macros[1064] fl(13,69) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  7,  2,  1, &macros[1071] fl(13,83) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  8,  2,  1, &macros[1077] fl(13,95) }
};
/* axor.t */
PAT p14[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  2,  1, &macros[1084] fl(14,6) },
 {  2,  2, ANYR, NONE, NONE, TEMP,  1,  2,  2,  2, &macros[1090] fl(14,15) },
 {  3,  3, ANYR, NONE, NONE, TEMP,  1,  3,  2,  3, &macros[1097] fl(14,25) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  5,  2,  1, &macros[1108] fl(14,37) },
 {  2,  1, ANYR, NONE, NONE, TEMP,  1,  6,  2,  1, &macros[1115] fl(14,48) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  7,  2,  1, &macros[1122] fl(14,63) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  8,  2,  1, &macros[1128] fl(14,72) }
};
/* bef.t */
PAT p15[] = {
 {  1,  1, ANYR, NONE, NONE, TEMP,  1,  1,  5,  0, &macros[1135] fl(15,9) },
 {  7,  1, ANYL, NONE, NONE, TEMP,  1,  1,  5,  0, &macros[1135] fl(15,13) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  5,  5,  0, &macros[1145] fl(15,30) },
 {  7,  1, ANYL, NONE, NONE, TEMP,  1,  5,  5,  0, &macros[1145] fl(15,34) },
 {  3,  1, ANYR, NONE, NONE, TEMP,  1,  6,  5,  0, &macros[1159] fl(15,42) },
 {  7,  1, ANYL, NONE, NONE, TEMP,  1,  6,  5,  0, &macros[1159] fl(15,46) },
 { 13,  4, ANYR, NONE, NONE, TEMP,  3,  4,  9,  1, &macros[1173] fl(15,59) },
 { 14,  4, ANYL, NONE, NONE, TEMP,  3,  4,  9,  1, &macros[1181] fl(15,66) },
 { 13,  4, ANYR, NONE, NONE, TEMP,  1,  4,  9,  1, &macros[1184] fl(15,76) },
 { 14,  4, ANYL, NONE, NONE, TEMP,  1,  4,  9,  1, &macros[1184] fl(15,80) }
};
/* blkmv.t */
PAT p16[] = {
 {  9,  1, ANYR, PAIR, PAIR, NONE,  4,  4,  4,  4, &macros[1190] fl(16,9) }
};
/* div.t */
PAT p17[] = {
 {  3, 10,  RR0,   R1, NONE,   R1,  4,  1,  9,  1, &macros[1198] fl(17,11) },
 {  3, 10,  RQ0,   R3, NONE,   R3,  4,  1,  2,  3, &macros[1206] fl(17,21) },
 {  3, 11,  RR0,   R1, NONE,   R1,  4,  1,  2,  1, &macros[1219] fl(17,29) },
 {  3, 12,  RQ0,  RR2, NONE,  RR2,  4,  3,  9,  3, &macros[1224] fl(17,42) },
 {  3, 12,  RQ0,  RR2, NONE,  RR2,  4,  3, 10,  3, &macros[1231] fl(17,54) },
 {  3, 13,  RQ0,  RR2, NONE,  RR2,  4,  3,  2,  3, &macros[1274] fl(17,70) }
};
/* leaves.t */
PAT p18[] = {
 { 15,  1, NONE, NONE, NONE, NONE, 10,  1,  0,  0, &macros[1277] fl(18,27) },
 { 15, 16, NONE, NONE, NONE, NONE, 10,  2,  0,  0, &macros[1282] fl(18,43) },
 { 16,  1, NONE, NONE, NONE, NONE, 10,  1,  0,  0, &macros[1285] fl(18,53) },
 { 15,  9, NONE, NONE, NONE, NONE, 10,  9,  0,  0, &macros[1291] fl(18,66) },
 { 15,  9, ANYR, ANYR, NONE, TEMP,  4,  9,  0,  0, &macros[1294] fl(18,73) },
 { 16,  9, NONE, NONE, NONE, NONE, 10,  9,  0,  0, &macros[1298] fl(18,83) },
 { 16,  9, ANYR, ANYR, NONE, TEMP,  4,  9,  0,  0, &macros[1305] fl(18,90) },
 { 13,  1, ANYR, NONE, NONE, TEMP, 12,  0,  0,  0, &macros[1309] fl(18,102) },
 {  7,  1, ANYL, NONE, NONE, TEMP, 12,  0,  0,  0, &macros[1309] fl(18,106) },
 { 13,  9, ANYR, NONE, NONE, TEMP, 12,  0,  0,  0, &macros[1312] fl(18,112) },
 {  9, 17, NONE, NONE, NONE, NONE, 14, 17,  0,  0, &macros[1316] fl(18,125) },
 {  9, 18, NONE, NONE, NONE, NONE, 14, 18,  0,  0, &macros[1317] fl(18,137) },
 { 13,  1, ANYR, NONE, NONE, TEMP,  2,  1,  0,  0, &macros[1318] fl(18,146) },
 {  7,  1, ANYL, NONE, NONE, TEMP,  2,  1,  0,  0, &macros[1318] fl(18,150) },
 { 13,  2, ANYR, NONE, NONE, TEMP,  2,  2,  0,  0, &macros[1321] fl(18,156) },
 { 17,  4, ANYR, ANYR, NONE, TEMP,  4,  4,  0,  0, &macros[1325] fl(18,168) },
 {  5,  4, ANYL, ANYL, NONE, TEMP,  4,  4,  0,  0, &macros[1325] fl(18,172) },
 { 13,  4, ANYR, NONE, NONE, TEMP, 15,  4,  0,  0, &macros[1326] fl(18,191) },
 {  7,  4, ANYL, NONE, NONE, TEMP, 15,  4,  0,  0, &macros[1326] fl(18,195) },
 { 13,  4, ANYR, NONE, NONE, TEMP,  2,  9,  0,  0, &macros[1333] fl(18,203) },
 {  7,  4, ANYL, NONE, NONE, TEMP,  2,  9,  0,  0, &macros[1333] fl(18,207) },
 { 13,  4, ANYR, NONE, NONE, TEMP,  2,  1,  0,  0, &macros[1335] fl(18,218) },
 {  7,  4, ANYL, NONE, NONE, TEMP,  2,  1,  0,  0, &macros[1335] fl(18,222) },
 { 13,  4, ANYR, NONE, NONE, TEMP, 16,  1,  0,  0, &macros[1343] fl(18,231) },
 {  7,  4, ANYL, NONE, NONE, TEMP, 16,  1,  0,  0, &macros[1343] fl(18,235) },
 { 18,  4, ANYR, NONE, NONE, NONE, 15,  4,  0,  0, &macros[1345] fl(18,244) },
 { 18,  4, ANYR, NONE, NONE, NONE,  2,  9,  0,  0, &macros[1358] fl(18,252) },
 { 18,  3, ANYR, NONE, NONE, NONE,  2,  3,  0,  0, &macros[1363] fl(18,261) },
 { 18,  1, NONE, NONE, NONE, NONE,  9,  1,  0,  0, &macros[1365] fl(18,272) },
 { 18,  1, ANYR, NONE, NONE, NONE,  2,  1,  0,  0, &macros[1368] fl(18,280) },
 { 18,  2, ANYR, NONE, NONE, NONE,  2,  2,  0,  0, &macros[1374] fl(18,289) },
 { 13,  3, ANYR, NONE, NONE, TEMP,  9,  3,  0,  0, &macros[1378] fl(18,300) },
 { 13,  3, ANYR, NONE, NONE, TEMP, 10,  3,  0,  0, &macros[1389] fl(18,308) },
 { 18, 15,  RQ0, NONE, NONE, NONE, 10, 15,  0,  0, &macros[1391] fl(18,318) },
 { 13, 15,  RQ0, NONE, NONE,  RQ0, 10, 15,  0,  0, &macros[1408] fl(18,330) },
 { 13, 15,  RQ0, NONE, NONE,  RQ0,  4, 15,  0,  0, &macros[1419] fl(18,337) },
 { 13, 15,  RQ0, NONE, NONE,  RQ0, 10, 19,  0,  0, &macros[1419] fl(18,350) },
 { 13, 19,  RR0, NONE, NONE,  RR0,  4, 15,  0,  0, &macros[1427] fl(18,358) },
 { 13, 19,  RR0, NONE, NONE,  RR0, 10, 15,  0,  0, &macros[1431] fl(18,364) },
 { 13,  1, ANYR, NONE, NONE, TEMP, 10,  5,  0,  0, &macros[1443] fl(18,377) },
 { 13,  1, ANYR, NONE, NONE, TEMP, 10,  6,  0,  0, &macros[1450] fl(18,384) },
 { 17,  1, ANYR, ANYR, NONE, TEMP,  4,  5,  0,  0, &macros[1457] fl(18,396) },
 { 17,  1, ANYR, ANYR, NONE, TEMP,  4,  6,  0,  0, &macros[1459] fl(18,402) },
 { 13,  3, ANYR, NONE, NONE, TEMP,  2, 11,  0,  0, &macros[1461] fl(18,413) },
 { 13,  3, ANYR, NONE, NONE, TEMP,  2, 10,  0,  0, &macros[1468] fl(18,420) },
 { 13,  3, ANYR, NONE, NONE, TEMP, 10,  5,  0,  0, &macros[1470] fl(18,430) },
 { 13,  3, ANYR, NONE, NONE, TEMP, 10,  6,  0,  0, &macros[1480] fl(18,438) },
 { 13,  1, ANYR, NONE, NONE, TEMP,  2,  9,  0,  0, &macros[1491] fl(18,449) },
 { 13,  5, ANYR, NONE, NONE, TEMP, 17,  1,  0,  0, &macros[1494] fl(18,468) },
 { 13,  6, ANYR, NONE, NONE, TEMP, 17,  1,  0,  0, &macros[1497] fl(18,475) },
 { 13,  5, ANYR, NONE, NONE, TEMP, 10,  1,  0,  0, &macros[1500] fl(18,482) },
 { 13,  6, ANYR, NONE, NONE, TEMP, 10,  1,  0,  0, &macros[1506] fl(18,489) },
 { 13,  5, ANYR, NONE, NONE, TEMP, 10,  9,  0,  0, &macros[1512] fl(18,496) },
 { 17,  5, ANYR, ANYR, NONE, TEMP,  4,  1,  0,  0, &macros[1518] fl(18,507) },
 { 17,  6, ANYR, ANYR, NONE, TEMP,  4,  1,  0,  0, &macros[1520] fl(18,513) },
 { 17,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  0,  0, &macros[1522] fl(18,529) },
 { 17,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  0,  0, &macros[1523] fl(18,535) },
 { 13,  4, ANYR, NONE, NONE, TEMP,  2,  9,  0,  0, &macros[1524] fl(18,547) }
};
/* mul.t */
PAT p19[] = {
 {  3,  1,  RR0,   R1, NONE,   R1,  4,  1,  2,  1, &macros[1527] fl(19,9) },
 {  3,  3,  RQ0,  RR2, NONE,  RR2,  4,  3,  2,  3, &macros[1531] fl(19,17) }
};
/* neg.t */
PAT p20[] = {
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  0,  0, &macros[1535] fl(20,5) },
 {  4,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  0,  0, &macros[1538] fl(20,12) },
 {  4,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  0,  0, &macros[1541] fl(20,21) }
};
/* not.t */
PAT p21[] = {
 {  8,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  0,  0, &macros[1555] fl(21,5) },
 {  8,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  0,  0, &macros[1558] fl(21,12) },
 {  8,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  0,  0, &macros[1561] fl(21,20) }
};
/* or.t */
PAT p22[] = {
 {  8,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  2,  1, &macros[1569] fl(22,5) },
 {  8,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  2,  2, &macros[1572] fl(22,12) },
 {  6,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  2,  3, &macros[1575] fl(22,20) },
 {  6,  4, ANYR, ANYR, NONE, TEMP,  4,  9, 18,  3, &macros[1584] fl(22,31) },
 {  6,  4, ANYR, ANYR, NONE, TEMP,  4,  9,  2,  3, &macros[1586] fl(22,38) }
};
/* relop.t */
PAT p23[] = {
 { 19,  0, NONE, NONE, NONE, NONE, 19,  1,  9,  1, &macros[1588] fl(23,27) },
 { 19,  0, NONE, NONE, NONE, NONE, 16,  1,  2,  1, &macros[1588] fl(23,31) },
 { 20,  0, ANYR, ANYR, NONE, NONE,  4,  1,  2,  1, &macros[1594] fl(23,38) },
 { 19,  0, NONE, NONE, NONE, NONE, 10,  2,  9,  2, &macros[1598] fl(23,59) },
 { 20,  0, ANYR, ANYR, NONE, NONE,  4,  2,  2,  2, &macros[1602] fl(23,66) },
 { 15,  0, NONE, NONE, NONE, NONE, 10,  3, 12,  0, &macros[1608] fl(23,81) },
 { 15,  0, NONE, ANYR, NONE, NONE,  4,  3, 12,  0, &macros[1611] fl(23,89) },
 { 16,  0, NONE, ANYR, NONE, NONE,  4,  3, 12,  0, &macros[1614] fl(23,97) },
 { 19,  0, NONE, ANYR, NONE, NONE,  4,  3,  2,  3, &macros[1617] fl(23,104) },
 { 15,  0, NONE, NONE, NONE, NONE, 10,  4, 12,  0, &macros[1620] fl(23,121) },
 { 15,  0, NONE, ANYR, NONE, NONE,  4,  4, 12,  0, &macros[1623] fl(23,127) },
 { 16,  0, NONE, ANYR, NONE, NONE,  4,  4, 12,  0, &macros[1626] fl(23,133) },
 { 19,  0, NONE, ANYR, NONE, NONE,  4,  4,  2, 20, &macros[1629] fl(23,142) }
};
/* rem.t */
PAT p24[] = {
 {  3, 10,  RR0,   R0, NONE,   R0,  2,  1,  9,  1, &macros[1631] fl(24,7) },
 {  3, 10,  RQ0,   R1, NONE,   R1,  2,  1,  2,  3, &macros[1638] fl(24,18) },
 {  3, 11,  RR0,   R0, NONE,   R0,  2,  1,  2,  1, &macros[1645] fl(24,27) },
 {  3, 12,  RQ0,  RR0, NONE,  RR0,  2,  3,  9,  3, &macros[1652] fl(24,38) },
 {  3, 12,  RQ0,  RR0, NONE,  RR0,  2,  3, 10,  3, &macros[1659] fl(24,49) },
 {  3, 13,  RQ0,  RR0, NONE,  RR0,  2,  3,  2,  3, &macros[1685] fl(24,64) }
};
/* shl.t */
PAT p25[] = {
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  9,  1, &macros[1692] fl(25,9) },
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  4,  1, &macros[1695] fl(25,16) },
 {  4,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  9,  1, &macros[1698] fl(25,24) },
 {  4,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  4,  1, &macros[1701] fl(25,31) },
 {  1,  4, ANYR, ANYR, NONE, TEMP,  4,  1,  9,  1, &macros[1704] fl(25,47) },
 {  1,  4, ANYR, ANYR, NONE, TEMP,  4,  1,  4,  1, &macros[1716] fl(25,56) }
};
/* shr.t */
PAT p26[] = {
 {  4, 11, ANYR, ANYR, NONE, TEMP,  4, 11,  9,  1, &macros[1723] fl(26,12) },
 {  4, 10, ANYR, ANYR, NONE, TEMP,  4,  1,  9,  1, &macros[1726] fl(26,19) },
 {  4, 11, ANYR, ANYR, NONE, TEMP,  4, 11,  4,  1, &macros[1728] fl(26,26) },
 {  4, 10, ANYR, ANYR, NONE, TEMP,  4,  1,  4,  1, &macros[1733] fl(26,34) },
 {  4, 13, ANYR, ANYR, NONE, TEMP,  4, 13,  9,  1, &macros[1737] fl(26,44) },
 {  4, 12, ANYR, ANYR, NONE, TEMP,  4, 12,  9,  1, &macros[1740] fl(26,51) },
 {  4, 13, ANYR, ANYR, NONE, TEMP,  4, 13,  4,  1, &macros[1742] fl(26,58) },
 {  4, 12, ANYR, ANYR, NONE, TEMP,  4, 12,  4,  1, &macros[1747] fl(26,66) }
};
/* sub.t */
PAT p27[] = {
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  5,  0, &macros[1751] fl(27,7) },
 {  4,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  2,  1, &macros[1754] fl(27,15) },
 {  4,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  2,  2, &macros[1757] fl(27,23) },
 {  4,  9, ANYR, ANYR, NONE, TEMP,  4,  9,  2,  3, &macros[1760] fl(27,35) },
 {  5,  9, ANYL, ANYL, NONE, TEMP,  4,  9,  2,  3, &macros[1760] fl(27,39) },
 {  6,  4, ANYR, ANYR, NONE, TEMP,  4,  4,  2,  1, &macros[1763] fl(27,51) },
 {  5,  4, ANYL, ANYL, NONE, TEMP,  4,  4,  2,  1, &macros[1763] fl(27,55) }
};
/* xor.t */
PAT p28[] = {
 {  8,  1, ANYR, ANYR, NONE, TEMP,  4,  1,  2,  1, &macros[1766] fl(28,5) },
 {  8,  2, ANYR, ANYR, NONE, TEMP,  4,  2,  2,  2, &macros[1769] fl(28,12) },
 {  6,  3, ANYR, ANYR, NONE, TEMP,  4,  3,  2,  3, &macros[1772] fl(28,20) }
};
PATX patx[] = {
	p3,	13,
	p27,	7,
	p19,	2,
	p17,	6,
	p24,	6,
	p7,	5,
	p22,	5,
	p28,	3,
	p25,	6,
	p26,	8,
	p1,	9,
	p13,	9,
	p6,	4,
	p4,	8,
	p9,	8,
	p2,	7,
	p8,	7,
	p14,	7,
	p10,	8,
	p11,	12,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	p23,	13,
	NULL,	0,
	NULL,	0,
	p20,	3,
	p21,	3,
	NULL,	0,
	NULL,	0,
	NULL,	0,
	p15,	10,
	p15,	10,
	p5,	12,
	p5,	12,
	NULL,	0,
	NULL,	0,
	NULL,	0,
	NULL,	0,
	p18,	58,
	p18,	58,
	NULL,	0,
	NULL,	0,
	p12,	31,
	NULL,	0,
	NULL,	0,
	NULL,	0,
	p18,	58,
	p18,	58,
	p16,	1
};
PATFLAG	patcache[] = {
	PEFFECT|PRVALUE|PSREL,
	PEFFECT|PRVALUE|PEREL,
	PEFFECT|PRVALUE,
	PEFFECT|PRVALUE|PSREL|P_SLT,
	PLVALUE|P_SLT,
	PEFFECT|PRVALUE|P_SLT,
	PLVALUE,
	PEFFECT|PRVALUE|PEREL|P_SLT,
	PEFFECT,
	PEFFECT|PRVALUE|PEREL|P_SRT,
	PLVALUE|P_SRT,
	PEFFECT|PRVALUE|P_SRT,
	PRVALUE,
	PEFFECT|PLVALUE,
	PEREL,
	PNEREL,
	PRVALUE|P_SLT,
	PFNARG,
	PREL,
	PREL|P_SLT
};
int patcsize=sizeof(patcache)/sizeof(PATFLAG);
TYPESET	typecache[] = {
	WORD,
	BYTE,
	LONG,
	LPTX,
	FS8,
	FU8,
	FFLD16,
	FFLD8,
	LONG|LPTX,
	UWORD,
	FS16,
	FU32,
	FS32,
	LONG|LPTX|FLT,
	DBL,
	WORD|BYTE,
	NFLT,
	FLT|DBL,
	FLT,
	LONG|LPTX|WORD
};
FLAG	flagcache[] = {
	T_ADR|T_LV,
	T_ADR|T_IMM,
	T_REG|T_MMX,
	T_TREG,
	T_1|T_MMX,
	T_2|T_MMX,
	T_SREG|T_MMX,
	T_REG,
	T_IMM|T_MMX,
	T_ADR,
	T_UHS|T_MMX,
	T_0|T_MMX,
	T_EASY|T_MMX,
	T_RREG|T_MMX,
	T_LSS|T_MMX,
	T_RREG|T_LREG|T_MMX,
	T_NBH|T_MMX,
	T_UHC|T_MMX,
	T_ADR|T_MMX
};
ival_t	ivalcache[] = {
	0x1,
	0x2,
	0x0,
	0x9
};
lval_t	lvalcache[] = {
	0x1L
};
char	*gidcache[] = {
	"dfpack",
	"fdpack"
};
#if !TINY
char	*namecache[] = {
	"prefac.f",
	"aadd.t",
	"aand.t",
	"add.t",
	"adiv.t",
	"aft.t",
	"amul.t",
	"and.t",
	"aor.t",
	"arem.t",
	"ashl.t",
	"ashr.t",
	"assign.t",
	"asub.t",
	"axor.t",
	"bef.t",
	"blkmv.t",
	"div.t",
	"leaves.t",
	"mul.t",
	"neg.t",
	"not.t",
	"or.t",
	"relop.t",
	"rem.t",
	"shl.t",
	"shr.t",
	"sub.t",
	"xor.t"
};
#endif
