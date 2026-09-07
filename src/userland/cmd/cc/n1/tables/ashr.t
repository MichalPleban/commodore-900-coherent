/ ashr.t - Z8001 compound shift-right 'ASHR' (x >>= n).  Load-modify-store; right
/ shift = SLL/SLA (or dynamic SDL/SDA) with a NEGATIVE count (decoder infers SRL/
/ SRA).  Constant count was negated by modtree; variable count is NEG'd at runtime.
/ signed (FS16) -> arithmetic (SRA); unsigned (UWORD) -> logical (SRL).

ASHR:
%	PEFFECT|PRVALUE|PSREL
	FS16	ANYR	*	*	TEMP
		ADR|LV	FS16
		IMM|MMX	WORD
		[ZLD]	[R],[AL]
		[ZSLA]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL
	UWORD	ANYR	*	*	TEMP
		ADR|LV	UWORD
		IMM|MMX	WORD
		[ZLD]	[R],[AL]
		[ZSLL]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL
	FS16	ANYR	*	*	TEMP
		ADR|LV	FS16
		TREG	WORD
		[ZLD]	[R],[AL]
		[ZNEG]	[AR]
		[ZSDA]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL
	UWORD	ANYR	*	*	TEMP
		ADR|LV	UWORD
		TREG	WORD
		[ZLD]	[R],[AL]
		[ZNEG]	[AR]
		[ZSDL]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

/ 32-bit long lvalue: load pair, right shift (negated count: signed SLAL / unsigned
/ SLLL static, SDAL/SDLL dynamic with a runtime NEG), store the pair back.
%	PEFFECT|PRVALUE
	FS32	ANYR	*	*	TEMP
		ADR|LV	FS32
		IMM|MMX	WORD
		[ZLDL]	[R],[AL]
		[ZSLAL]	[R],[AR]
		[ZLDL]	[AL],[R]
%	PEFFECT|PRVALUE
	FU32	ANYR	*	*	TEMP
		ADR|LV	FU32
		IMM|MMX	WORD
		[ZLDL]	[R],[AL]
		[ZSLLL]	[R],[AR]
		[ZLDL]	[AL],[R]
%	PEFFECT|PRVALUE
	FS32	ANYR	*	*	TEMP
		ADR|LV	FS32
		TREG	WORD
		[ZLDL]	[R],[AL]
		[ZNEG]	[AR]
		[ZSDAL]	[R],[AR]
		[ZLDL]	[AL],[R]
%	PEFFECT|PRVALUE
	FU32	ANYR	*	*	TEMP
		ADR|LV	FU32
		TREG	WORD
		[ZLDL]	[R],[AL]
		[ZNEG]	[AR]
		[ZSDLL]	[R],[AR]
		[ZLDL]	[AL],[R]

/ char lvalue (word-typed node over a byte): promote the byte to a word (signed
/ EXTSB / unsigned CLRB high) so the right shift sees the correct bits, shift, store
/ the low byte.  Negated count for static; runtime NEG for dynamic.
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		IMM|MMX	WORD
		[ZLDB]	[LO R],[AL]
		[ZEXTSB]	[R]
		[ZSLA]	[R],[AR]
		[ZLDB]	[AL],[LO R]
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		IMM|MMX	WORD
		[ZLDB]	[LO R],[AL]
		[ZCLRB]	[HI R]
		[ZSLL]	[R],[AR]
		[ZLDB]	[AL],[LO R]
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		TREG	WORD
		[ZLDB]	[LO R],[AL]
		[ZEXTSB]	[R]
		[ZNEG]	[AR]
		[ZSDA]	[R],[AR]
		[ZLDB]	[AL],[LO R]
%	PEFFECT|PRVALUE
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		TREG	WORD
		[ZLDB]	[LO R],[AL]
		[ZCLRB]	[HI R]
		[ZNEG]	[AR]
		[ZSDL]	[R],[AR]
		[ZLDB]	[AL],[LO R]
