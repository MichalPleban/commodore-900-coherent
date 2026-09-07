/ relop.t - Z8001 relational operators (test context only; the parser turns all
/ value contexts into ?: ). Each rule compares, then [REL0] emits the per-condition
/ relative jump ZJREL|cc (table1.c optab relation rows) to [LAB]. Adapted from the
/ i8086 relop.t: ZCMP->ZCP, ZCMPB->ZCPB. Core word/byte/long; pointer + the more
/ specialized half-constant forms still TODO (donor cc-i8086 relop.t is 340 lines).

EQ:
NE:
GT:
GE:
LT:
LE:
ULT:
ULE:
UGT:
UGE:

/////////
/ Word compare: a MEMORY word against a constant, in place -- `CP mem,#k'.  The Z8000
/ compare has a memory destination (DA 0x4D01 = 8 bytes segmented, X 0x4D01+index = 6),
/ against `LD R0,mem ; CP R0,#k' = 10 and 8: the in-place form is SHORTER in both
/ addressing modes, matching the original MWC backend (`CP 0x03:0x35A0,#0x0002') and the
/ i8086 donor's ADR-left rule.  Stacked above the register-left rule so both share one
/ code body.  When the value is already live in a register, the n2 peephole (cpstate's
/ cmpmem) rewrites the operand back to that register, so a pre-loaded word costs nothing.
/////////
%	PREL
	*		NONE	*	*	NONE
		ADR|MMX		WORD
		IMM|MMX		WORD
%	PREL
	*		NONE	*	*	NONE
		RREG|LREG|MMX	WORD
		ADR|IMM		WORD
			[ZCP]	[AL],[AR]
			[REL0]	[LAB]

%	PREL|P_SLT
	*		ANYR	ANYR	*	NONE
		TREG		WORD
		ADR|IMM		WORD
			[ZCP]	[R],[AR]
			[REL0]	[LAB]

/////////
/ Byte compare (avoids widening to word).
/////////
/ A MEMORY byte compared directly against a constant: CPB mem,#k -- do NOT load it into a
/ register first (`LDB RL0,mem ; CPB RL0,#k' is a word longer).  Matches the original MWC
/ backend (`CPB @R4,#0x2D' / `CPB off(R13),#k') and the i8086 ADR-byte rule.  CPB is a real
/ subtract-compare (sets V and C), so this is valid for ORDERING too, not only equality.
/ Covers a frame/global byte and a far deref @RRn alike (`*p == c', `b[i] < c').  Must
/ precede the TREG rule below so the in-place form is chosen over a forced register load.
/ [LO AL] dials the operand's low half: T_ADR covers a REGISTER as well as memory, and a
/ byte value in a register lives in RLn (the tables put byte temps in R0..R7 and address
/ them as [LO R]), so the byte-register form must be selected explicitly.  On a byte
/ MEMORY operand the selector is a no-op (basebias is 0 for S8/U8), so the `CPB mem,#k'
/ form is unchanged -- same instruction, same length.
%	PREL
	*		NONE	*	*	NONE
		ADR		BYTE
		IMM|MMX		BYTE
			[ZCPB]	[LO AL],[AR]
			[REL0]	[LAB]

%	PREL|P_SLT
	*		ANYR	ANYR	*	NONE
		TREG		BYTE
		ADR|IMM		BYTE
			[ZCPB]	[LO R],[LO AR]
			[REL0]	[LAB]

/////////
/ Long compare (register pair).
/////////
/ Against 0, MEMORY-direct: a long/far-ptr already in memory (`*p != 0', `l != 0', `G != 0')
/ is tested IN PLACE -- the Z8000 TESTL has a memory form (@RRn 0x1C08, DA/X 0x5C08; n2 encodes
/ all three), so one TESTL replaces LDL-into-a-temp-then-TESTL.  Must precede the TREG rule
/ below; a register/computed value (not T_ADR) falls through to it.  Matches the original (it
/ tests the deref in place); the implicit `if(*p)' already does this via leaves.t.
%	PEREL
	*		NONE	*	*	NONE
		ADR		LONG
		0|MMX		*
			[ZTESTL]	[AL]
			[REL0]	[LAB]
/ Against 0: TESTL the pair -- one word, the dedicated zero test, NOT CPL against a
/ 32-bit zero immediate (3 words).  EQUALITY (l==0 / l!=0) uses only Z, which TESTL sets.
%	PEREL
	*		NONE	ANYR	*	NONE
		TREG		LONG
		0|MMX		*
			[ZTESTL]	[AL]
			[REL0]	[LAB]
/ ORDERING (l>0 / l<0 / ...): TESTL omits V and C, which the signed/unsigned condition
/ codes need -- clear them (RESFLG V,C = mask 9) so they read 0, as a CPL against 0 sets.
%	PNEREL
	*		NONE	ANYR	*	NONE
		TREG		LONG
		0|MMX		*
			[ZTESTL]	[AL]
			[ZRESFLG]	[CONST 9]
			[REL0]	[LAB]
%	PREL
	*		NONE	ANYR	*	NONE
		TREG		LONG
		ADR|IMM		LONG
			[ZCPL]	[AL],[AR]
			[REL0]	[LAB]

/////////
/ Far pointer compare: a seg:offset pair is a 32-bit value.
/ Null check `p == 0' / `p != 0' (e.g. `*++argv' in a && context): TESTL the pair --
/ one word, no immediate to load, the dedicated zero test (NOT CPL against a 32-bit 0).
/ EQUALITY uses only Z (which TESTL sets); ORDERING (p>0 / p<0, rare) needs V/C which
/ TESTL omits, so clear them (RESFLG V,C = mask 9), as a CPL against 0 would set them.
/////////
/ Null check MEMORY-direct: a far pointer already in memory (`*p == 0' / `*p != 0',
/ `argv[i] == 0') is TESTL'd in place (one op) instead of LDL-into-a-temp-then-TESTL.
/ Must precede the TREG rule; a register/computed pointer falls through to it.
%	PEREL
	*		NONE	*	*	NONE
		ADR		LPTX
		0|MMX		*
			[ZTESTL]	[AL]
			[REL0]	[LAB]
%	PEREL
	*		NONE	ANYR	*	NONE
		TREG		LPTX
		0|MMX		*
			[ZTESTL]	[AL]
			[REL0]	[LAB]
%	PNEREL
	*		NONE	ANYR	*	NONE
		TREG		LPTX
		0|MMX		*
			[ZTESTL]	[AL]
			[ZRESFLG]	[CONST 9]
			[REL0]	[LAB]
/ Compare against another pointer / non-zero immediate: CPL (pointer is null iff both
/ halves 0, so this also orders the seg:offset pair as a 32-bit value).
%	PREL
	*		NONE	ANYR	*	NONE
		TREG		LPTX
		ADR|IMM		LONG|LPTX|WORD
			[ZCPL]	[AL],[AR]
			[REL0]	[LAB]
