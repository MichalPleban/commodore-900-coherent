/ ASUB.t - Z8001 compound subtract 'ASUB' (x -= y).  Z8000 arithmetic is register-
/ destination only, so a compound op on a memory lvalue is load-modify-store:
/ load the lvalue into a result temp R, apply the op with the rhs, store R back.

ASUB:
%	PEFFECT|PRVALUE|PSREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	WORD
		ADR|IMM	WORD
		[ZLD]	[R],[AL]
		[ZSUB]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL
	BYTE	ANYR	*	*	TEMP
		ADR|LV	BYTE
		ADR|IMM	BYTE
		[ZLDB]	[LO R],[AL]
		[ZSUBB]	[LO R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFR]	[REL0]	[LAB]

/ 32-bit long lvalue: load the pair, SUBL, store the pair back.
%	PEFFECT|PRVALUE
	LONG	ANYR	*	*	TEMP
		ADR|LV	LONG
		ADR|IMM	LONG
		[ZLDL]	[R],[AL]
		[ZSUBL]	[R],[AR]
		[ZLDL]	[AL],[R]

/ Far pointer REGISTER-pair lvalue (`register char *p; p -= i'): subtract the scaled offset
/ from the OFFSET half IN PLACE -- no load-modify-store round-trip.  Must precede the ADR|LV
/ memory rule (a register lvalue also matches LV); REG|MMX forces an exact register match so a
/ memory lvalue falls through.  Value context loads the new pair into the result temp.
%	PEFFECT|PRVALUE
	LPTX	ANYR	*	*	TEMP
		REG|MMX	LPTX
		ADR|IMM	WORD
		[ZSUB]	[LO AL],[AR]
	[IFV]	[ZLDL]	[R],[AL]

/ Far pointer MEMORY lvalue (p -= i): no memory-destination SUB, so load the pair, subtract
/ the element-scaled offset from the low half, store the pair back.
%	PEFFECT|PRVALUE
	LPTX	ANYR	*	*	TEMP
		ADR|LV	LPTX
		ADR|IMM	WORD
		[ZLDL]	[R],[AL]
		[ZSUB]	[LO R],[AR]
		[ZLDL]	[AL],[R]

/ char lvalue (word-typed node over a byte lvalue): word op, store the low byte.  A
/ used value ([IFV]) is the STORED char widened to int, not the untruncated word temp
/ (`(c -= 1)' from 0 leaves 0x00FF and reads as +255): EXTSB truncates to the byte and
/ sign-extends, unsigned clears the high byte.  Split by signedness as in aadd.t.
%	PEFFECT|PRVALUE|PEREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		ADR|IMM	WORD
		[ZLDB]	[LO R],[AL]
		[ZSUB]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
	[IFR]	[ZORB]	[LO R],[LO R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		ADR|IMM	WORD
		[ZLDB]	[LO R],[AL]
		[ZSUB]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
	[IFR]	[ZORB]	[LO R],[LO R]
	[IFR]	[REL0]	[LAB]

/ Bit-field -= (FIELD lvalue).  Same single-temp masked load-modify-store as aadd.t
/ (O_new = O XOR (((O - V) XOR O) & fieldmask)); a borrow beyond the field is masked
/ off, giving the defined modular wrap.  [AR] = rhs<<base (preshifted, unmasked).
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD16
		ADR|IMM		WORD
			[ZLD]	[R],[AL]
			[ZSUB]	[R],[AR]
			[ZXOR]	[R],[AL]
			[ZAND]	[R],[LO EMASK]
			[ZXOR]	[R],[AL]
			[ZLD]	[AL],[R]
		[IFV]	[ZAND]	[R],[LO EMASK]

%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD8
		ADR|IMM		WORD
			[ZLDB]	[LO R],[AL]
			[ZSUBB]	[LO R],[LO AR]
			[ZXORB]	[LO R],[AL]
			[ZANDB]	[LO R],[LO EMASK]
			[ZXORB]	[LO R],[AL]
			[ZLDB]	[AL],[LO R]
		[IFV]	[ZANDB]	[LO R],[LO EMASK]
