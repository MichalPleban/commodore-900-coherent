/ sub.t - Z8001 selection rules for binary '-' (SUB:). DRAFT; grammar validated
/ via tabgen. Opcodes from ../../../generated/opcode.h. Mirrors add.t.

SUB:

/ x - 1  ->  DEC Rd,#1
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		1|MMX	*
		[ZDEC]	[R],[CONST 1]
	[IFR]	[REL0]	[LAB]

/ general 16-bit subtract
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		ADR|IMM	WORD
		[ZSUB]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ byte subtract
%	PEFFECT|PRVALUE|PSREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		ADR|IMM	BYTE
		[ZSUBB]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ 32-bit subtract (single Z8000 instruction).
/ NODE/LEFT types and the PLVALUE arm exactly as in add.t, for the subtracting form of
/ the same idiom, `*(char *)(<long constant> - (long)i)'.  Here the difference itself
/ comes out typed LPTX and its left operand (the constant) LONG -- the mirror image of
/ the ADD case -- which is why both types must be admitted on both.  See add.t.
%	PEFFECT|PRVALUE|PSREL|P_SLT
	LONG|LPTX	ANYR	ANYR	*	TEMP
		TREG	LONG|LPTX
		ADR|IMM	LONG
%	PLVALUE|P_SLT
	LONG|LPTX	ANYL	ANYL	*	TEMP
		TREG	LONG|LPTX
		ADR|IMM	LONG
		[ZSUBL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ pointer - int  (segmented far pointer LPTX - scaled WORD offset).  Mirrors add.t's
/ `pointer + int': subtract the WORD from the OFFSET half [LO R]; the segment half
/ [HI R] rides along unchanged (in-segment arithmetic).  Reached by `a + n' / `&a[n]'
/ for a LOCAL array, which cc0 forms as (FP + n) - frame_offset -- a far pointer minus
/ a constant -- and by any `p - i' on a far pointer.
%	PEFFECT|PRVALUE|P_SLT
	LPTX	ANYR	ANYR	*	TEMP
		TREG	LPTX
		ADR|IMM	WORD
%	PLVALUE|P_SLT
	LPTX	ANYL	ANYL	*	TEMP
		TREG	LPTX
		ADR|IMM	WORD
		[ZSUB]	[LO R],[AR]
