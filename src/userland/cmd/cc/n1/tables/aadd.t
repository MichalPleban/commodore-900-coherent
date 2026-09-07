/ aadd.t - Z8001 compound add 'AADD' (x += y).  The Z8000 has NO memory-destination
/ arithmetic (unlike the i8086), so a compound op on a memory lvalue is load-modify-
/ store: load the lvalue value into a result temp R, ADD the rhs, store R back.  R
/ also carries the value for an rvalue context (R is the result reg) and is tested
/ for a relational context ([IFR]).

AADD:
%	PEFFECT|PRVALUE|PSREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	WORD
		ADR|IMM	WORD
		[ZLD]	[R],[AL]
		[ZADD]	[R],[AR]
		[ZLD]	[AL],[R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL
	BYTE	ANYR	*	*	TEMP
		ADR|LV	BYTE
		ADR|IMM	BYTE
		[ZLDB]	[LO R],[AL]
		[ZADDB]	[LO R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFR]	[REL0]	[LAB]

/ 32-bit long lvalue: load the pair, ADDL, store the pair back.
%	PEFFECT|PRVALUE
	LONG	ANYR	*	*	TEMP
		ADR|LV	LONG
		ADR|IMM	LONG
		[ZLDL]	[R],[AL]
		[ZADDL]	[R],[AR]
		[ZLDL]	[AL],[R]

/ Far pointer REGISTER-pair lvalue (`register char *p; p += i'): the pair is already in
/ registers, so ADD the (element-scaled) offset to its LOW half IN PLACE -- no load-modify-
/ store round-trip (the original's form, e.g. `INC R7,#5').  Must precede the ADR|LV memory
/ rule below, since a register lvalue also matches LV.  A value context loads the new pair
/ into the result temp like bef.t; a pure EFFECT statement (`p += i;') is just the ADD.
%	PEFFECT|PRVALUE
	LPTX	ANYR	*	*	TEMP
		REG|MMX	LPTX
		ADR|IMM	WORD
		[ZADD]	[LO AL],[AR]
	[IFV]	[ZLDL]	[R],[AL]

/ Far pointer MEMORY lvalue (p += i): the Z8000 has no memory-destination ADD, so load the
/ pair, ADD the (already element-scaled) word offset to the LOW half, segment unchanged,
/ store the pair back.
%	PEFFECT|PRVALUE
	LPTX	ANYR	*	*	TEMP
		ADR|LV	LPTX
		ADR|IMM	WORD
		[ZLDL]	[R],[AL]
		[ZADD]	[LO R],[AR]
		[ZLDL]	[AL],[R]

/ char lvalue: C promotes char->int, so the AADD node is WORD-typed over a byte
/ lvalue (cf. the char ASSIGN in assign.t).  Load the byte into a word temp, ADD
/ the word rhs, store the low byte back; the low byte of the sum is correct
/ regardless of the temp's high half.  Test the stored low byte for a relational.
/ The VALUE of the expression is the STORED char widened to int, so when it is used
/ ([IFV]) re-derive it from the low byte: the word temp still holds the untruncated
/ sum over a stale high half (`(c += 91)' from 37 leaves 0x0080 and reads as +128
/ instead of -128).  EXTSB truncates to the byte and sign-extends in one instruction;
/ unsigned clears the high byte.  Effect-only `c += n;' emits nothing extra.
%	PEFFECT|PRVALUE|PEREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	FS8
		ADR|IMM	WORD
		[ZLDB]	[LO R],[AL]
		[ZADD]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
	[IFR]	[ZORB]	[LO R],[LO R]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL
	WORD	ANYR	*	*	TEMP
		ADR|LV	FU8
		ADR|IMM	WORD
		[ZLDB]	[LO R],[AL]
		[ZADD]	[R],[AR]
		[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
	[IFR]	[ZORB]	[LO R],[LO R]
	[IFR]	[REL0]	[LAB]

/ Bit-field += (FIELD lvalue).  modlfld preshifted the rhs to the field position
/ ([AR] = rhs<<base), but unlike |= it is NOT masked -- an add can carry out of the
/ field, so the result must be re-masked.  The Z8000 has no memory-destination op, so
/ load-modify-store; a single temp suffices via the identity
/   O_new = O XOR (((O + V) XOR O) & fieldmask)
/ which changes only the field bits (carry beyond the field is masked off, giving the
/ defined modular wrap).  EMASK is the field mask, from out.c.
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD16
		ADR|IMM		WORD
			[ZLD]	[R],[AL]
			[ZADD]	[R],[AR]
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
			[ZADDB]	[LO R],[LO AR]
			[ZXORB]	[LO R],[AL]
			[ZANDB]	[LO R],[LO EMASK]
			[ZXORB]	[LO R],[AL]
			[ZLDB]	[AL],[LO R]
		[IFV]	[ZANDB]	[LO R],[LO EMASK]
