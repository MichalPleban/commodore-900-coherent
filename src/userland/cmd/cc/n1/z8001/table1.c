/*
 * n1/z8001/table1.c
 * Machine-specific tables used by the cc1 code generator. Segmented Z8001.
 * Template: n1/i386/table1.c. Values from src/h/mch.h (register model + type
 * codes + ABI), generated/opcode.h (Z* mnemonics), and cc1mch.h (kinds, cc).
 */

#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * Width table, indexed by machine type. Values give the relative width order
 * for iswiden() (NOT bytes). Z8001: byte<word<long; LPTR (segmented ptr) is
 * 32-bit so it orders with long; SPTR (near ptr) with word.
 */
char	wtype[] = {
	0,	0,		/* S8	U8	*/
	1,	1,		/* S16	U16	*/
	2,	2,		/* S32	U32	*/
	3,	4,		/* F32	F64	*/
	5,			/* BLK		*/
	0,	1,		/* FLD8	FLD16	*/
	2,	2,		/* LPTR	LPTB	*/
	1,	1		/* SPTR	SPTB	*/
};

/*
 * Register table, indexed by 'real' register number (mch.h: R0..R15=0..15,
 * RR0..RR14=16..23, RQ0..RQ12=24..27, RH0..RH7=28..35, RL0..RL7=36..43, then
 * the pseudo registers). Word regs R0..R7 have byte halves RHn/RLn; R8..R15
 * do not. R13=FP, R14:R15=RR14=SP are reserved (NOFREE). Pairs are big-endian
 * (hi = even word, lo = odd word). Fields:
 *   r_lvalue r_rvalue r_goal r_enpair r_hihalf r_lohalf r_phys
 */
REGDESC	reg[] = {
/* R0  */ { KWB|KSP, KWB|KSP, MRVALUE, RR0,  RH0, RL0, BREG(R0) },
/* R1  */ { KWB|KSP, KWB|KSP, MRVALUE, RR0,  RH1, RL1, BREG(R1) },
/* R2  */ { KWB|KSP, KWB|KSP, MRVALUE, RR2,  RH2, RL2, BREG(R2) },
/* R3  */ { KWB|KSP, KWB|KSP, MRVALUE, RR2,  RH3, RL3, BREG(R3) },
/* R4  */ { KWB|KSP, KWB|KSP, MRVALUE, RR4,  RH4, RL4, BREG(R4) },
/* R5  */ { KWB|KSP, KWB|KSP, MRVALUE, RR4,  RH5, RL5, BREG(R5) },
/* R6  */ { KWB|KSP, KWB|KSP, MRVALUE, RR6,  RH6, RL6, BREG(R6) },
/* R7  */ { KWB|KSP, KWB|KSP, MRVALUE, RR6,  RH7, RL7, BREG(R7) },
/* R8  */ { KW|KSP,  KW|KSP,  MRVALUE, RR8,  -1,  -1,  BREG(R8) },
/* R9  */ { KW|KSP,  KW|KSP,  MRVALUE, RR8,  -1,  -1,  BREG(R9) },
/* R10 */ { KW|KSP,  KW|KSP,  MRVALUE, RR10, -1,  -1,  BREG(R10) },
/* R11 */ { KW|KSP,  KW|KSP,  MRVALUE, RR10, -1,  -1,  BREG(R11) },
/* R12 */ { KW|KSP,  KW|KSP,  MRVALUE, RR12, -1,  -1,  BREG(R12) },
/* R13 */ { 0,    0,    -1,      RR12, -1,  -1,  BREG(R13) },	/* FP */
/* R14 */ { 0,    0,    -1,      RR14, -1,  -1,  BREG(R14) },	/* SP seg */
/* R15 */ { 0,    0,    -1,      RR14, -1,  -1,  BREG(R15) },	/* SP off */

/* RR0  */ { 0,      KL|KLP, MRVALUE, RQ0,  R0,  R1,  BREG(R0)|BREG(R1) },	/* r_lvalue EMPTY: RR0 may not be an indirect, base or index register (Z8000 CPU Tech Man 5.2, p.5-2) -- @R0 aliases LD #imm -- and that is a property of the ROLE, so no kind qualifies, KL included: a long cast to a far pointer addresses exactly as a far pointer does.  r_rvalue keeps KL|KLP: HOLDING a long or a far pointer in RR0 (the long/pointer return pair) is legal. */
/* RR2  */ { KL|KLP, KL|KLP, MRVALUE, RQ0,  R2,  R3,  BREG(R2)|BREG(R3) },
/* RR4  */ { KL|KLP, KL|KLP, MRVALUE, RQ4,  R4,  R5,  BREG(R4)|BREG(R5) },
/* RR6  */ { KL|KLP, KL|KLP, MRVALUE, RQ4,  R6,  R7,  BREG(R6)|BREG(R7) },
/* RR8  */ { KL|KLP, KL|KLP, MRVALUE, RQ8,  R8,  R9,  BREG(R8)|BREG(R9) },
/* RR10 */ { KL|KLP, KL|KLP, MRVALUE, RQ8,  R10, R11, BREG(R10)|BREG(R11) },
/* RR12 */ { KL|KLP, KL|KLP, MRVALUE, RQ12, R12, R13, BREG(R12)|BREG(R13) },
/* RR14 */ { 0, 0,  -1,      -1,   R14, R15, BREG(R14)|BREG(R15) },	/* SP */

/* RQ0  */ { 0, KD, MRVALUE, -1, RR0,  RR2,  BREG(R0)|BREG(R1)|BREG(R2)|BREG(R3) },
/* RQ4  */ { 0, KD, MRVALUE, -1, RR4,  RR6,  BREG(R4)|BREG(R5)|BREG(R6)|BREG(R7) },
/* RQ8  */ { 0, KD, MRVALUE, -1, RR8,  RR10, BREG(R8)|BREG(R9)|BREG(R10)|BREG(R11) },
/* RQ12 */ { 0, 0,  -1,      -1, RR12, RR14, BREG(R12)|BREG(R13)|BREG(R14)|BREG(R15) },

/* RH0 */ { 0, 0, -1, R0, -1, -1, BREG(R0) },
/* RH1 */ { 0, 0, -1, R1, -1, -1, BREG(R1) },
/* RH2 */ { 0, 0, -1, R2, -1, -1, BREG(R2) },
/* RH3 */ { 0, 0, -1, R3, -1, -1, BREG(R3) },
/* RH4 */ { 0, 0, -1, R4, -1, -1, BREG(R4) },
/* RH5 */ { 0, 0, -1, R5, -1, -1, BREG(R5) },
/* RH6 */ { 0, 0, -1, R6, -1, -1, BREG(R6) },
/* RH7 */ { 0, 0, -1, R7, -1, -1, BREG(R7) },
/* RL0 */ { 0, 0, -1, R0, -1, -1, BREG(R0) },
/* RL1 */ { 0, 0, -1, R1, -1, -1, BREG(R1) },
/* RL2 */ { 0, 0, -1, R2, -1, -1, BREG(R2) },
/* RL3 */ { 0, 0, -1, R3, -1, -1, BREG(R3) },
/* RL4 */ { 0, 0, -1, R4, -1, -1, BREG(R4) },
/* RL5 */ { 0, 0, -1, R5, -1, -1, BREG(R5) },
/* RL6 */ { 0, 0, -1, R6, -1, -1, BREG(R6) },
/* RL7 */ { 0, 0, -1, R7, -1, -1, BREG(R7) },

/* NONE   */ { 0, 0, -1, -1, -1, -1, 0 },
/* ANYR   */ { 0, 0, -1, -1, -1, -1, 0 },
/* ANYL   */ { 0, 0, -1, -1, -1, -1, 0 },
/* PAIR   */ { 0, 0, -1, -1, -1, -1, 0 },
/* TEMP   */ { 0, 0, -1, -1, -1, -1, 0 },
/* LOTEMP */ { 0, 0, -1, -1, -1, -1, 0 },
/* HITEMP */ { 0, 0, -1, -1, -1, -1, 0 }
};

/*
 * The opcode table, indexed by operation (op - ADD). Three columns:
 *   col 0 = primary opcode; col 1 = special/short form (INC/DEC/TEST/move);
 *   col 2 = carry/extend form (ADC/SBC) or, for relations, a cc variant.
 * RELATION ROWS hold Z8000 CONDITION CODES (cc1mch.h CC_*), not opcodes: the
 * Z8000 has ONE conditional jump (ZJR/ZJP) + a 4-bit cc.
 * Right shifts reuse SLA/SLL with a negative count (verified decoder behavior),
 * so SHR/ASHR share the left-shift opcodes; the count sign is set at emit.
 */
unsigned char	optab[][3] = {
	{ ZADD,  ZINC,  ZADC  },		/* ADD */
	{ ZSUB,  ZDEC,  ZSBC  },		/* SUB */
	{ ZMULT, 0,     0     },		/* MUL */
	{ ZDIV,  0,     0     },		/* DIV */
	{ ZDIV,  0,     0     },		/* REM */
	{ ZAND,  ZTEST, ZSUB  },		/* AND */
	{ ZOR,   0,     0     },		/* OR  */
	{ ZXOR,  ZCOM,  0     },		/* XOR */
	{ ZSLA,  ZSLL,  ZRLC  },		/* SHL */
	{ ZSLA,  ZSLL,  ZRRC  },		/* SHR */

	{ ZADD,  ZINC,  ZADC  },		/* AADD */
	{ ZSUB,  ZDEC,  ZSBC  },		/* ASUB */
	{ ZMULT, ZLD,   ZLDB  },		/* AMUL */
	{ ZDIV,  ZLD,   ZLDB  },		/* ADIV */
	{ ZDIV,  ZLD,   ZLDB  },		/* AREM */
	{ ZAND,  ZSUB,  0     },		/* AAND */
	{ ZOR,   0,     0     },		/* AOR  */
	{ ZXOR,  ZCOM,  0     },		/* AXOR */
	{ ZSLA,  ZSLL,  ZRLC  },		/* ASHL */
	{ ZSLA,  ZSLL,  ZRRC  },		/* ASHR */

	/* relation rows: per-condition relative-jump opcodes ZJREL|cc (cols REL0/
	 * REL1 normal+swapped sense; col 2 the long-compare cc). n2 maps to JR cc. */
	{ ZJREL|CC_EQ,  ZJREL|CC_EQ,  0           },	/* EQ  */
	{ ZJREL|CC_NE,  ZJREL|CC_NE,  0           },	/* NE  */
	{ ZJREL|CC_GT,  ZJREL|CC_LT,  ZJREL|CC_GT },	/* GT  */
	{ ZJREL|CC_GE,  ZJREL|CC_LE,  ZJREL|CC_GE },	/* GE  */
	{ ZJREL|CC_LE,  ZJREL|CC_GE,  ZJREL|CC_LE },	/* LE  */
	{ ZJREL|CC_LT,  ZJREL|CC_GT,  ZJREL|CC_LT },	/* LT  */
	{ ZJREL|CC_UGT, ZJREL|CC_ULT, ZJREL|CC_UGT },	/* UGT */
	{ ZJREL|CC_UGE, ZJREL|CC_ULE, ZJREL|CC_UGE },	/* UGE */
	{ ZJREL|CC_ULE, ZJREL|CC_UGE, ZJREL|CC_ULE },	/* ULE */
	{ ZJREL|CC_ULT, ZJREL|CC_UGT, ZJREL|CC_ULT },	/* ULT */

	{ 0,     0,    0     },			/* STAR    */
	{ 0,     0,    0     },			/* ADDR    */
	{ ZNEG,  ZSBC, 0     },			/* NEG     */
	{ ZCOM,  0,    0     },			/* COM     */
	{ 0,     0,    0     },			/* NOT     */
	{ 0,     0,    0     },			/* QUEST   */
	{ 0,     0,    0     },			/* COLON   */
	{ ZADDL, ZINC, ZSUBL },			/* INCBEF  */
	{ ZSUBL, ZDEC, ZADDL },			/* DECBEF  */
	{ ZADDL, ZINC, ZSUBL },			/* INCAFT  */
	{ ZSUBL, ZDEC, ZADDL },			/* DECAFT  */
	{ 0,     0,    0     },			/* COMMA   */
	{ 0,     0,    0     },			/* CALL    */
	{ 0,     0,    0     },			/* ANDAND  */
	{ 0,     0,    0     },			/* OROR    */
	{ 0,     0,    0     },			/* CAST    */
	{ 0,     0,    0     },			/* CONVERT */
	{ 0,     0,    0     },			/* FIELD   */
	{ 0,     0,    0     },			/* SIZEOF  */
	{ ZLD,   ZSUB, 0     },			/* ASSIGN  */
	{ 0,     0,    0     },			/* NOP     */
	{ 0,     0,    0     },			/* INIT    */
	{ 0,     0,    0     },			/* ARGLST  */
	{ 0,     0,    0     },			/* LEAF    */
	{ 0,     0,    0     },			/* FIXUP   */
	{ 0,     0,    0     }			/* BLKMOVE */
};

/*
 * Per-type info, indexed by machine type (mch.h S8..SPTB).
 *   p_frreg : function return register (-1 = never a return type)
 *             ABI: int->R1, long/LPTR->RR0, double->RQ0, near ptr->R1
 *   p_frcxt : return context (select)
 *   p_size  : size of a temp / stack arg slot (word-granular)
 *   p_incr  : real memory size (autoinc/dec)
 *   p_type  : the match type-flag bit (mch.h F*)
 *   p_kind  : register kind; p_pair: kind if paired
 */
PERTYPE	pertype[] = {
/*		p_frreg p_frcxt   p_size p_incr p_type  p_kind p_pair */
/* S8    */ { R1,   MRVALUE, 2, 1, FS8,   KB, KL },
/* U8    */ { R1,   MRVALUE, 2, 1, FU8,   KB, KL },
/* S16   */ { R1,   MRVALUE, 2, 2, FS16,  KW, KL },
/* U16   */ { R1,   MRVALUE, 2, 2, FU16,  KW, KL },
/* S32   */ { RR0,  MRVALUE, 4, 4, FS32,  KL, 0  },
/* U32   */ { RR0,  MRVALUE, 4, 4, FU32,  KL, 0  },
/* F32   */ { RR0,  MRVALUE, 4, 4, FF32,  KL, 0  },
/* F64   */ { RQ0,  MRVALUE, 8, 8, FF64,  KD, 0  },
/* BLK   */ { -1,   MRVALUE, 0, 0, FBLK,  0,  0  },
/* FLD8  */ { -1,   MRVALUE, 0, 0, FFLD8, 0,  0  },
/* FLD16 */ { -1,   MRVALUE, 0, 0, FFLD16,0,  0  },
/* LPTR  */ { RR0,  MRVALUE, 4, 4, FLPTR, KLP, KLP },
/* LPTB  */ { RR0,  MRVALUE, 4, 4, FLPTB, KLP, KLP },
/* SPTR  */ { R1,   MRVALUE, 2, 2, FSPTR, KSP, KL },
/* SPTB  */ { R1,   MRVALUE, 2, 2, FSPTB, KSP, KL }
};

/*
 * Relation adjustment tables (machine-INDEPENDENT; verbatim from the template).
 * Indexed by (rel - EQ). fliprel: sense reversed; otherel: subtrees swapped.
 */
char	fliprel[] = {
	EQ, NE, LT, LE, GE, GT, ULT, ULE, UGE, UGT
};

char	otherel[] = {
	NE, EQ, LE, LT, GT, GE, ULE, ULT, UGT, UGE
};

/*
 * Type table, indexed by machine type. Used by the cc1mch.h type-test macros.
 * Bits: 01 long  02 word  04 unsigned  010 float  020 int(eger)  040 byte
 *       0100 sized  0200 pointer.  (Pointers are unsigned; LPTR=long, SPTR=word.)
 */
char	tinfo[] = {
	0060,	0064,		/* S8	U8	*/
	0022,	0026,		/* S16	U16	*/
	0021,	0025,		/* S32	U32	*/
	0010,	0010,		/* F32	F64	*/
	0100,			/* BLK		*/
	0060,	0022,		/* FLD8	FLD16	*/
	0205,	0305,		/* LPTR	LPTB	*/
	0206,	0306		/* SPTR	SPTB	*/
};

/* end of n1/z8001/table1.c */
