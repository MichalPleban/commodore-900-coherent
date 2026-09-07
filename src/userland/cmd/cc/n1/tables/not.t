/ not.t - Z8001 selection rules for unary '~' (operator COM:). DRAFT.

COM:

%	PEFFECT|PRVALUE|PEREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		*	*
		[ZCOM]	[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		*	*
		[ZCOMB]	[R]
	[IFR]	[REL0]	[LAB]

/ long ~l: the Z8000 has no COML, so complement each half (bitwise, no carry).
%	PEFFECT|PRVALUE|PEREL|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		*	*
		[ZCOM]	[HI R]
		[ZCOM]	[LO R]
	[IFR]	[ZTESTL]	[R]
	[IFR]	[REL0]	[LAB]
