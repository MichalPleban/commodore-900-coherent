/ div.t - Z8001 '/' (DIV:). Z8000 DIV RRd,Rs: 32-bit RRd / 16-bit Rs -> quotient
/ in the low word (Rd+1), remainder in the high word (Rd). We use the fixed pair
/ RR0; the C int result is the quotient = R1. The Z8000 DIV is SIGNED, so the
/ dividend extension depends on signedness: UNSIGNED zero-extends (CLR high word),
/ SIGNED sign-extends (EXTS).
DIV:
/ UNSIGNED first (UWORD = FU16): zero-extend the dividend. The one-word DIV is
/ exact for an unsigned divide only while the divisor < 0x8000, so this rule takes
/ the divisor a CONSTANT can be: bit 15 clear (bit 15 set is strength-reduced to a
/ compare in mtree2, since the quotient is then 0 or 1).
%	PEFFECT|PRVALUE
	UWORD		RR0	R1	*	R1
		TREG		WORD
		IMM|MMX		WORD
			[ZCLR]	[REGNO R0]
			[ZDIV]	[REGNO RR0],[AR]
/ Any other unsigned divisor arrives widened to 32 bits (modoper), zero-extended and
/ so positive, for the wider divisor of DIVL. The 16-bit dividend is the low word of
/ the 64-bit RQ0 dividend (R3) with the rest zeroed; the quotient lands in RR2 and is
/ at most 0xFFFF, so its low word R3 is the 16-bit result.
%	PEFFECT|PRVALUE
	UWORD		RQ0	R3	*	R3
		TREG		WORD
		ADR|IMM		LONG
			[ZSUBL]		[REGNO RR0],[REGNO RR0]
			[ZCLR]		[REGNO R2]
			[ZDIVL]		[REGNO RQ0],[AR]
/ SIGNED (FS16): sign-extend the dividend.
%	PEFFECT|PRVALUE
	FS16		RR0	R1	*	R1
		TREG		WORD
		ADR|IMM		WORD
			[ZEXTS]	[REGNO RR0]
			[ZDIV]	[REGNO RR0],[AR]

/ 32-bit divide: DIVL RQ0,Rs gives the quotient in the low pair RR2.  The dividend
/ (in RR2) is widened to the 64-bit RQ0 first -- sign-extend (EXTSL) for signed, or
/ zero the high pair (SUBL) for unsigned.
/ UNSIGNED by a CONSTANT: DIVL is exact.  The dividend is zero-extended and so
/ positive; a constant divisor with bit 31 set was rewritten to a compare in mtree2,
/ so every constant reaching here is under 0x80000000 and positive too.
%	PEFFECT|PRVALUE
	FU32		RQ0	RR2	*	RR2
		TREG		LONG
		IMM|MMX		LONG
			[ZSUBL]		[REGNO RR0],[REGNO RR0]
			[ZDIVL]		[REGNO RQ0],[AR]
/ UNSIGNED by a RUNTIME divisor: DIVL is the widest divide the machine has, and it is
/ SIGNED, so a divisor with bit 31 set reads as negative and the quotient comes out
/ negated.  Such a divisor exceeds half the U32 range, so the quotient is 0 or 1 and
/ the remainder a or a-b: test the divisor's sign and take that arm instead.  The
/ ordinary divide is the FALL-THROUGH-free arm -- JR PL jumps straight to it -- so the
/ common path pays the TESTL and the taken jump and no more.
%	PEFFECT|PRVALUE
	FU32		RQ0	RR2	*	RR2
		TREG		LONG
		ADR		LONG
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
		[DLAB1]:
%	PEFFECT|PRVALUE
	FS32		RQ0	RR2	*	RR2
		TREG		LONG
		ADR|IMM		LONG
			[ZEXTSL]	[REGNO RR0]
			[ZDIVL]		[REGNO RQ0],[AR]
