/*
 * Machine header.
 * Zilog Z-8000.
 * Handles both the Z-8001
 * and Z-8002.
 * The SEGCPU define determines
 * the default type of assembler.
 * However, the assembler can always
 * put out both types.
 */
#ifndef	MTYPE_H
#include <mtype.h>
#endif

#define	SEGCPU	1
#define	LADDR	1		/* long addressing form of l.out */
#ifndef SEGCPU
#define	SEGCPU	0
#endif

#define	HEX	1
#define	LOHI	0
#define	CLIST	WLIST
#define	MINIT	1
#define	JRSZ	2			/* Length of a jr instruction */
#define	CALRSZ	2			/* Length of a calr instruction */

#if	SEGCPU
#define	CPU	"Zilog Z-8001"
#define	M_MACHINE	M_Z8001
#else
#define	CPU	"Zilog Z-8002"
#define M_MACHINE	M_Z8002
#endif

/* For list.c */
#define	ADRFMT	"   %04X"
#define	NBOL	8
#define	BFMT	" %02x"
#define	WFMT	"  %04x"
#define	SKIP	"   "

#define	fbyte(x)	((((int) x)>>8)&0377)
#define	sbyte(x)	(((int) x)&0377)
#define	fword(x)	((unsigned)((x)>>16))
#define	sword(x)	((unsigned)(x))
#define locrup(x)       (((address)(x)+01)&~01)

typedef long	address;
typedef unsigned offset;

/* More kinds */
#define	S_BIT	50
#define	S_CALL	51
#define	S_CALR	52
#define	S_CC	53
#define	S_CLR	54
#define	S_CP	55
#define	S_CPD	56
#define	S_DEC	57
#define	S_DI	58
#define	S_DJNZ	59
#define	S_EX	60
#define	S_FLG	61
#define	S_FLGN	62
#define	S_HALT	63
#define	S_IN	64
#define	S_IND	65
#define	S_INDR	66
#define	S_IVN	67
#define	S_JP	68
#define	S_JR	69
#define	S_LD	71
#define	S_LDA	72
#define	S_LDAR	73
#define	S_CTL	74
#define	S_LDM	76
#define	S_LDPS	77
#define	S_LDR	78
#define	S_POP	79
#define	S_PUSH	80
#define	S_R	81
#define	S_REG	82
#define	S_RET	83
#define	S_RL	84
#define	S_RR	85
#define	S_RSRC	86
#define	S_SC	87
#define	S_SDA	88
#define	S_SIN	89
#define	S_SLA	90
#define	S_SOUT	91
#define	S_TCC	92
#define	S_SRA	93
#define	S_TRTR	94
#define	S_LONG	95
#define	S_TRT	96
#define	S_EVEN	97
#define	S_ODD	98
#define	S_LDK	99
#define	S_CPS	100
#define	S_OUT	101
#define	S_SEGM	102

#define	S_L	040		/* Long instruction */
#define	S_2	001		/* Mask for register pair */
#define	S_4	003		/* Mask for quad register */

/* Modes */
#define	R	(0<<4)
#define	IM	(1<<4)
#define	DA	(2<<4)
#define	IR	(3<<4)
#define	X	(4<<4)
#define	BA	(5<<4)
#define	BX	(6<<4)

/*
 * Legal flags
 * Reserve 03 for S_2 and S_4
 */
#define	DAOK	04
#define	IROK	010
#define	XOK	020
#define	BAOK	040
#define	BXOK	0100
#define	UP4	0200
#define	LONG	0400
#define	W	0400
#define	ISRC	01000
#define	ROK	02000
#define	IMOK	04000
#define	IREF	010000		/* I space reference */

#define	mof(x)		((x).e_mode&~017)
#define	rof(x)		((x).e_mode&017)
#define	mofp(x)		((x)->e_mode&~017)
#define	rofp(x)		((x)->e_mode&017)
#define	mbits(a,b)	((a<<15)|(b<<14))
#define	makeop(a,b)	(((a)&W)|(b))
#define	isk(n)		((n)>=0 && (n)<=15)
#define	isaim(a)	(mof(a)==IM && (a).e_type==E_ACON)

/* Some opcodes */
#define	LDB	0x2000
#define	LDW	0x2100
#define	LDL	0x1400
#define	PUSHW	0x1300
#define	PUSHI	0x0D09
#define	UN	0x08
#define	SCHIGH	0x7F
#define	RLBY2	0x02
#define	CPL	0x1000
#define	CPI	0x0C01
#define	LDMLD	0x1C01
#define	LDMST	0x1C09
#define	LDA	0x3600
#define	LDABX	0x7400
#define	LDABA	0x3400
#define	LDCTL	0x7D00
#define	LDCTLB	0x8C00
#define	TOCTLR	0x08
#define	FLAGS	0x01
#define	INDA	0x3A04
#define	OUTDA	0x3A06
#define	STL	0x1D00
#define	ST	0x2E00
#define	STLBX	0x7700
#define	STBX	0x7200
#define	LDLBX	0x7500
#define	LDBX	0x7000
#define	LDBI	0xC000
#define	LDI	0x0C05
#define	LDK	0xBD00
#define	STBA	0x3200
#define	STLBA	0x3700
#define	LDBA	0x3000
#define	LDLBA	0x3500
#define	JP	0x1E00
#define	CALR	0xD000
#define	CALL	0x1F00
