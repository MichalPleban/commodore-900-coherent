/ neg.t - Z8001 selection rules for unary '-' (NEG:). DRAFT.

NEG:

%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		*	*
		[ZNEG]	[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		*	*
		[ZNEGB]	[R]
	[IFR]	[REL0]	[LAB]

/ long -l: the Z8000 has no NEGL, so form the two's complement -- complement each
/ half (~l) then add 1 to the pair (ADDL carries low->high).
%	PEFFECT|PRVALUE|PSREL|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		*	*
		[ZCOM]	[HI R]
		[ZCOM]	[LO R]
		[ZADDL]	[R],[LCONST 1]
	[IFR]	[ZTESTL]	[R]
	[IFR]	[REL0]	[LAB]
