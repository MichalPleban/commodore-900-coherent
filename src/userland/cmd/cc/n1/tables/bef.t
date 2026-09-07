/ bef.t - Z8001 prefix increment/decrement (++x / --x). The value of the
/ expression is the NEW (incremented) value, so increment FIRST, then fetch.
/ [OP1] resolves to ZINC (INCBEF) or ZDEC (DECBEF) from table1.c optab; the
/ Z8000 INC/DEC take a count operand (1..16) and work directly on a register
/ or memory operand (no load/store needed). Adapted from i8086 bef.t.

INCBEF:
DECBEF:
%	PEFFECT|PRVALUE|PSREL
	WORD		ANYR	*	*	TEMP
		ADR|LV		WORD
		1|MMX		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR|LV		WORD
		1|MMX		*
			[OP1]	[AL],[CONST 1]
		[IFV]	[ZLD]	[R],[AL]
		[IFR]	[REL0]	[LAB]

/ char lvalue (word-typed node over a byte): INCB/DECB the byte memory, then (value
/ context) fetch the new byte.  [TL OP1] tags [OP1] (ZINC/ZDEC) to its byte variant
/ ZINCB/ZDECB by the byte lvalue type (optab col 2 now holds the long inverse op).
/ The node is WORD (char promoted to int), but LDB fills only the low byte, so when
/ the value is used ([IFV]) extend it through the high byte -- signed sign-extends
/ (EXTSB), unsigned clears it -- else a word consumer (`if(--c < 0)') reads whatever
/ the register last held: `--c' from 0 leaves 0x00FF and tests as +255.  Split by
/ signedness exactly as the postfix rules in aft.t.  Effect-only `++c;' emits nothing
/ extra.
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FS8
		1|MMX		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR|LV		FS8
		1|MMX		*
			[TL OP1]	[AL],[CONST 1]
		[IFV]	[ZLDB]	[LO R],[AL]
		[IFV]	[ZEXTSB]	[R]

%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FU8
		1|MMX		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR|LV		FU8
		1|MMX		*
			[TL OP1]	[AL],[CONST 1]
		[IFV]	[ZLDB]	[LO R],[AL]
		[IFV]	[ZCLRB]	[HI R]

/ far pointer in a REGISTER lvalue (e.g. `*++p'): INC/DEC the OFFSET word (low half) by
/ the element size (AR), segment unchanged, then UNCONDITIONALLY load the NEW pointer pair
/ into R so it is a materialized rvalue the consumer (deref / compare / ...) can take.  The
/ rvalue and the effect/lvalue forms need different emits (cf. i8086 bef.t), so they
/ are separate rules, not one shared block.
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		REG|MMX		LPTX
		IMM|MMX		WORD
			[OP1]	[LO AL],[AR]
			[ZLDL]	[R],[AL]
/ effect / lvalue: INC/DEC the offset; load the pair only if the value is used.
%	PEFFECT|PLVALUE
	LPTX		ANYL	*	*	TEMP
		REG|MMX		LPTX
		IMM|MMX		WORD
			[OP1]	[LO AL],[AR]
		[IFV]	[ZLDL]	[R],[AL]

/ far pointer in MEMORY: @RRn indirect has no displacement, so the OFFSET word (LOW half,
/ at +2 big-endian) is unreachable in place -- load the pair, INC/DEC its offset half [LO R],
/ store it back.  The prefix value IS the NEW pointer, so R already holds it (no reload).
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		ADR|LV		LPTX
		IMM|MMX		WORD
%	PEFFECT|PLVALUE
	LPTX		ANYL	*	*	TEMP
		ADR|LV		LPTX
		IMM|MMX		WORD
			[ZLDL]	[R],[AL]
			[OP1]	[LO R],[AR]
			[ZLDL]	[AL],[R]
