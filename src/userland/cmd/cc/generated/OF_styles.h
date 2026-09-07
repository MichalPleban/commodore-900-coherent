/* OF_styles.h - n2/z8001 instruction-style codes for optab.c (DRAFT).
 * Analogous to the i8086 cc2mch.h OF_* set; asm.c dispatches on these to
 * fuse the optab base opcode with the packed a_mode address field. */
#ifndef OF_STYLES_H
#define OF_STYLES_H

#define	OF_IMPL     0	/* no operands (HALT/IRET/NOP/DI/EI/MBIT...) */
#define	OF_FLAG     1	/* immediate-only flag op (SC/SETFLG/COMFLG/RESFLG) */
#define	OF_DOP      2	/* two-operand ALU, word (ADD/AND/OR/SUB/XOR/ADC/SBC); reg|imm|IR|DA|X */
#define	OF_DOPB     3	/* two-operand ALU, byte */
#define	OF_DOPL     4	/* two-operand ALU, long (ADDL/SUBL/CPL) */
#define	OF_CP       5	/* compare (CP/CPB): ALU forms + addr,#i */
#define	OF_SOP      6	/* single-operand, word (CLR/COM/NEG/TEST/TSET/TESTL) */
#define	OF_SOPB     7	/* single-operand, byte */
#define	OF_INCDEC   8	/* INC/DEC: 4-bit immediate count in opcode */
#define	OF_MUL      9	/* MULT/DIV (RR/RQ pair/quad result) */
#define	OF_SHIFT    10	/* static shift SLA/SLL/SRA/SRL, signed count word */
#define	OF_SHIFTD   11	/* dynamic shift SDA/SDL, count in register */
#define	OF_ROT      12	/* rotate RL/RR/RLC/RRC/RLDB/RRDB/DAB */
#define	OF_EXTS     13	/* sign extend EXTS/EXTSB/EXTSL */
#define	OF_BIT      14	/* bit BIT/SET/RES (static or dynamic bit#) */
#define	OF_LD       15	/* load LD/LDB/LDL (all addressing forms) */
#define	OF_LDA      16	/* load address/relative LDA/LDAR/LDR */
#define	OF_LDK      17	/* load constant nibble LDK */
#define	OF_LDM      18	/* load/store multiple LDM (built specially) */
#define	OF_LDCTL_   19	/* control-register load LDCTL (privileged) */
#define	OF_EX       20	/* exchange EX/EXB */
#define	OF_PUSH     21	/* PUSH/PUSHL (@RR15) */
#define	OF_POP      22	/* POP/POPL */
#define	OF_JP       23	/* JP cc (DA/IR/X) */
#define	OF_JR       24	/* JR cc, relative-8 */
#define	OF_CALR     25	/* CALR relative-12 */
#define	OF_DJNZ     26	/* DJNZ r, relative-7 */
#define	OF_CALL     27	/* CALL/LDPS (DA/IR/X) */
#define	OF_RET      28	/* RET cc */
#define	OF_TCC      29	/* test condition code TCC/TCCB */
#define	OF_IO       30	/* IN/OUT/SIN/SOUT (privileged; not C-emitted) */
#define	OF_ASMONLY  31	/* block/privileged/special, asm-only (CPDx, LDCTL, ...); review */

#define	NOF_STYLE	32

#endif /* OF_STYLES_H */
