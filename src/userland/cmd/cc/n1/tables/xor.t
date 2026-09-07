/ xor.t - Z8001 selection rules for binary 'XOR' (logical, word+byte). DRAFT.

XOR:

%	PEFFECT|PRVALUE|PEREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		ADR|IMM	WORD
		[ZXOR]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		ADR|IMM	BYTE
		[ZXORB]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ 32-bit XOR: two 16-bit XORs on the pair halves (the Z8000 has no 32-bit logical).
%	PEFFECT|PRVALUE|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		ADR|IMM	LONG
			[ZXOR]	[HI R],[HI AR]
			[ZXOR]	[LO R],[LO AR]
