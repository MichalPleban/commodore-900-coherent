/ adiv.t - Z8001 compound divide 'ADIV' (x /= y).  Load lvalue into R1 (low word of
/ the RR0 dividend), extend into R0 (UNSIGNED: CLR zero-extend; SIGNED: EXTS sign-
/ extend), DIV RR0,rhs (quotient -> R1), store R1 back.

ADIV:
%	PEFFECT|PRVALUE
	UWORD	RR0	*	*	R1
		ADR|LV	WORD
		IMM|MMX	WORD
		[ZLD]	[REGNO R1],[AL]
		[ZCLR]	[REGNO R0]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLD]	[AL],[REGNO R1]
/ An unsigned divisor the one-word DIV cannot hold (anything but a constant with bit
/ 15 clear) arrives widened to 32 bits (modoper): dividend into R3, the low word of
/ the 64-bit RQ0 dividend, rest zeroed, DIVL, and the quotient's low word R3 is the
/ 16-bit value stored back.
%	PEFFECT|PRVALUE
	UWORD	RQ0	*	*	R3
		ADR|LV	WORD
		ADR|IMM	LONG
		[ZLD]		[REGNO R3],[AL]
		[ZSUBL]		[REGNO RR0],[REGNO RR0]
		[ZCLR]		[REGNO R2]
		[ZDIVL]		[REGNO RQ0],[AR]
		[ZLD]		[AL],[REGNO R3]
%	PEFFECT|PRVALUE
	FS16	RR0	*	*	R1
		ADR|LV	WORD
		ADR|IMM	WORD
		[ZLD]	[REGNO R1],[AL]
		[ZEXTS]	[REGNO RR0]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLD]	[AL],[REGNO R1]

/ 32-bit `l /= r':  load lvalue into RR2 (low half of the RQ0 dividend), extend into
/ RR0 (UNSIGNED: SUBL zero; SIGNED: EXTSL sign), DIVL RQ0,rhs (quotient -> RR2), store
/ RR2 back.
%	PEFFECT|PRVALUE
	FU32	RQ0	*	*	RR2
		ADR|LV	LONG
		IMM|MMX	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZSUBL]		[REGNO RR0],[REGNO RR0]
		[ZDIVL]		[REGNO RQ0],[AR]
		[ZLDL]		[AL],[REGNO RR2]
/ An unsigned RUNTIME divisor: DIVL is signed, so bit 31 set reads as negative -- take
/ the 0-or-1 quotient arm for such a divisor (div.t carries the reasoning).
%	PEFFECT|PRVALUE
	FU32	RQ0	*	*	RR2
		ADR|LV	LONG
		ADR	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZTESTL]	[AR]
		[ZJRPL]		[LAB0]
		[ZLDL]		[REGNO RR0],[REGNO RR2]
		[ZSUBL]		[REGNO RR2],[REGNO RR2]
		[ZCPL]		[REGNO RR0],[AR]
		[ZJRULT]	[LAB1]
		[ZSUBL]		[REGNO RR0],[AR]
		[ZLD]		[REGNO R3],[CONST 1]
		[ZJP]		[LAB1]
	[DLAB0]:[ZSUBL]		[REGNO RR0],[REGNO RR0]
		[ZDIVL]		[REGNO RQ0],[AR]
	[DLAB1]:[ZLDL]		[AL],[REGNO RR2]
%	PEFFECT|PRVALUE
	FS32	RQ0	*	*	RR2
		ADR|LV	LONG
		ADR|IMM	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZEXTSL]	[REGNO RR0]
		[ZDIVL]		[REGNO RQ0],[AR]
		[ZLDL]		[AL],[REGNO RR2]

/ char lvalue (word-typed node over a byte).  The Z8000 has no byte DIV, and unlike
/ the multiply the quotient DOES depend on the whole dividend, so the byte is widened
/ all the way to the 32-bit RR0 dividend before the divide.  The widening follows the
/ LVALUE's signedness (the divide itself is the node's, always signed here, since a
/ char promotes to int): signed sign-extends byte->word->long (EXTSB then EXTS);
/ unsigned zeroes the whole dividend first (one SUBL) and drops the byte into RL1.
/ Quotient -> R1; store its low byte, and for a VALUE context widen that byte back.
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R1
		ADR|LV	FS8
		ADR|IMM	WORD
		[ZLDB]	[LO REGNO R1],[AL]
		[ZEXTSB]	[REGNO R1]
		[ZEXTS]	[REGNO RR0]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLDB]	[AL],[LO REGNO R1]
	[IFV]	[ZEXTSB]	[REGNO R1]
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R1
		ADR|LV	FU8
		ADR|IMM	WORD
		[ZSUBL]	[REGNO RR0],[REGNO RR0]
		[ZLDB]	[LO REGNO R1],[AL]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLDB]	[AL],[LO REGNO R1]
	[IFV]	[ZCLRB]	[HI REGNO R1]
