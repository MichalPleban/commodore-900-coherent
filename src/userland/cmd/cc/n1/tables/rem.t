/ rem.t - Z8001 '%' (REM:). Same DIV as div.t but the result is the REMAINDER =
/ the high word (R0). The dividend goes in R1 (low word) -- a different register
/ than the result R0 -- so it is loaded there explicitly.
REM:
/ UNSIGNED remainder: zero-extend dividend. As in div.t the one-word DIV serves the
/ divisor a constant can be (bit 15 clear).
%	PEFFECT|PRVALUE
	UWORD		RR0	R0	*	R0
		ADR|IMM		WORD
		IMM|MMX		WORD
			[ZLD]	[REGNO R1],[AL]
			[ZCLR]	[REGNO R0]
			[ZDIV]	[REGNO RR0],[AR]
/ Any other unsigned divisor arrives widened to 32 bits (modoper) for DIVL, which
/ leaves the remainder in the high pair RR0; the remainder is below the 16-bit
/ divisor, so its low word R1 is the 16-bit result. The dividend goes in R3, the low
/ word of the RQ0 dividend, with the rest zeroed.
%	PEFFECT|PRVALUE
	UWORD		RQ0	R1	*	R1
		ADR|IMM		WORD
		ADR|IMM		LONG
			[ZLD]		[REGNO R3],[AL]
			[ZSUBL]		[REGNO RR0],[REGNO RR0]
			[ZCLR]		[REGNO R2]
			[ZDIVL]		[REGNO RQ0],[AR]
/ SIGNED remainder: sign-extend dividend.
%	PEFFECT|PRVALUE
	FS16		RR0	R0	*	R0
		ADR|IMM		WORD
		ADR|IMM		WORD
			[ZLD]	[REGNO R1],[AL]
			[ZEXTS]	[REGNO RR0]
			[ZDIV]	[REGNO RR0],[AR]

/ 32-bit remainder: DIVL leaves the remainder in the high pair RR0.  The dividend goes
/ in RR2 (loaded explicitly, since the result RR0 differs); widen RQ0 then DIVL.
/ UNSIGNED by a CONSTANT: exact, as in div.t.
%	PEFFECT|PRVALUE
	FU32		RQ0	RR0	*	RR0
		ADR|IMM		LONG
		IMM|MMX		LONG
			[ZLDL]		[REGNO RR2],[AL]
			[ZSUBL]		[REGNO RR0],[REGNO RR0]
			[ZDIVL]		[REGNO RQ0],[AR]
/ UNSIGNED by a RUNTIME divisor: DIVL is signed, so a divisor with bit 31 set reads as
/ negative.  Such a divisor is over half the U32 range, so the remainder is the dividend
/ itself when it is the smaller and dividend-minus-divisor otherwise; the quotient DIVL
/ would leave in RR2 is not this rule's result, so that arm does not compute it.
%	PEFFECT|PRVALUE
	FU32		RQ0	RR0	*	RR0
		ADR|IMM		LONG
		ADR		LONG
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
		[DLAB1]:
%	PEFFECT|PRVALUE
	FS32		RQ0	RR0	*	RR0
		ADR|IMM		LONG
		ADR|IMM		LONG
			[ZLDL]		[REGNO RR2],[AL]
			[ZEXTSL]	[REGNO RR0]
			[ZDIVL]		[REGNO RQ0],[AR]
