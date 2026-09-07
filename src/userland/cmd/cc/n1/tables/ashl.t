/ ashl.t - Z8001 compound shift-left 'ASHL' (x <<= n).  Load-modify-store (Z8000
/ shifts are register-only): load lvalue into temp R, shift, store R back.  Left
/ shift is sign-agnostic.  Constant count -> static SLL; variable -> dynamic SDL.

ASHL:
%	PEFFECT|PRVALUE|PSREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	WORD
		IMM|MMX	WORD
		[ZLD]	[R],[AL]
		[ZSLL]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	WORD
		TREG	WORD
		[ZLD]	[R],[AL]
		[ZSDL]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

/ 32-bit long lvalue: load pair, long shift left (static SLLL / dynamic SDLL), store.
%	PEFFECT|PRVALUE
	LONG	ANYR	*	*	TEMP
		ADR|LV	LONG
		IMM|MMX	WORD
		[ZLDL]	[R],[AL]
		[ZSLLL]	[R],[AR]
		[ZLDL]	[AL],[R]
%	PEFFECT|PRVALUE
	LONG	ANYR	*	*	TEMP
		ADR|LV	LONG
		TREG	WORD
		[ZLDL]	[R],[AL]
		[ZSDLL]	[R],[AR]
		[ZLDL]	[AL],[R]

/ char lvalue (word-typed node over a byte): left shift the word temp (the low byte
/ of the result depends only on the low byte), store the low byte.  Static + dynamic.
/ A left shift carries bits OUT of the byte, so a used value ([IFV]) must come from the
/ STORED byte, not the word temp (`(c <<= 3)' from 127 leaves 0x03F8 and reads as +1016
/ instead of -8): EXTSB truncates to the byte and sign-extends, unsigned clears the high
/ byte.  Split by signedness, as ashr.t already is.
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		IMM|MMX	WORD
		[ZLDB]	[LO R],[AL]
		[ZSLL]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		IMM|MMX	WORD
		[ZLDB]	[LO R],[AL]
		[ZSLL]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		TREG	WORD
		[ZLDB]	[LO R],[AL]
		[ZSDL]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		TREG	WORD
		[ZLDB]	[LO R],[AL]
		[ZSDL]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
