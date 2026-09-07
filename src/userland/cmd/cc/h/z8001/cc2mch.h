/*
 * h/z8001/cc2mch.h
 * Machine-specific definitions for the C compiler's final phase (cc2, the
 * direct-to-bits assembler/object writer). Segmented Z8001.
 * Template: h/i8086/cc2mch.h. The instruction-encoding STYLE codes (OF_*) live
 * in generated/OF_styles.h (shared with n2/z8001/optab.c); cc2.h includes this
 * header, so the styles are visible to getcode()/assemble().
 *
 * Z8001 vs i8086: ADDRESS is a 16-bit within-segment offset (the 7-bit segment
 * is tracked separately, per the l.out seg:offset model). The Z8000 has ONE
 * unconditional jump (JP, the UNCON relation) and its conditional relatives all
 * reverse via the 4-bit condition code, so the i8086 channel/reverse predicates
 * (which excluded the x86 LOOP/JCXZ family) collapse to "every relative branch
 * is reversible/channelable except the data pseudo-ops".
 */
#ifndef CC2MCH_H
#define CC2MCH_H

#include "OF_styles.h"		/* the OF_* encoding styles (Z8001) */

typedef	unsigned long	ADDRESS;		/* seg:offset (32-bit); the low 16 are	*/
typedef	long		SIGNEDADDRESS;		/* the within-segment offset at emit	*/

#define	UNCON	8		/* JUMP i_rel for an unconditional branch = cc T (always) */
#define	ischnrel(rel)	(1)	/* Z8000 conditional jumps all channel		*/
#define	isrevrel(rel)	((rel) != UNCON)	/* every conditional cc reverses (^8)	*/

/*
 * Opcode-table flags (optab.c third column; match generated/optab.c). OP_BYTE/
 * OP_DWORD select the byte / register-pair instruction variant; OP_JUMP marks a
 * relative branch (getcode turns it into a JUMP record for span-dependent
 * sizing); OP_NPTR is unused on the Z8000 (kept for table compatibility).
 */
#define	OP_BYTE		010	/* byte instruction		*/
#define	OP_NPTR		020	/* no PTR in listing (data directive)	*/
#define	OP_DWORD	040	/* register-pair (long) instruction */
/* OP_JUMP (01) and OP_DD (02) are MI-owned (cc2.h); a span-dependent relative
 * branch ORs OP_JUMP into op_flag, a data directive ORs OP_DD.  No 0100 flag --
 * that collided with the MI's OP_JUMP. */

/*
 * Data-emission pseudo-ops (the MI optim/getfun treat a CODE record whose i_op is
 * in [ZBYTE..ZGPTR] as initialized data, not an instruction).  These are NOT new
 * defines here -- they are the n1->n2 i1 contract in pseudops.h (ZPSEUDO_BASE 0xC0,
 * ABOVE the real opcodes), the exact bytes cc1's gen2.c writes via bput().  The
 * opinfo[] table carries matching rows at 192..195.
 */
#include "pseudops.h"		/* ZBYTE/ZWORD/ZLPTR/ZGPTR = 0xC0..0xC3	*/

/*
 * Data-emission styles for those opinfo[] rows (the OF_* in OF_styles.h are the
 * instruction-encoding styles 0..NOF_STYLE-1; these continue the enum for the four
 * data pseudo-ops).  The emitter routes OF_WORD/OF_LPTR/OF_GPTR/OF_BYTE_ to the
 * data writer instead of the opcode fuser; OF_WORD also marks switch-table words
 * for optim.c's jump-threading scan.
 */
#define	OF_BYTE_	(NOF_STYLE+0)	/* .byte				*/
#define	OF_WORD		(NOF_STYLE+1)	/* .word (also switch-table entry)	*/
#define	OF_LPTR		(NOF_STYLE+2)	/* near-pointer datum			*/
#define	OF_GPTR		(NOF_STYLE+3)	/* far-pointer datum			*/

/*
 * l.out relocation segment codes (the low byte of a reloc record names the segment
 * whose linked base ld adds).  Shared by genins/emitaddr (emit1.c) and the object
 * writer (outcoh.c).
 */
#define	L_SHRI	0		/* text (shared instruction) segment	*/
#define	L_PRVD	4		/* private data segment			*/
#define	L_BSSD	5		/* private bss (uninitialized data)	*/

/*
 * Externals.
 */
extern	int	framesize;	/* size of the current function's stack frame */
extern	int	framemask;	/* callee-saved register-variable mask (from AUTOS) */

#endif /* CC2MCH_H */
