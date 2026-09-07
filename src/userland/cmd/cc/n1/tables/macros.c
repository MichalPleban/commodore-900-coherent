#ifdef vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif
char macros[] = {
/* prefac.f */
/* aadd.t */
/*    0 */
	ZLD, M_R, M_AL,
	ZADD, M_R, M_AR,
	ZLD, M_AL, M_R,
	M_IFR, M_REL0, M_LAB, M_ENDIF,
	M_END,
/*   14 */
	ZLDB, M_LO, M_R, M_AL,
	ZADDB, M_LO, M_R, M_AR,
	ZLDB, M_AL, M_LO,
	M_JMP1, 0x08,		/* 8 */
/*   27 */
	ZLDL, M_R, M_AL,
	ZADDL, M_R, M_AR,
	ZLDL, M_AL, M_R,
	M_END,
/*   37 */
	ZADD, M_LO, M_AL, M_AR,
	M_IFV, ZLDL, M_R, M_AL, M_ENDIF,
	M_END,
/*   47 */
	ZLDL, M_R, M_AL,
	ZADD, M_LO,
	M_JMP1, 0x1F,		/* 31 */
/*   54 */
	ZLDB, M_LO, M_R, M_AL,
	ZADD, M_R, M_AR,
	ZLDB, M_AL, M_LO, M_R,
	M_IFV, ZEXTSB, M_R, M_ENDIF,
	M_IFR, ZORB, M_LO, M_R, M_LO, M_R, M_ENDIF,
	M_JMP1, 0x09,		/* 9 */
/*   78 */
	ZLDB, M_LO, M_R, M_AL,
	ZADD, M_R, M_AR,
	ZLDB, M_AL, M_LO, M_R,
	M_IFV, ZCLRB, M_HI,
	M_JMP1, 0x43,		/* 67 */
/*   94 */
	ZLD, M_R, M_AL,
	ZADD, M_R, M_AR,
	ZXOR, M_R, M_AL,
	ZAND, M_R, M_LO, M_EMASK,
	ZXOR, M_R, M_AL,
	ZLD, M_AL, M_R,
	M_IFV, ZAND, M_R, M_LO, M_EMASK, M_ENDIF,
	M_END,
/*  120 */
	ZLDB, M_LO, M_R, M_AL,
	ZADDB, M_LO, M_R, M_LO, M_AR,
	ZXORB, M_LO, M_R, M_AL,
	ZANDB, M_LO, M_R, M_LO, M_EMASK,
	ZXORB, M_LO, M_R, M_AL,
	ZLDB, M_AL, M_LO, M_R,
	M_IFV, ZANDB, M_LO,
	M_JMP1, 0x73,		/* 115 */
/* aand.t */
/*  151 */
	ZLD, M_R, M_AL,
	ZAND,
	M_JMP1, 0x04,		/* 4 */
/*  157 */
	ZLDB, M_LO, M_R, M_AL,
	ZANDB,
	M_JMP1, 0x13,		/* 19 */
/*  164 */
	ZLDL, M_R, M_AL,
	ZAND, M_LO, M_R, M_LO, M_AR,
	ZAND, M_HI, M_R, M_HI,
	M_JMP1, 0x20,		/* 32 */
/*  178 */
	ZLDB, M_LO, M_R, M_AL,
	ZAND,
	M_JMP1, 0x3B,		/* 59 */
/*  185 */
	ZLDB, M_LO, M_R, M_AL,
	ZAND,
	M_JMP1, 0x53,		/* 83 */
/*  192 */
	ZLD, M_R, M_AL,
	ZAND, M_R, M_AR,
	M_JMP1, 0x6E,		/* 110 */
/*  200 */
	ZLDB, M_LO, M_R, M_AL,
	ZANDB, M_LO, M_R, M_LO, M_AR,
	M_JMP1, 0x8E,		/* 142 */
/* add.t */
/*  211 */
	ZINC, M_R, M_ICON, 0x01,
	M_JMP1, 0x09,		/* 9 */
/*  217 */
	ZINC, M_R, M_ICON, 0x02,
	M_JMP1, 0x09,		/* 9 */
/*  223 */
	ZADD, M_R, M_AR,
	M_JMP1, 0x09,		/* 9 */
/*  228 */
	ZADDB,
	M_JMP1, 0xE0,		/* 224 */
/*  231 */
	ZADDL,
	M_JMP1, 0xE0,		/* 224 */
/*  234 */
	ZADD, M_LO, M_R, M_AR,
	M_END,
/*  239 */
	ZADD, M_LO, M_R, M_AR,
	ZLD, M_HI, M_R, M_REGNO, R14,
	M_END,
/*  249 */
	ZADD, M_LO, M_R, M_LO, M_AR,
	ZLD, M_HI, M_R, M_HI, M_AR,
	M_END,
/* adiv.t */
/*  260 */
	ZLD, M_REGNO, R1, M_AL,
	ZCLR, M_REGNO, R0,
	ZDIV, M_REGNO, RR0, M_AR,
	ZLD, M_AL, M_REGNO, R1,
	M_END,
/*  276 */
	ZLD, M_REGNO, R3, M_AL,
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZCLR, M_REGNO, R2,
	ZDIVL, M_REGNO, RQ0, M_AR,
	ZLD, M_AL, M_REGNO, R3,
	M_END,
/*  297 */
	ZLD, M_REGNO, R1, M_AL,
	ZEXTS, M_REGNO, RR0,
	M_JMPB, 0x25,		/* 267 */
/*  306 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZDIVL, M_REGNO, RQ0, M_AR,
	ZLDL, M_AL, M_REGNO, RR2,
	M_END,
/*  324 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZTESTL, M_AR,
	ZJRPL, M_LAB0,
	ZLDL, M_REGNO, RR0, M_REGNO, RR2,
	ZSUBL, M_REGNO, RR2, M_REGNO, RR2,
	ZCPL, M_REGNO, RR0, M_AR,
	ZJRULT, M_LAB1,
	ZSUBL, M_REGNO, RR0, M_AR,
	ZLD, M_REGNO, R3, M_ICON, 0x01,
	ZJP, M_LAB1,
	M_DLAB0, ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZDIVL, M_REGNO, RQ0, M_AR,
	M_DLAB1,
	M_JMPB, 0x33,		/* 319 */
/*  372 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZEXTSL,
	M_JMPB, 0x40,		/* 313 */
/*  379 */
	ZLDB, M_LO, M_REGNO, R1, M_AL,
	ZEXTSB, M_REGNO, R1,
	ZEXTS, M_REGNO, RR0,
	ZDIV, M_REGNO, RR0, M_AR,
	ZLDB, M_AL, M_LO, M_REGNO, R1,
	M_IFV, ZEXTSB, M_REGNO, R1, M_ENDIF,
	M_END,
/*  405 */
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZLDB, M_LO, M_REGNO, R1, M_AL,
	ZDIV, M_REGNO, RR0, M_AR,
	ZLDB, M_AL, M_LO, M_REGNO, R1,
	M_IFV, ZCLRB, M_HI,
	M_JMPB, 0x1A,		/* 401 */
/* aft.t */
/*  429 */
	M_IFV, ZLD, M_R, M_AL, M_ENDIF,
	M_OP1, M_AL, M_ICON, 0x01,
	M_END,
/*  439 */
	M_IFV, ZLDB, M_LO, M_R, M_AL, M_ENDIF,
	M_IFV, ZEXTSB, M_R, M_ENDIF,
	M_TL,
	M_JMPB, 0x10,		/* 434 */
/*  452 */
	M_IFV, ZLDB, M_LO, M_R, M_AL, M_ENDIF,
	M_IFV, ZCLRB, M_HI,
	M_JMPB, 0x0E,		/* 447 */
/*  463 */
	M_IFV, ZLDL, M_R, M_AL, M_ENDIF,
	M_OP1, M_LO, M_AL, M_AR,
	M_END,
/*  473 */
	ZLDL, M_R, M_AL,
	M_IFV, ZPUSHL, M_R, M_ENDIF,
	M_OP1, M_LO, M_R, M_AR,
	ZLDL, M_AL, M_R,
	M_IFV, ZPOPL, M_R, M_ENDIF,
	M_END,
/*  492 */
	ZLDL, M_R, M_AL,
	M_OP0, M_R, M_LCON, 0x01,
	ZLDL, M_AL, M_R,
	M_IFV, M_OP2, M_R, M_LCON, 0x01, M_ENDIF,
	M_END,
/* amul.t */
/*  509 */
	ZLD, M_REGNO, R1, M_AL,
	ZMULT,
	M_JMPB, 0xF6,		/* 268 */
/*  516 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZMULTL,
	M_JMPB, 0xCD,		/* 316 */
/*  523 */
	ZLDB, M_LO, M_REGNO, R1, M_AL,
	ZMULT,
	M_JMPB, 0x8A,		/* 391 */
/*  531 */
	ZLDB, M_LO, M_REGNO, R1, M_AL,
	ZMULT,
	M_JMPB, 0x79,		/* 416 */
/* and.t */
/*  539 */
	ZAND,
	M_JMP1, 0xE0,		/* 224 */
/*  542 */
	ZANDB,
	M_JMP1, 0xE0,		/* 224 */
/*  545 */
	ZAND, M_HI, M_R, M_HI, M_AR,
	ZAND, M_LO, M_R, M_LO, M_AR,
	M_END,
/*  556 */
	M_JMPB, 0x06,		/* 550 */
/*  558 */
	M_JMPB, 0x0D,		/* 545 */
/* aor.t */
/*  560 */
	ZLD, M_R, M_AL,
	ZOR,
	M_JMP1, 0x04,		/* 4 */
/*  566 */
	ZLDB, M_LO, M_R, M_AL,
	ZORB,
	M_JMP1, 0x13,		/* 19 */
/*  573 */
	ZLDL, M_R, M_AL,
	ZOR, M_LO, M_R, M_LO, M_AR,
	ZOR,
	M_JMP1, 0xAD,		/* 173 */
/*  584 */
	ZLDB, M_LO, M_R, M_AL,
	ZOR,
	M_JMP1, 0x3B,		/* 59 */
/*  591 */
	ZLDB, M_LO, M_R, M_AL,
	ZOR,
	M_JMP1, 0x53,		/* 83 */
/*  598 */
	ZLD, M_R, M_AL,
	ZOR,
	M_JMP1, 0xC4,		/* 196 */
/*  604 */
	ZLDB, M_LO, M_R, M_AL,
	ZORB,
	M_JMP1, 0xCD,		/* 205 */
/* arem.t */
/*  611 */
	ZLD, M_REGNO, R1, M_AL,
	ZCLR, M_REGNO, R0,
	ZDIV, M_REGNO, RR0, M_AR,
	ZLD, M_AL, M_REGNO, R0,
	M_END,
/*  627 */
	ZLD, M_REGNO, R3, M_AL,
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZCLR, M_REGNO, R2,
	ZDIVL, M_REGNO, RQ0,
	M_JMP2, 0x0E, 0x01,	/* 270 */
/*  645 */
	ZLD, M_REGNO, R1, M_AL,
	ZEXTS, M_REGNO, RR0,
	M_JMPB, 0x22,		/* 618 */
/*  654 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZDIVL, M_REGNO, RQ0, M_AR,
	ZLDL, M_AL, M_REGNO, RR0,
	M_END,
/*  672 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZTESTL, M_AR,
	ZJRPL, M_LAB0,
	ZLDL, M_REGNO, RR0, M_REGNO, RR2,
	ZCPL, M_REGNO, RR0, M_AR,
	ZJRULT, M_LAB1,
	ZSUBL, M_REGNO, RR0, M_AR,
	ZJP, M_LAB1,
	M_DLAB0, ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZDIVL, M_REGNO, RQ0, M_AR,
	M_DLAB1,
	M_JMPB, 0x29,		/* 667 */
/*  710 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZEXTSL,
	M_JMPB, 0x36,		/* 661 */
/*  717 */
	ZLDB, M_LO, M_REGNO, R1, M_AL,
	ZEXTSB, M_REGNO, R1,
	ZEXTS, M_REGNO, RR0,
	ZDIV, M_REGNO, RR0, M_AR,
	ZLDB, M_AL, M_LO, M_REGNO, R0,
	M_IFV, ZEXTSB, M_REGNO, R0, M_ENDIF,
	M_END,
/*  743 */
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZLDB, M_LO, M_REGNO, R1, M_AL,
	ZDIV, M_REGNO, RR0, M_AR,
	ZLDB, M_AL, M_LO, M_REGNO, R0,
	M_IFV, ZCLRB, M_HI,
	M_JMPB, 0x1A,		/* 739 */
/* ashl.t */
/*  767 */
	ZLD, M_R, M_AL,
	ZSLL,
	M_JMP1, 0x04,		/* 4 */
/*  773 */
	ZLD, M_R, M_AL,
	ZSDL,
	M_JMP1, 0x04,		/* 4 */
/*  779 */
	ZLDL, M_R, M_AL,
	ZSLLL,
	M_JMP1, 0x1F,		/* 31 */
/*  785 */
	ZLDL, M_R, M_AL,
	ZSDLL,
	M_JMP1, 0x1F,		/* 31 */
/*  791 */
	ZLDB, M_LO, M_R, M_AL,
	ZSLL, M_R, M_AR,
	ZLDB, M_AL, M_LO, M_R,
	M_IFV, ZEXTSB, M_R, M_ENDIF,
	M_END,
/*  807 */
	ZLDB, M_LO, M_R, M_AL,
	ZSLL, M_R, M_AR,
	ZLDB, M_AL, M_LO, M_R,
	M_IFV, ZCLRB, M_HI, M_R, M_ENDIF,
	M_END,
/*  824 */
	ZLDB, M_LO, M_R, M_AL,
	ZSDL,
	M_JMPB, 0x21,		/* 796 */
/*  831 */
	ZLDB, M_LO, M_R, M_AL,
	ZSDL,
	M_JMPB, 0x18,		/* 812 */
/* ashr.t */
/*  838 */
	ZLD, M_R, M_AL,
	ZSLA,
	M_JMP1, 0x04,		/* 4 */
/*  844 */
	M_JMPB, 0x4D,		/* 767 */
/*  846 */
	ZLD, M_R, M_AL,
	ZNEG, M_AR,
	ZSDA,
	M_JMP1, 0x04,		/* 4 */
/*  854 */
	ZLD, M_R, M_AL,
	ZNEG, M_AR,
	M_JMPB, 0x53,		/* 776 */
/*  861 */
	ZLDL, M_R, M_AL,
	ZSLAL,
	M_JMP1, 0x1F,		/* 31 */
/*  867 */
	M_JMPB, 0x58,		/* 779 */
/*  869 */
	ZLDL, M_R, M_AL,
	ZNEG, M_AR,
	ZSDAL,
	M_JMP1, 0x1F,		/* 31 */
/*  877 */
	ZLDL, M_R, M_AL,
	ZNEG, M_AR,
	M_JMPB, 0x5E,		/* 788 */
/*  884 */
	ZLDB, M_LO, M_R, M_AL,
	ZEXTSB, M_R,
	ZSLA, M_R, M_AR,
	ZLDB, M_AL, M_LO, M_R,
	M_END,
/*  898 */
	ZLDB, M_LO, M_R, M_AL,
	ZCLRB, M_HI, M_R,
	ZSLL,
	M_JMPB, 0x0F,		/* 891 */
/*  908 */
	ZLDB, M_LO, M_R, M_AL,
	ZEXTSB, M_R,
	ZNEG, M_AR,
	ZSDA,
	M_JMPB, 0x1A,		/* 891 */
/*  919 */
	ZLDB, M_LO, M_R, M_AL,
	ZCLRB, M_HI, M_R,
	ZNEG, M_AR,
	ZSDL,
	M_JMPB, 0x26,		/* 891 */
/* assign.t */
/*  931 */
	ZCLR, M_AL,
	M_END,
/*  934 */
	ZCLRB, M_AL,
	M_END,
/*  937 */
	ZCLR, M_HI, M_AL,
	ZCLR, M_LO, M_AL,
	M_END,
/*  944 */
	ZLD, M_AL, M_AR,
	M_END,
/*  948 */
	ZLDB, M_AL, M_AR,
	M_END,
/*  952 */
	M_JMPB, 0x12,		/* 934 */
/*  954 */
	M_JMPB, 0x06,		/* 948 */
/*  956 */
	ZLDL, M_AL, M_AR,
	M_END,
/*  960 */
	M_JMPB, 0x04,		/* 956 */
/*  962 */
	ZLD, M_LO, M_AL, M_LO, M_AR,
	ZLD, M_HI, M_AL, M_HI, M_AR,
	M_END,
/*  973 */
	ZLDL, M_HI, M_AL, M_REGNO, RR0,
	ZLDL, M_LO,
	M_JMP2, 0x40, 0x01,	/* 320 */
/*  983 */
	M_JMP1, 0x3D,		/* 61 */
/*  985 */
	M_JMP1, 0x55,		/* 85 */
/*  987 */
	M_JMP1, 0x3D,		/* 61 */
/*  989 */
	M_JMP1, 0x55,		/* 85 */
/*  991 */
	ZLD, M_AL, M_R,
	M_IFR, ZOR, M_R,
	M_JMP1, 0x4A,		/* 74 */
/*  999 */
	ZLDB, M_AL, M_LO, M_R,
	M_JMP1, 0x45,		/* 69 */
/* 1005 */
	M_JMP1, 0x21,		/* 33 */
/* 1007 */
	ZLD, M_R, M_AL,
	ZAND, M_R, M_LO, M_CMASK,
	M_JMP2, 0x59, 0x02,	/* 601 */
/* 1017 */
	ZLDB, M_LO, M_R, M_AL,
	ZANDB, M_LO, M_R, M_LO, M_CMASK,
	M_JMP2, 0x60, 0x02,	/* 608 */
/* asub.t */
/* 1029 */
	ZLD, M_R, M_AL,
	ZSUB,
	M_JMP1, 0x04,		/* 4 */
/* 1035 */
	ZLDB, M_LO, M_R, M_AL,
	ZSUBB,
	M_JMP1, 0x13,		/* 19 */
/* 1042 */
	ZLDL, M_R, M_AL,
	ZSUBL,
	M_JMP1, 0x1F,		/* 31 */
/* 1048 */
	ZSUB,
	M_JMP1, 0x26,		/* 38 */
/* 1051 */
	ZLDL, M_R, M_AL,
	ZSUB,
	M_JMP1, 0x33,		/* 51 */
/* 1057 */
	ZLDB, M_LO, M_R, M_AL,
	ZSUB,
	M_JMP1, 0x3B,		/* 59 */
/* 1064 */
	ZLDB, M_LO, M_R, M_AL,
	ZSUB,
	M_JMP1, 0x53,		/* 83 */
/* 1071 */
	ZLD, M_R, M_AL,
	ZSUB,
	M_JMP1, 0x62,		/* 98 */
/* 1077 */
	ZLDB, M_LO, M_R, M_AL,
	ZSUBB,
	M_JMP1, 0x7D,		/* 125 */
/* axor.t */
/* 1084 */
	ZLD, M_R, M_AL,
	ZXOR,
	M_JMP1, 0x04,		/* 4 */
/* 1090 */
	ZLDB, M_LO, M_R, M_AL,
	ZXORB,
	M_JMP1, 0x13,		/* 19 */
/* 1097 */
	ZLDL, M_R, M_AL,
	ZXOR, M_LO, M_R, M_LO, M_AR,
	ZXOR,
	M_JMP1, 0xAD,		/* 173 */
/* 1108 */
	ZLDB, M_LO, M_R, M_AL,
	ZXOR,
	M_JMP1, 0x3B,		/* 59 */
/* 1115 */
	ZLDB, M_LO, M_R, M_AL,
	ZXOR,
	M_JMP1, 0x53,		/* 83 */
/* 1122 */
	ZLD, M_R, M_AL,
	ZXOR,
	M_JMP1, 0xC4,		/* 196 */
/* 1128 */
	ZLDB, M_LO, M_R, M_AL,
	ZXORB,
	M_JMP1, 0xCD,		/* 205 */
/* bef.t */
/* 1135 */
	M_OP1, M_AL, M_ICON, 0x01,
	M_IFV, ZLD, M_R, M_AL,
	M_JMP1, 0x4B,		/* 75 */
/* 1145 */
	M_TL, M_OP1, M_AL, M_ICON, 0x01,
	M_IFV, ZLDB, M_LO, M_R, M_AL, M_ENDIF,
	M_JMP2, 0x22, 0x03,	/* 802 */
/* 1159 */
	M_TL, M_OP1, M_AL, M_ICON, 0x01,
	M_IFV, ZLDB, M_LO, M_R, M_AL, M_ENDIF,
	M_JMP2, 0x32, 0x03,	/* 818 */
/* 1173 */
	M_OP1, M_LO, M_AL, M_AR,
	ZLDL, M_R, M_AL,
	M_END,
/* 1181 */
	M_OP1,
	M_JMP1, 0x26,		/* 38 */
/* 1184 */
	ZLDL, M_R, M_AL,
	M_OP1,
	M_JMP1, 0x33,		/* 51 */
/* blkmv.t */
/* 1190 */
	ZLD, M_R, M_SIZE,
	ZLDIRB, M_RL, M_RR, M_R,
	M_END,
/* div.t */
/* 1198 */
	ZCLR, M_REGNO, R0,
	ZDIV, M_REGNO, RR0, M_AR,
	M_END,
/* 1206 */
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZCLR, M_REGNO, R2,
	ZDIVL, M_REGNO, RQ0, M_AR,
	M_END,
/* 1219 */
	ZEXTS, M_REGNO, RR0,
	M_JMPB, 0x15,		/* 1201 */
/* 1224 */
	ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	M_JMPB, 0x0F,		/* 1214 */
/* 1231 */
	ZTESTL, M_AR,
	ZJRPL, M_LAB0,
	ZLDL, M_REGNO, RR0, M_REGNO, RR2,
	ZSUBL, M_REGNO, RR2, M_REGNO, RR2,
	ZCPL, M_REGNO, RR0, M_AR,
	ZJRULT, M_LAB1,
	ZSUBL, M_REGNO, RR0, M_AR,
	ZLD, M_REGNO, R3, M_ICON, 0x01,
	ZJP, M_LAB1,
	M_DLAB0, ZSUBL, M_REGNO, RR0, M_REGNO, RR0,
	ZDIVL, M_REGNO, RQ0, M_AR,
	M_DLAB1,
	M_END,
/* 1274 */
	ZEXTSL,
	M_JMPB, 0x30,		/* 1227 */
/* leaves.t */
/* 1277 */
	ZTEST, M_AL,
	M_REL0, M_LAB,
	M_END,
/* 1282 */
	ZTESTB,
	M_JMPB, 0x05,		/* 1278 */
/* 1285 */
	ZCP, M_AL, M_ICON, 0x03,
	M_JMPB, 0x0A,		/* 1279 */
/* 1291 */
	ZTESTL,
	M_JMPB, 0x0E,		/* 1278 */
/* 1294 */
	ZTESTL, M_R,
	M_JMPB, 0x11,		/* 1279 */
/* 1298 */
	ZTESTL, M_AL,
	ZRESFLG, M_ICON, 0x04,
	M_JMPB, 0x18,		/* 1279 */
/* 1305 */
	ZTESTL, M_R,
	M_JMPB, 0x07,		/* 1300 */
/* 1309 */
	ZCLR, M_R,
	M_END,
/* 1312 */
	ZSUBL, M_R, M_R,
	M_END,
/* 1316 */
	M_END,
/* 1317 */
	M_END,
/* 1318 */
	ZLD,
	M_JMPB, 0x8D,		/* 1178 */
/* 1321 */
	ZLDB, M_LO,
	M_JMPB, 0x91,		/* 1178 */
/* 1325 */
	M_END,
/* 1326 */
	ZLDA, M_LO, M_R, M_NSE, M_AL,
	M_JMP1, 0xF3,		/* 243 */
/* 1333 */
	M_JMPB, 0x9C,		/* 1177 */
/* 1335 */
	ZLD, M_LO, M_R, M_AL,
	ZCLR, M_HI, M_R,
	M_END,
/* 1343 */
	M_JMPB, 0x08,		/* 1335 */
/* 1345 */
	ZLDA, M_LO, M_R, M_NSE, M_AL,
	ZLD, M_HI, M_R, M_REGNO, R14,
	ZPUSHL, M_R,
	M_END,
/* 1358 */
	ZLDL, M_R, M_AL,
	M_JMPB, 0x06,		/* 1355 */
/* 1363 */
	M_JMPB, 0x05,		/* 1358 */
/* 1365 */
	ZPUSH, M_AL,
	M_END,
/* 1368 */
	ZLD, M_R, M_AL,
	ZPUSH, M_R,
	M_END,
/* 1374 */
	ZLDB, M_LO,
	M_JMPB, 0x07,		/* 1369 */
/* 1378 */
	ZLD, M_HI, M_R, M_HI, M_AL,
	ZLD, M_LO, M_R, M_LO, M_AL,
	M_END,
/* 1389 */
	M_JMPB, 0xD4,		/* 1177 */
/* 1391 */
	ZLDL, M_REGNO, RR0, M_HI, M_AL,
	ZLDL, M_REGNO, RR2, M_LO, M_AL,
	ZPUSHL, M_REGNO, RR2,
	ZPUSHL, M_REGNO, RR0,
	M_END,
/* 1408 */
	ZLDL, M_REGNO, RR0, M_HI, M_AL,
	ZLDL, M_REGNO, RR2, M_LO, M_AL,
	M_END,
/* 1419 */
	ZLDL, M_REGNO, RR0, M_AL,
	ZCALL, M_GID, 0x01,
	M_END,
/* 1427 */
	ZCALL, M_GID, 0x02,
	M_END,
/* 1431 */
	ZLDL, M_REGNO, RR0, M_HI, M_AL,
	ZLDL, M_REGNO, RR2, M_LO, M_AL,
	M_JMPB, 0x0E,		/* 1427 */
/* 1443 */
	ZLDB, M_LO, M_R, M_AL,
	ZEXTSB, M_R,
	M_END,
/* 1450 */
	ZLDB, M_LO, M_R, M_AL,
	ZCLRB,
	M_JMPB, 0x73,		/* 1340 */
/* 1457 */
	M_JMPB, 0x0A,		/* 1447 */
/* 1459 */
	M_JMPB, 0x05,		/* 1454 */
/* 1461 */
	ZLD, M_LO, M_R, M_AL,
	ZEXTS, M_R,
	M_END,
/* 1468 */
	M_JMPB, 0x85,		/* 1335 */
/* 1470 */
	ZLDB, M_LO, M_LO, M_R, M_AL,
	ZEXTSB, M_LO, M_R,
	M_JMPB, 0x0D,		/* 1465 */
/* 1480 */
	ZLDB, M_LO, M_LO, M_R, M_AL,
	ZCLRB, M_HI, M_LO, M_R,
	M_JMPB, 0x96,		/* 1339 */
/* 1491 */
	ZLD,
	M_JMPB, 0x6B,		/* 1385 */
/* 1494 */
	ZLD,
	M_JMPB, 0x32,		/* 1445 */
/* 1497 */
	ZLD,
	M_JMPB, 0x2E,		/* 1452 */
/* 1500 */
	ZLDB, M_LO, M_R, M_LO,
	M_JMPB, 0x3A,		/* 1446 */
/* 1506 */
	ZLDB, M_LO, M_R, M_LO,
	M_JMPB, 0x39,		/* 1453 */
/* 1512 */
	ZLDB, M_LO, M_R, M_LO,
	M_JMPB, 0x0D,		/* 1503 */
/* 1518 */
	M_JMPB, 0x47,		/* 1447 */
/* 1520 */
	M_JMPB, 0x42,		/* 1454 */
/* 1522 */
	M_END,
/* 1523 */
	M_END,
/* 1524 */
	M_JMP2, 0x99, 0x04,	/* 1177 */
/* mul.t */
/* 1527 */
	ZMULT,
	M_JMP2, 0xB2, 0x04,	/* 1202 */
/* 1531 */
	ZMULTL,
	M_JMP2, 0xBF, 0x04,	/* 1215 */
/* neg.t */
/* 1535 */
	ZNEG,
	M_JMP1, 0x08,		/* 8 */
/* 1538 */
	ZNEGB,
	M_JMP1, 0x08,		/* 8 */
/* 1541 */
	ZCOM, M_HI, M_R,
	ZCOM, M_LO, M_R,
	ZADDL, M_R, M_LCON, 0x01,
	M_IFR, ZTESTL,
	M_JMP1, 0x4A,		/* 74 */
/* not.t */
/* 1555 */
	ZCOM,
	M_JMP1, 0x08,		/* 8 */
/* 1558 */
	ZCOMB,
	M_JMP1, 0x08,		/* 8 */
/* 1561 */
	ZCOM, M_HI, M_R,
	ZCOM, M_LO, M_R,
	M_JMPB, 0x10,		/* 1551 */
/* or.t */
/* 1569 */
	ZOR,
	M_JMP1, 0xE0,		/* 224 */
/* 1572 */
	ZORB,
	M_JMP1, 0xE0,		/* 224 */
/* 1575 */
	ZOR, M_HI, M_R, M_HI, M_AR,
	ZOR,
	M_JMP2, 0x27, 0x02,	/* 551 */
/* 1584 */
	M_JMPB, 0x04,		/* 1580 */
/* 1586 */
	M_JMPB, 0x0B,		/* 1575 */
/* relop.t */
/* 1588 */
	ZCP, M_AL, M_AR,
	M_REL0, M_LAB,
	M_END,
/* 1594 */
	ZCP, M_R,
	M_JMPB, 0x06,		/* 1590 */
/* 1598 */
	ZCPB, M_LO,
	M_JMPB, 0x0B,		/* 1589 */
/* 1602 */
	ZCPB, M_LO, M_R, M_LO,
	M_JMPB, 0x10,		/* 1590 */
/* 1608 */
	M_JMP2, 0x0B, 0x05,	/* 1291 */
/* 1611 */
	M_JMP2, 0x0B, 0x05,	/* 1291 */
/* 1614 */
	M_JMP2, 0x12, 0x05,	/* 1298 */
/* 1617 */
	ZCPL,
	M_JMPB, 0x1D,		/* 1589 */
/* 1620 */
	M_JMP2, 0x0B, 0x05,	/* 1291 */
/* 1623 */
	M_JMP2, 0x0B, 0x05,	/* 1291 */
/* 1626 */
	M_JMP2, 0x12, 0x05,	/* 1298 */
/* 1629 */
	M_JMPB, 0x0C,		/* 1617 */
/* rem.t */
/* 1631 */
	ZLD, M_REGNO, R1, M_AL,
	M_JMP2, 0xAE, 0x04,	/* 1198 */
/* 1638 */
	ZLD, M_REGNO, R3, M_AL,
	M_JMP2, 0xB6, 0x04,	/* 1206 */
/* 1645 */
	ZLD, M_REGNO, R1, M_AL,
	M_JMP2, 0xC3, 0x04,	/* 1219 */
/* 1652 */
	ZLDL, M_REGNO, RR2, M_AL,
	M_JMP2, 0xC8, 0x04,	/* 1224 */
/* 1659 */
	ZLDL, M_REGNO, RR2, M_AL,
	ZTESTL, M_AR,
	ZJRPL, M_LAB0,
	ZLDL, M_REGNO, RR0, M_REGNO, RR2,
	ZCPL, M_REGNO, RR0, M_AR,
	ZJRULT, M_LAB1,
	ZSUBL, M_REGNO, RR0, M_AR,
	M_JMP2, 0xEC, 0x04,	/* 1260 */
/* 1685 */
	ZLDL, M_REGNO, RR2, M_AL,
	M_JMP2, 0xFA, 0x04,	/* 1274 */
/* shl.t */
/* 1692 */
	ZSLL,
	M_JMP1, 0xE0,		/* 224 */
/* 1695 */
	ZSDL,
	M_JMP1, 0xE0,		/* 224 */
/* 1698 */
	ZSLLL,
	M_JMP1, 0xE0,		/* 224 */
/* 1701 */
	ZSDLL,
	M_JMP1, 0xE0,		/* 224 */
/* 1704 */
	ZLD, M_LO, M_R, M_AL,
	ZSLL, M_LO, M_R, M_AR,
	ZCLR, M_HI,
	M_JMP1, 0x08,		/* 8 */
/* 1716 */
	ZLD, M_LO, M_R, M_AL,
	ZSDL,
	M_JMPB, 0x0C,		/* 1709 */
/* shr.t */
/* 1723 */
	ZSLA,
	M_JMP1, 0xE0,		/* 224 */
/* 1726 */
	M_JMPB, 0x22,		/* 1692 */
/* 1728 */
	ZNEG, M_AR,
	ZSDA,
	M_JMP1, 0xE0,		/* 224 */
/* 1733 */
	ZNEG, M_AR,
	M_JMPB, 0x28,		/* 1695 */
/* 1737 */
	ZSLAL,
	M_JMP1, 0xE0,		/* 224 */
/* 1740 */
	M_JMPB, 0x2A,		/* 1698 */
/* 1742 */
	ZNEG, M_AR,
	ZSDAL,
	M_JMP1, 0xE0,		/* 224 */
/* 1747 */
	ZNEG, M_AR,
	M_JMPB, 0x30,		/* 1701 */
/* sub.t */
/* 1751 */
	ZDEC,
	M_JMP1, 0xD4,		/* 212 */
/* 1754 */
	ZSUB,
	M_JMP1, 0xE0,		/* 224 */
/* 1757 */
	ZSUBB,
	M_JMP1, 0xE0,		/* 224 */
/* 1760 */
	ZSUBL,
	M_JMP1, 0xE0,		/* 224 */
/* 1763 */
	ZSUB,
	M_JMP1, 0xEB,		/* 235 */
/* xor.t */
/* 1766 */
	ZXOR,
	M_JMP1, 0xE0,		/* 224 */
/* 1769 */
	ZXORB,
	M_JMP1, 0xE0,		/* 224 */
/* 1772 */
	ZXOR, M_HI, M_R, M_HI, M_AR,
	ZXOR,
	M_JMP2, 0x27, 0x02,	/* 551 */
 0
};
