/ mul.t - Z8001 '*' (MUL:).
/ The Z8000 MULT RRd,Rs multiplies the LOW word of the destination pair RRd by the
/ source Rs and leaves the 32-bit product in RRd; the C `int` result is that low
/ word. We use the fixed pair RR0 (= R0:R1): the multiplicand + result live in R1
/ (the low half) and R0 (the high half) is clobbered -- exactly the i8086 DXAX/AX
/ shape, since the Z8000 MULT (like x86 MUL) ties the result to a register pair.
/ [A flexible pair allocation would avoid always clobbering RR0.]
MUL:
%	PEFFECT|PRVALUE
	WORD		RR0	R1	*	R1
		TREG		WORD
		ADR|IMM		WORD
			[ZMULT]	[REGNO RR0],[AR]

/ 32-bit multiply: MULTL RQ0,Rs gives the 64-bit product in RQ0; the long result is
/ its low pair RR2.  Signed and unsigned agree in the low 32 bits, so one rule.
%	PEFFECT|PRVALUE
	LONG		RQ0	RR2	*	RR2
		TREG		LONG
		ADR|IMM		LONG
			[ZMULTL]	[REGNO RQ0],[AR]
