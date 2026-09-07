/*
 * mch.h - Z8001 (segmented Z8000) machine description for Mark Williams C.
 *
 * Follows the donor's i8086/mch.h: same macro structure, Z8000 content.
 * Register/CC/addressing facts are from the verified decoder (see
 * ../../generated/).
 *
 * Shared by all four passes (cc0/cc1/cc2/cc3). The compiler-internal addressing
 * tags (A_*) and register numbers here are the compiler's model; the mapping to
 * real Z8000 opcode bits lives in n2/z8001/{optab.c,asm.c} (../generated/).
 */

/* ---- build-flavor toggles ---------------------------------------------- */
#define RUNNING_LARGE	1	/* segmented: default pointer is a 2-word (seg:off) far pointer (LPTR) */
#define SIZEOF_LARGE	1	/* large objects: sizeof_t is long */
#define ONLYSMALL	0	/* NOT a small-only build (cf. i8086 Coherent) */
#define APPENDBAR	1	/* C identifiers carry a trailing '_' in the object (Coherent Z8001 libc/crt0 convention) */
#define IEEE		1	/* the Coherent Z8001 soft-float (libc/crt dadd/dmul/...) is IEEE, so the
				 * compile-time constant folder must emit IEEE doubles too */
#define DECVAX		0
#define NATIVEFP	0	/* fold target doubles via the software reader (host-FP independent) */
#define AS_FORMAT	1	/* Coherent ".s" assembly text (n3) */
#define NDP		0	/* no separate FP coprocessor */
/* TINY=1 omits the -S codegen-debug dumps (snapf et al.). The i8086 template
 * uses TINY=0, but its snapf walks varargs with the `*((T*)p)++` cast-lvalue
 * idiom that modern gcc rejects (same class as diag.c/geno.c). Bring-up uses
 * TINY=1 (no effect on generated code, matches the working i386 host harness);
 * enabling -S later means host-porting snapf/snap1 to <stdarg.h>. */
#define TINY		1	/* no codegen-debug (-S) dumps for the host build */

/* ---- machine type codes (index for IVAL_T/LVAL_T/DVAL_T; == i8086) ------ */
#define S8	0	/* signed byte */
#define U8	1	/* unsigned byte */
#define S16	2	/* signed word */
#define U16	3	/* unsigned word */
#define S32	4	/* signed long */
#define U32	5	/* unsigned long */
#define F32	6	/* short float */
#define F64	7	/* long float */
#define BLK	8	/* block of bytes */
#define FLD8	9	/* bit field, byte wide */
#define FLD16	10	/* bit field, word wide */
#define LPTR	11	/* large (2-word seg:off) pointer */
#define LPTB	12	/* large pointer to BLK */
#define SPTR	13	/* small (1-word) pointer  -- non-segmented use only */
#define SPTB	14	/* small pointer to BLK */

/* ---- derived type selectors (Z8001 int=16, long=32, double=64) --------- */
#define TRUTH	S16	/* type of truth values */
#define LOFS	LPTR	/* large offset LEAF type */
#define SOFS	SPTR	/* small offset LEAF type */
#define OFFS	LPTR	/* default offset LEAF type (segmented) == LOFS */
#define IVAL_T	S16	/* ival_t constant type */
#define LVAL_T	S32	/* lval_t constant type */
#define DVAL_T	F64	/* dval_t constant type */
#define I_FMT	"%d"
#define I_FMTX	"%x"
#define NBPBYTE	8
#define saligntype(ip)	salign(ip)	/* struct alignment = widest member (word data wants even) */

/* ---- per-type flag bits (1<<typecode; used by the .t type-sets) -------- */
#define FS8	01
#define FU8	02
#define FS16	04
#define FU16	010
#define FS32	020
#define FU32	040
#define FF32	0100
#define FF64	0200
#define FBLK	0400
#define FFLD8	01000
#define FFLD16	02000
#define FLPTR	04000
#define FLPTB	010000
#define FSPTR	020000
#define FSPTB	040000

/* ---- target value typedefs ---------------------------------------------
 * EXACT-WIDTH: these must hold the TARGET's 16/32-bit values correctly
 * regardless of the HOST word size, so constant folding is correct on a 64-bit
 * Linux host AND identical on the 16-bit Z8001 self-host. Do NOT bind them to
 * host int/long like the i8086 original. (On the 16-bit self-host these
 * stdint types resolve to int/long anyway.)
 */
#include <stdint.h>
typedef int16_t  ival_t;	/* target int  (16-bit) */
typedef int32_t  lval_t;	/* target long (32-bit) */
typedef char     dval_t[8];	/* target double image */
typedef int32_t  sizeof_t;	/* object sizes (SIZEOF_LARGE) */
typedef uint16_t uival_t;
typedef uint32_t ulval_t;

/* ---- limits / range-check masks (== i8086; target widths match) -------- */
#define MAXIV	32767L		/* max signed int */
#define MAXUV	65535L		/* max unsigned int */
#define MAXUCE	255
#define UIMASK	0xFFFF0000L	/* unsigned-int overflow check */
#define SIMASK	0xFFFF8000L	/* signed-int overflow check */
#define SLMASK	0x80000000L	/* signed-long sign */
#define ASMASK	0x0000FFFFL	/* WITHIN-SEGMENT offset mask (segment carried separately) */
#define MAXMEMB	MAXIV		/* max struct member offset */
#define MAXESIZE MAXUV		/* max aggregate size (within a 64K segment) */

/* ======================================================================== *
 * REGISTER MODEL                                                           *
 *   Word regs R0..R15 are numbered 0..15 == their HARDWARE encoding (so    *
 *   asm.c needs no remap for the common case). Pairs/quads/byte-regs get   *
 *   internal numbers above 15; their hardware encoding is derived in       *
 *   asm.c (pair = even word #, byte = grouped RH0-7=0-7 / RL0-7=8-15).     *
 *   Allocation roles are from the calling convention:                      *
 *     R0-R5  caller-saved/scratch; returns int=R1, long/ptr=RR0, dbl=RQ0   *
 *     R6-R12 callee-saved (allocatable, saved in prolog if used)           *
 *     R13    frame pointer   R14:R15 = RR14 = segmented stack pointer       *
 * ======================================================================== */
#define R0  0
#define R1  1
#define R2  2
#define R3  3
#define R4  4
#define R5  5
#define R6  6
#define R7  7
#define R8  8
#define R9  9
#define R10 10
#define R11 11
#define R12 12
#define R13 13		/* frame pointer (FP) */
#define R14 14		/* stack segment (high half of RR14) */
#define R15 15		/* stack offset  (low half of RR14)  */

/* register pairs (32-bit: long, segmented pointer, ADDL/SUBL/MULT/DIV) */
#define RR0  16
#define RR2  17
#define RR4  18
#define RR6  19
#define RR8  20
#define RR10 21
#define RR12 22
#define RR14 23		/* = stack pointer pair (seg:off) */

/* register quads (64-bit: double) */
#define RQ0  24
#define RQ4  25
#define RQ8  26
#define RQ12 27

/* byte regs (only R0..R7 have byte views; GROUPED encoding per the decoder:
 * RH0..RH7 = hw encode 0..7 (high bytes), RL0..RL7 = hw encode 8..15 (low)). */
#define RH0 28		/* high bytes of R0..R7 */
#define RH1 29
#define RH2 30
#define RH3 31
#define RH4 32
#define RH5 33
#define RH6 34
#define RH7 35
#define RL0 36		/* low bytes of R0..R7 */
#define RL1 37
#define RL2 38
#define RL3 39
#define RL4 40
#define RL5 41
#define RL6 42
#define RL7 43

/* pseudo / code-generator registers (== i8086 set) */
#define NONE	44	/* no register */
#define ANYR	45	/* any rvalue register */
#define ANYL	46	/* any lvalue register */
#define PAIR	47	/* a register pair */
#define TEMP	48	/* a temporary */
#define LOTEMP	49	/* low half of a temp pair */
#define HITEMP	50	/* high half of a temp pair */

#define NRREG	44	/* # real regs (0..43) */
#define NREG	51	/* # regs incl. pseudos */
#define FRREG	R0	/* first real reg */

/* per-(word-)register busy bits. Pair/quad/byte busy = OR of constituent word
 * bits, computed in the allocator (table1.c REGDESC), not given separate bits. */
typedef uint32_t PREGSET;
#define BREG(n)	(1UL<<(n))	/* BREG(R3) etc. */

/* the byte-addressable registers: only R0..R7 have RHn/RLn views (cf. the
 * i8086, where byte values were restricted to AX/BX/CX/DX). */
#define BYTEREGS (BREG(R0)|BREG(R1)|BREG(R2)|BREG(R3)|BREG(R4)|BREG(R5)|BREG(R6)|BREG(R7))

/* allocation class sets (the i8086 original keeps these in cc1mch.h; placed
 * here for the draft -- move to cc1mch.h to match the upstream split). */
#define SWREG	R0			/* switch/jump-table scratch register */
#define FNUSED	(BREG(R0)|BREG(R1)|BREG(R2)|BREG(R3)|BREG(R4)|BREG(R5)) /* CALL clobbers scratch */
#define NOFREE	(BREG(R13)|BREG(R14)|BREG(R15))	/* FP + SP pair always reserved */

/* ======================================================================== *
 * ADDRESS-MODE ENCODING (compiler-internal a_mode; nibble-packed == i8086) *
 *   bits 15-12 getfield/peephole flags                                     *
 *   bits 11- 8 A_PREFX  -> SEGMENT-present flag (7-bit seg # lives in a     *
 *                          companion AFIELD member, NOT packed here; the    *
 *                          16-bit offset lives there too, masked by ASMASK) *
 *   bits  7- 4 A_AMOD   -> addressing mode                                  *
 *   bits  3- 0 A_REGM   -> register code                                    *
 * ======================================================================== */
#define A_REGM	0x000F		/* register code */
#define A_AMOD	0x00F0		/* address mode */
#define A_PREFX	0x0F00		/* segment selector / prefix */
#define A_EA	0x1000		/* peephole: effective address */
#define A_OFFS	0x2000		/* getfield: offset present */
#define A_LID	0x4000		/* getfield: local id present */
#define A_GID	0x8000		/* getfield: global id present */
#define A_NONE	0

/* A_AMOD values (the Z8001 addressing modes; semantics from decode.go SrcSel) */
#define A_WR	(1<<4)		/* Rn          word register          */
#define A_BR	(2<<4)		/* RHn/RLn     byte register          */
#define A_IMM	(3<<4)		/* #data       immediate              */
#define A_IR	(4<<4)		/* @Rn (@RRn)  indirect register      */
#define A_DIR	(5<<4)		/* seg:offset  direct address (DA)    */
#define A_X	(6<<4)		/* addr(Rn)    indexed (DA + index)   */
#define A_IMML	(7<<4)		/* #data32     long (32-bit) immediate */
#define A_BA	(7<<4)		/* Rn(#disp)   base + 16-bit disp     */
#define A_BX	(8<<4)		/* Rn(Rm)      base + index register  */

/* A_PREFX: on Z8001 there are no segment-override registers like the i8086.
 * The flag just records "an explicit segment accompanies this address"; the
 * 7-bit segment number is carried in the companion AFIELD. Reserve one
 * value: */
#define A_SEG	(1<<8)		/* segment number present in companion field */
#define A_CS	(2<<8)		/* address is in the code space (vs data) */

/* canonical combinations (arch-independent helpers) */
#define GIDFMT	(A_GID|A_DIR)	/* global-id direct */
#define LIDFMT	(A_LID|A_DIR)	/* local-id  direct */
#define CONFMT	(A_OFFS|A_IMM)	/* immediate constant */

/* ---- calling convention / stack-frame constants ------------------------ */
#define DOWN	1		/* stack grows downward */
#define ICALLS	1		/* free a level on call */
#define FPREG	R13		/* frame pointer register */
#define SPREG	R15		/* stack pointer (offset half of RR14) */
#define SSEGREG	R14		/* stack segment (high half of RR14)  */
#define ARGBASE	4		/* first arg at SP+4 (past the 4-byte segmented return PC) */
#define mapssize(i)	(i)	/* stack-size roundup: word granularity (no-op) */

/* return registers by type are defined in n1/z8001/table1.c pertype[]:
 *   int -> R1 ;  long/pointer -> RR0 (R0=hi/seg, R1=lo/off) ;  double -> RQ0  */
