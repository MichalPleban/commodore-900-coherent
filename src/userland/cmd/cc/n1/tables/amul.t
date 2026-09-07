/ amul.t - Z8001 compound multiply 'AMUL' (x *= y).  Like mul.t but load-modify-
/ store the lvalue: load it into R1 (the multiplicand/low half of RR0), MULT
/ RR0,rhs (product low word -> R1), store R1 back.  Low word is sign-agnostic.

AMUL:
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R1
		ADR|LV	WORD
		ADR|IMM	WORD
		[ZLD]	[REGNO R1],[AL]
		[ZMULT]	[REGNO RR0],[AR]
		[ZLD]	[AL],[REGNO R1]

/ 32-bit `l *= r':  load lvalue into RR2 (multiplicand/low half of RQ0), MULTL
/ RQ0,rhs (product low 32 -> RR2), store RR2 back.  Low word is sign-agnostic.
%	PEFFECT|PRVALUE
	LONG	RQ0	*	*	RR2
		ADR|LV	LONG
		ADR|IMM	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZMULTL]	[REGNO RQ0],[AR]
		[ZLDL]		[AL],[REGNO RR2]

/ char lvalue (word-typed node over a byte).  The Z8000 has no byte MULT, so the
/ product is taken in a word -- but the multiplicand needs NO widening: a product
/ modulo 256 depends only on the factors modulo 256, so whatever the temp's high
/ byte held cannot reach the stored byte.  Load the byte into R1 (low half of the
/ RR0 multiplicand), MULT by the word rhs, store the product's low byte back.
/ The node is int-typed, so a VALUE context wants the stored char widened back to
/ an int: sign-extend (EXTSB) for signed, clear the high byte for unsigned.
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R1
		ADR|LV	FS8
		ADR|IMM	WORD
		[ZLDB]	[LO REGNO R1],[AL]
		[ZMULT]	[REGNO RR0],[AR]
		[ZLDB]	[AL],[LO REGNO R1]
	[IFV]	[ZEXTSB]	[REGNO R1]
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R1
		ADR|LV	FU8
		ADR|IMM	WORD
		[ZLDB]	[LO REGNO R1],[AL]
		[ZMULT]	[REGNO RR0],[AR]
		[ZLDB]	[AL],[LO REGNO R1]
	[IFV]	[ZCLRB]	[HI REGNO R1]
