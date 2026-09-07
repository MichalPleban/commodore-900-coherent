/ shl.t - Z8001 '<<' (SHL).  One Z8000 SHIFT does an arbitrary-width shift in a
/ single (2-word) instruction; the count word/register is positive for a left
/ shift.  Left shift is bit-identical for signed and unsigned, so one logical
/ shift (SLL/SDL) serves both.  P_SLT shares the operand's temp as the in-place
/ result.  Constant count -> static SLL (count in word2); variable count -> the
/ count is forced into a register and the dynamic SDL is used.

SHL:
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		IMM|MMX	WORD
		[ZSLL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		TREG	WORD
		[ZSDL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ 32-bit left shift: native SLLL (constant count) / SDLL (variable count).
%	PEFFECT|PRVALUE|PSREL|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		IMM|MMX	WORD
		[ZSLLL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		TREG	WORD
		[ZSDLL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ Left shift whose RESULT is a far pointer (LPTX): the 4.2.12 MI materializes an
/ int back into a pointer this way (the (p - NULL) ... (NULL + n) idiom after its
/ divide/multiply-by-size folding, e.g. the kernel's align()).  Mirrors the i8086
/ donor rule exactly: shift the 16-bit value as the pair's OFFSET (low) word and
/ ZERO the segment (donor: SAL lo / SUB hi,hi) -- an int carries no segment, and
/ the donor contract materializes it as seg 0 (same as the int-constant->pointer
/ leaf rule).  NOTE: 0.7.3-era kernel code that needs the ORIGINAL 0.7.3 codegen
/ semantics (segment PRESERVED through align()) must go through long, not int --
/ the original front end never truncated the pair (see os/ C0.2 notes).
%	PEFFECT|PRVALUE|PSREL
	LPTX	ANYR	ANYR	*	TEMP
		TREG	WORD
		IMM|MMX	WORD
		[ZLD]	[LO R],[AL]
		[ZSLL]	[LO R],[AR]
		[ZCLR]	[HI R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL
	LPTX	ANYR	ANYR	*	TEMP
		TREG	WORD
		TREG	WORD
		[ZLD]	[LO R],[AL]
		[ZSDL]	[LO R],[AR]
		[ZCLR]	[HI R]
	[IFR]	[REL0]	[LAB]
