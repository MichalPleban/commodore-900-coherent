/ and.t - Z8001 selection rules for binary 'AND' (logical, word+byte). DRAFT.

AND:

%	PEFFECT|PRVALUE|PEREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		ADR|IMM	WORD
		[ZAND]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		ADR|IMM	BYTE
		[ZANDB]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ 32-bit AND: two 16-bit ANDs on the pair halves (the Z8000 has no 32-bit logical).
%	PEFFECT|PRVALUE|P_SLT
	LONG	ANYR	ANYR	*	TEMP
		TREG	LONG
		ADR|IMM	LONG
			[ZAND]	[HI R],[HI AR]
			[ZAND]	[LO R],[LO AR]

/ AND whose RESULT is a far pointer (LPTX): the kernel's align() idiom --
/ `(ALL *)((vaddr_t)(p) & ~1L)' -- reaches selection as an AND retyped LPTX
/ (the kind-equal CONVERT collapse).  Mask with the UPPER HALF ALL-ONES
/ (~1L: T_UHS) leaves the segment word untouched: ONE AND on the offset
/ word -- exactly the shipped 0.7.3 kernel's align() form (AND R1,#0xFFFE).
%	PEFFECT|PRVALUE|P_SLT
	LPTX	ANYR	ANYR	*	TEMP
		TREG	LONG|LPTX
		UHS|MMX	LONG
		[ZAND]	[LO R],[LO AR]

/ General 32-bit mask of a pointer pair: both halves.
%	PEFFECT|PRVALUE|P_SLT
	LPTX	ANYR	ANYR	*	TEMP
		TREG	LONG|LPTX
		ADR|IMM	LONG
		[ZAND]	[HI R],[HI AR]
		[ZAND]	[LO R],[LO AR]
