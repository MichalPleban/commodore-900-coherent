/*
 * h/z8001/pseudops.h
 * Compiler PSEUDO-OPCODES for the n1->n2 CODE-record stream. These are NOT real
 * Z8000 instructions: they direct n2 to emit DATA (initializers, jump-table
 * entries) rather than encode an instruction. generated/opcode.h holds only the
 * real, decoder-derived opcodes (162, indices 0..161); the pseudo-ops live
 * above that range but below 256 (bput() writes a byte). n2/z8001 recognizes an
 * opcode >= ZPSEUDO_BASE and handles it as a data directive instead of indexing
 * the instruction optab. (i8086 numbered these ZBYTE..ZGPTR = 133..136.)
 */
#ifndef PSEUDOPS_H
#define PSEUDOPS_H

#define	ZPSEUDO_BASE	0xC0		/* 192: above real opcodes, < 256 */

#define	ZBYTE	(ZPSEUDO_BASE+0)	/* emit one byte			*/
#define	ZWORD	(ZPSEUDO_BASE+1)	/* emit one 16-bit word			*/
#define	ZLPTR	(ZPSEUDO_BASE+2)	/* emit a long (segmented seg:off) pointer */
#define	ZGPTR	(ZPSEUDO_BASE+3)	/* emit a global pointer		*/

/*
 * Conditional relative-jump band.  cc1 emits a branch as one opcode byte ZJREL|cc
 * (cc = the 4-bit Z8000 condition code) followed by the target label, in the
 * 0xD0..0xDF range -- above the real opcodes and the data pseudo-ops.  n2 turns
 * each into a span-dependent JUMP node (getcode), then JR cc at emit.  (cc1mch.h
 * defines the same value on the n1 side; guarded so a unit that pulls both agrees.)
 */
#ifndef	ZJREL
#define	ZJREL	0xD0		/* OR in a 4-bit cc -> 0xD0..0xDF	*/
#endif

#endif /* PSEUDOPS_H */
