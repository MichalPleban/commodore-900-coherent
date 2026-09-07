/ AXOR.t - Z8001 compound bitwise xor 'AXOR' (x ^= y).  Z8000 arithmetic is register-
/ destination only, so a compound op on a memory lvalue is load-modify-store:
/ load the lvalue into a result temp R, apply the op with the rhs, store R back.

AXOR:
%	PEFFECT|PRVALUE|PSREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	WORD
		ADR|IMM	WORD
		[ZLD]	[R],[AL]
		[ZXOR]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL
	BYTE	ANYR	*	*	TEMP
		ADR|LV	BYTE
		ADR|IMM	BYTE
		[ZLDB]	[LO R],[AL]
		[ZXORB]	[LO R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFR]	[REL0]	[LAB]

/ 32-bit long lvalue: ZXOR each half (the Z8000 has no long logical op), store the pair.
%	PEFFECT|PRVALUE
	LONG	ANYR	*	*	TEMP
		ADR|LV	LONG
		ADR|IMM	LONG
		[ZLDL]	[R],[AL]
		[ZXOR]	[LO R],[LO AR]
		[ZXOR]	[HI R],[HI AR]
		[ZLDL]	[AL],[R]

/ char lvalue (word-typed node over a byte lvalue): word op, store the low byte.  A
/ used value ([IFV]) is the STORED char widened to int, not the word temp over a stale
/ high half: EXTSB truncates to the byte and sign-extends, unsigned clears it.
%	PEFFECT|PRVALUE|PEREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		ADR|IMM	WORD
		[ZLDB]	[LO R],[AL]
		[ZXOR]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
	[IFR]	[ZORB]	[LO R],[LO R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		ADR|IMM	WORD
		[ZLDB]	[LO R],[AL]
		[ZXOR]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
	[IFR]	[ZORB]	[LO R],[LO R]
	[IFR]	[REL0]	[LAB]

/ Bit-field axor (FIELD lvalue).  modlfld already masked+shifted the rhs into the field
/ position ([AR]), so the masked value is simply ZXOR'd into the loaded object -- no
/ carry to contain, hence no re-mask of the result.  Load-modify-store (no Z8000
/ memory-destination logical op); [IFV] extracts the field for a value context.
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD16
		ADR|IMM		WORD
			[ZLD]	[R],[AL]
			[ZXOR]	[R],[AR]
			[ZLD]	[AL],[R]
		[IFV]	[ZAND]	[R],[LO EMASK]

%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD8
		ADR|IMM		WORD
			[ZLDB]	[LO R],[AL]
			[ZXORB]	[LO R],[LO AR]
			[ZLDB]	[AL],[LO R]
		[IFV]	[ZANDB]	[LO R],[LO EMASK]
