/ or.t - Z8001 selection rules for binary 'OR' (logical, word+byte). DRAFT.

OR:

%	PEFFECT|PRVALUE|PEREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		ADR|IMM	WORD
		[ZOR]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		ADR|IMM	BYTE
		[ZORB]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ 32-bit OR: two 16-bit ORs on the pair halves (the Z8000 has no 32-bit logical).
%	PEFFECT|PRVALUE|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		ADR|IMM	LONG
			[ZOR]	[HI R],[HI AR]
			[ZOR]	[LO R],[LO AR]

/ OR whose RESULT is a far pointer (LPTX): the kernel's setused() tag --
/ `(char *)((vaddr_t)p | 1L)' -- an OR retyped LPTX by the kind-equal
/ CONVERT collapse.  An operand with the UPPER HALF ALL-ZERO (1L: T_UHC)
/ cannot touch the segment word: ONE OR on the offset word.
%	PEFFECT|PRVALUE|P_SLT
	LPTX	ANYR	ANYR	*	TEMP
		TREG	LONG|LPTX
		UHC|MMX	LONG
		[ZOR]	[LO R],[LO AR]

/ General 32-bit OR into a pointer pair: both halves.
%	PEFFECT|PRVALUE|P_SLT
	LPTX	ANYR	ANYR	*	TEMP
		TREG	LONG|LPTX
		ADR|IMM	LONG
		[ZOR]	[HI R],[HI AR]
		[ZOR]	[LO R],[LO AR]
