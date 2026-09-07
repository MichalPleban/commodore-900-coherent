/ arem.t - Z8001 compound remainder 'AREM' (x %= y).  As adiv but the result is the
/ remainder, left in R0 (high word of RR0) after DIV; store R0 back to the lvalue.

AREM:
%	PEFFECT|PRVALUE
	UWORD	RR0	*	*	R0
		ADR|LV	WORD
		IMM|MMX	WORD
		[ZLD]	[REGNO R1],[AL]
		[ZCLR]	[REGNO R0]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLD]	[AL],[REGNO R0]
/ An unsigned divisor the one-word DIV cannot hold (anything but a constant with bit
/ 15 clear) arrives widened to 32 bits (modoper): dividend into R3, the low word of
/ the 64-bit RQ0 dividend, rest zeroed, DIVL, and the remainder's low word R1 is the
/ 16-bit value stored back.
%	PEFFECT|PRVALUE
	UWORD	RQ0	*	*	R1
		ADR|LV	WORD
		ADR|IMM	LONG
		[ZLD]		[REGNO R3],[AL]
		[ZSUBL]		[REGNO RR0],[REGNO RR0]
		[ZCLR]		[REGNO R2]
		[ZDIVL]		[REGNO RQ0],[AR]
		[ZLD]		[AL],[REGNO R1]
%	PEFFECT|PRVALUE
	FS16	RR0	*	*	R0
		ADR|LV	WORD
		ADR|IMM	WORD
		[ZLD]	[REGNO R1],[AL]
		[ZEXTS]	[REGNO RR0]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLD]	[AL],[REGNO R0]

/ 32-bit `l %= r':  as the long ADIV but DIVL leaves the remainder in the high pair
/ RR0; load lvalue into RR2, extend into RR0, DIVL RQ0,rhs, store RR0 (remainder) back.
%	PEFFECT|PRVALUE
	FU32	RQ0	*	*	RR0
		ADR|LV	LONG
		IMM|MMX	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZSUBL]		[REGNO RR0],[REGNO RR0]
		[ZDIVL]		[REGNO RQ0],[AR]
		[ZLDL]		[AL],[REGNO RR0]
/ An unsigned RUNTIME divisor: DIVL is signed, so bit 31 set reads as negative -- take
/ the dividend-or-dividend-minus-divisor remainder arm for such a divisor (div.t carries
/ the reasoning).
%	PEFFECT|PRVALUE
	FU32	RQ0	*	*	RR0
		ADR|LV	LONG
		ADR	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZTESTL]	[AR]
		[ZJRPL]		[LAB0]
		[ZLDL]		[REGNO RR0],[REGNO RR2]
		[ZCPL]		[REGNO RR0],[AR]
		[ZJRULT]	[LAB1]
		[ZSUBL]		[REGNO RR0],[AR]
		[ZJP]		[LAB1]
	[DLAB0]:[ZSUBL]		[REGNO RR0],[REGNO RR0]
		[ZDIVL]		[REGNO RQ0],[AR]
	[DLAB1]:[ZLDL]		[AL],[REGNO RR0]
%	PEFFECT|PRVALUE
	FS32	RQ0	*	*	RR0
		ADR|LV	LONG
		ADR|IMM	LONG
		[ZLDL]		[REGNO RR2],[AL]
		[ZEXTSL]	[REGNO RR0]
		[ZDIVL]		[REGNO RQ0],[AR]
		[ZLDL]		[AL],[REGNO RR0]

/ char lvalue (word-typed node over a byte): the adiv byte dividend widening (signed
/ EXTSB+EXTS, unsigned a single zeroing SUBL), then the remainder rather than the
/ quotient -- DIV leaves it in R0, the high word of the pair.  Its sign follows the
/ dividend, so it is always in char range; store its low byte, and widen that byte
/ back for a VALUE context.
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R0
		ADR|LV	FS8
		ADR|IMM	WORD
		[ZLDB]	[LO REGNO R1],[AL]
		[ZEXTSB]	[REGNO R1]
		[ZEXTS]	[REGNO RR0]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLDB]	[AL],[LO REGNO R0]
	[IFV]	[ZEXTSB]	[REGNO R0]
%	PEFFECT|PRVALUE
	WORD	RR0	*	*	R0
		ADR|LV	FU8
		ADR|IMM	WORD
		[ZSUBL]	[REGNO RR0],[REGNO RR0]
		[ZLDB]	[LO REGNO R1],[AL]
		[ZDIV]	[REGNO RR0],[AR]
		[ZLDB]	[AL],[LO REGNO R0]
	[IFV]	[ZCLRB]	[HI REGNO R0]
