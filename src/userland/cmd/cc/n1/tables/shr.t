/ shr.t - Z8001 '>>' (SHR).  The Z8000 has no distinct right-shift opcode: a right
/ shift is the SAME SHIFT instruction (SLL/SLA or dynamic SDL/SDA) with a NEGATIVE
/ count, and the decoder infers SRL (logical) / SRA (arithmetic) from the sign.
/   - Constant count: modtree already stored it negated, so emit the static
/     SLL/SLA with the (negative) immediate.  signed FS16 -> ZSLA (=>SRA);
/     unsigned UWORD -> ZSLL (=>SRL).
/   - Variable count: force the count into a register, NEG it at run time (an
/     unavoidable Z8000 cost -- right = negative count), then the dynamic shift:
/     signed -> ZSDA (=>SRA); unsigned -> ZSDL (=>SRL).

SHR:
%	PEFFECT|PRVALUE|PSREL|P_SLT
	FS16	ANYR	ANYR	*	TEMP
		TREG	FS16
		IMM|MMX	WORD
		[ZSLA]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	UWORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		IMM|MMX	WORD
		[ZSLL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	FS16	ANYR	ANYR	*	TEMP
		TREG	FS16
		TREG	WORD
		[ZNEG]	[AR]
		[ZSDA]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	UWORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		TREG	WORD
		[ZNEG]	[AR]
		[ZSDL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ 32-bit right shift: negative count; signed FS32 -> SLAL/SDAL (SRA), unsigned FU32 ->
/ SLLL/SDLL (SRL).  Constant count was negated by modtree; variable count is NEG'd.
%	PEFFECT|PRVALUE|PSREL|P_SLT
	FS32	ANYR	ANYR	*	TEMP
		TREG	FS32
		IMM|MMX	WORD
		[ZSLAL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	FU32	ANYR	ANYR	*	TEMP
		TREG	FU32
		IMM|MMX	WORD
		[ZSLLL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	FS32	ANYR	ANYR	*	TEMP
		TREG	FS32
		TREG	WORD
		[ZNEG]	[AR]
		[ZSDAL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PSREL|P_SLT
	FU32	ANYR	ANYR	*	TEMP
		TREG	FU32
		TREG	WORD
		[ZNEG]	[AR]
		[ZSDLL]	[R],[AR]
	[IFR]	[REL0]	[LAB]
