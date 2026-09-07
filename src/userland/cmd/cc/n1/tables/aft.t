/ aft.t - Z8001 postfix increment/decrement (x++ / x--). The value of the
/ expression is the OLD value, so FETCH first, then increment. Same right-hand
/ handling as bef.t. [OP1] = ZINC (INCAFT) / ZDEC (DECAFT). Adapted from i8086.

INCAFT:
DECAFT:
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR		WORD
		1|MMX		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR		WORD
		1|MMX		*
		[IFV]	[ZLD]	[R],[AL]
			[OP1]	[AL],[CONST 1]

/ char lvalue: fetch the OLD byte (value context) first, then INCB/DECB the memory.
/ [TL OP1] tags ZINC/ZDEC to ZINCB/ZDECB by the byte type (col 2 is the long inverse).
/ The result is a WORD (char promoted to int), but LDB fills only the low byte, so
/ when the value is used ([IFV]) extend it through the high byte -- signed sign-
/ extends (EXTSB), unsigned clears the high byte -- else a word consumer (e.g.
/ `if(c++)') reads whatever the register last held.
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR		FS8
		1|MMX		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR		FS8
		1|MMX		*
		[IFV]	[ZLDB]	[LO R],[AL]
		[IFV]	[ZEXTSB]	[R]
			[TL OP1]	[AL],[CONST 1]

%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR		FU8
		1|MMX		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR		FU8
		1|MMX		*
		[IFV]	[ZLDB]	[LO R],[AL]
		[IFV]	[ZCLRB]	[HI R]
			[TL OP1]	[AL],[CONST 1]

/ far pointer in a REGISTER lvalue: the register IS the storage, so fetch the OLD pair
/ into R (the value), then INC/DEC the register's offset half [LO AL] in place.
%	PEFFECT|PRVALUE
	LPTX		ANYR	*	*	TEMP
		REG|MMX		LPTX
		IMM|MMX		WORD
%	PLVALUE
	LPTX		ANYL	*	*	TEMP
		REG|MMX		LPTX
		IMM|MMX		WORD
		[IFV]	[ZLDL]	[R],[AL]
			[OP1]	[LO AL],[AR]

/ far pointer in MEMORY: @RRn indirect has no displacement, so the OFFSET word (the LOW
/ half, at +2 big-endian) is unreachable in place -- load the pair, INC/DEC its offset half
/ [LO R], store it back (as aadd.t does).  A value context needs the OLD pair to survive the
/ store, so save/restore it (PUSHL/POPL) around the bump; [OP1] carries the ++/-- direction.
%	PEFFECT|PRVALUE
	LPTX		ANYR	*	*	TEMP
		ADR|LV		LPTX
		IMM|MMX		WORD
%	PLVALUE
	LPTX		ANYL	*	*	TEMP
		ADR|LV		LPTX
		IMM|MMX		WORD
			[ZLDL]	[R],[AL]
		[IFV]	[ZPUSHL]	[R]
			[OP1]	[LO R],[AR]
			[ZLDL]	[AL],[R]
		[IFV]	[ZPOPL]	[R]

/ 32-bit long postfix whose OLD value is used (y=l-- / while(l--)).  The Z8000 has no
/ memory long-inc and the old value must survive the store, so (matching the original
/ MWC backend): fetch OLD into R, [OP0]=ADDL/SUBL bumps it to the new value, store it,
/ then [OP2] (the inverse long op) undoes the bump so R again holds the OLD value.  The
/ amount is a true 32-bit immediate via [LCONST 1] (a word [CONST 1] would mis-encode
/ the ADDL/SUBL long immediate).  Post-effect (no value) is rewritten onto the long
/ compound-assign by modtree, so it never reaches here.
%	PEFFECT|PRVALUE
	LONG		ANYR	*	*	TEMP
		ADR		LONG
		1|MMX		*
%	PLVALUE
	LONG		ANYL	*	*	TEMP
		ADR		LONG
		1|MMX		*
			[ZLDL]	[R],[AL]
			[OP0]	[R],[LCONST 1]
			[ZLDL]	[AL],[R]
		[IFV]	[OP2]	[R],[LCONST 1]
