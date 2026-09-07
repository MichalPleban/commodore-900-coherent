/ assign.t - Z8001 scalar assignment '=' (ASSIGN:). Picks the most efficient
/ Z8000 tool for each shape:
/   dst = 0          -> CLR        (shortest zero, reg or memory)
/   dst(reg) = src   -> LD reg,src (load; src may be mem/imm/reg)
/   dst(mem) = reg   -> LD mem,reg (store)
/   dst(mem) = #imm  -> LD mem,#imm (store immediate)
/   dst(mem) = mem   -> force src into a TEMP (P_SRT), LD mem,R  (the Z8000 has
/                       no scalar mem<->mem move; aggregate moves use LDIR/LDDR,
/                       handled by BLKMOVE, not here).
/ The TEMP also carries the value for an rvalue context ([IFV]) and is tested
/ for a relational context ([IFR]). Adapted from i8086 assign.t (ZMOV->ZLD).

ASSIGN:

/////////
/ dst = 0 : CLR / CLRB is the shortest zero on the Z8000.
/////////
%	PEFFECT
	WORD		NONE	*	*	NONE
		ADR|LV		WORD
		0|MMX		*
			[ZCLR]	[AL]
%	PEFFECT
	BYTE		NONE	*	*	NONE
		ADR|LV		BYTE
		0|MMX		*
			[ZCLRB]	[AL]
/ long / far pointer = 0 : CLR both halves (a null pointer is seg 0 : offset 0).  The
/ null constant is a WORD 0 ICON, so the LONG|LPTX immediate store below won't match it.
/ For a far (@RRn) lvalue the HIGH half is @RR+2, which CLR cannot encode -- n2 emits
/ the equivalent `LD @RR+2,#0' there (store-imm DOES have the @RRn+disp form), so this
/ stays optimal (two in-place clears) for BOTH near and far.
%	PEFFECT
	LONG|LPTX	NONE	*	*	NONE
		ADR|LV		LONG|LPTX
		0|MMX		*
			[ZCLR]	[HI AL]
			[ZCLR]	[LO AL]

/////////
/ Direct: at least one operand is a register or immediate, so a single LD does
/ it (no temp). dst(reg)<-src(easy) | dst(mem)<-reg | dst(mem)<-imm.
/////////
%	PEFFECT
	WORD		NONE	*	*	NONE
		REG|MMX		WORD
		EASY|MMX	WORD
%	PEFFECT
	WORD		NONE	*	*	NONE
		ADR|LV		WORD
		REG|MMX		WORD
%	PEFFECT
	WORD		NONE	*	*	NONE
		ADR|LV		WORD
		IMM|MMX		WORD
			[ZLD]	[AL],[AR]

%	PEFFECT
	BYTE		NONE	*	*	NONE
		REG|MMX		BYTE
		EASY|MMX	BYTE
%	PEFFECT
	BYTE		NONE	*	*	NONE
		ADR|LV		BYTE
		REG|MMX		BYTE
%	PEFFECT
	BYTE		NONE	*	*	NONE
		ADR|LV		BYTE
		IMM|MMX		BYTE
			[ZLDB]	[AL],[AR]

/ char = 0: the same int-const promotion makes the ASSIGN WORD-typed, so the byte-typed
/ CLRB rule above does NOT match -- but storing 0 to a byte is CLRB mem (1 word), shorter
/ than `LDB mem,#0' (the byte-immediate store below, 1 word longer).  Must precede the
/ IMM byte-store rule so the zero case wins; matches the original (`CLRB off(R13)').
%	PEFFECT
	WORD		NONE	*	*	NONE
		ADR|LV		BYTE
		0|MMX		WORD
			[ZCLRB]	[AL]

/ char = <int const>: C promotes the constant so the ASSIGN is WORD-typed and the
/ immediate arrives as a WORD, but the lvalue is a byte.  In EFFECT context (value
/ discarded) store the constant straight to the byte memory -- ZLDB addr,#imm (the
/ Z8000 byte-immediate-to-memory; n2 takes the low byte).  Without this the source
/ is forced into a TEMP (LDK Rn,#k ; LDB addr,RLn -- 2 insns for 1).  RVALUE/EREL
/ contexts still need the value, so they fall through to the TEMP rules below.
%	PEFFECT
	WORD		NONE	*	*	NONE
		ADR|LV		BYTE
		IMM|MMX		WORD
			[ZLDB]	[AL],[AR]

/////////
/ Long / segmented pointer: move both halves (one operand reg/imm).
/////////
/ Direct LOAD into a register PAIR: `RRd = <reg|mem|imm>' is a single native LDL
/ (the Z8000 moves the whole 32-bit pair in one instruction).  Without this a
/ far-pointer register variable's load -- e.g. copying a far-ptr PARAM from its
/ frame slot into its callee-saved pair -- went through a temp (LDL RR0,mem ; move
/ RR0->RRd), wasting two instructions on every such load.  REG-destination, so it
/ doesn't overlap the ADR|LV (memory-destination) stores below.
%	PEFFECT
	LONG|LPTX|FLT	NONE	*	*	NONE
		REG|MMX		LONG|LPTX|FLT
		ADR|IMM		LONG|LPTX|FLT
			[ZLDL]	[AL],[AR]
/ Direct STORE of a register pair to memory: `mem = RRsrc' is a single native LDL store (the
/ Z8000 moves the whole 32-bit pair in one instruction).  Without this, storing a far-ptr
/ VALUE (e.g. a function result, or a far pointer held in a pair) to a frame slot or global
/ went through the general mem<-mem path -> two `LD' halves; the original uses one `LDL mem,RR'.
/ REG source + memory dest, so it overlaps neither the REG-dest load above nor the IMM store
/ below; REG|MMX forces an exact register match so a memory source falls through.
%	PEFFECT
	LONG|LPTX|FLT	NONE	*	*	NONE
		ADR|LV		LONG|LPTX|FLT
		REG|MMX		LONG|LPTX|FLT
			[ZLDL]	[AL],[AR]
%	PEFFECT
	LONG|LPTX|FLT	NONE	*	*	NONE
		ADR|LV		LONG|LPTX|FLT
		IMM|MMX		LONG|LPTX|FLT
			[ZLD]	[LO AL],[LO AR]
			[ZLD]	[HI AL],[HI AR]

/////////
/ Double (F64) store: a quad value in the pinned RQ0 (a soft-float result) stored to
/ memory as two LDLs -- high pair to offset 0, low pair to offset 4 (big-endian).
/ Source pinned to RQ0 (the KD allocator can't allocate a free quad); RR0/RR2 = RQ0.
/////////
%	PEFFECT|PRVALUE
	DBL		RQ0	*	*	RQ0
		ADR|LV		DBL
		TREG		DBL
			[ZLDL]	[HI AL],[REGNO RR0]
			[ZLDL]	[LO AL],[REGNO RR2]

/////////
/ char assignment. C promotes char->int, so the ASSIGN node is WORD-typed and
/ the SOURCE arrives as a WORD value (already sign/zero-extended); the lvalue is
/ a byte. Force the source into a WORD temp and store its low byte (ZLDB [LO R]).
/ The VALUE of the expression is the STORED char widened to int, NOT the
/ untruncated word (`(c = 200)' is -56 for a signed char), so a value consumer
/ ([IFV]) re-derives it from the low byte: signed sign-extends, unsigned clears
/ the high byte.  Test the low byte for a relational.  Covers byte mem->mem
/ (src forced into the temp by P_SRT).
/////////
%	PEFFECT|PRVALUE|PEREL|P_SRT
	WORD		ANYR	*	ANYR	TEMP
		ADR|LV		FS8
		TREG		WORD
%	PLVALUE|P_SRT
	WORD		ANYL	*	ANYL	TEMP
		ADR|LV		FS8
		TREG		WORD
			[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
		[IFR]	[ZORB]	[LO R],[LO R]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SRT
	WORD		ANYR	*	ANYR	TEMP
		ADR|LV		FU8
		TREG		WORD
%	PLVALUE|P_SRT
	WORD		ANYL	*	ANYL	TEMP
		ADR|LV		FU8
		TREG		WORD
			[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
		[IFR]	[ZORB]	[LO R],[LO R]
		[IFR]	[REL0]	[LAB]

/ char = char (byte source, no promotion): force the byte source into a byte
/ TEMP and store it. (char-to-char mem->mem.)  The node is WORD-typed (the C
/ promotion) but the temp holds only the byte over a stale high half, so a
/ VALUE consumer -- `tab[t = tp->t_type]' indexes with the whole word -- needs
/ [IFV] to widen the temp first: signed sign-extends, unsigned clears the high
/ byte (cf. the char AADD in aadd.t).
%	PEFFECT|PRVALUE|PEREL|P_SRT
	WORD		ANYR	*	ANYR	TEMP
		ADR|LV		FS8
		TREG		BYTE
%	PLVALUE|P_SRT
	WORD		ANYL	*	ANYL	TEMP
		ADR|LV		FS8
		TREG		BYTE
			[ZLDB]	[AL],[LO R]
	[IFV]	[ZEXTSB]	[R]
		[IFR]	[ZORB]	[LO R],[LO R]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SRT
	WORD		ANYR	*	ANYR	TEMP
		ADR|LV		FU8
		TREG		BYTE
%	PLVALUE|P_SRT
	WORD		ANYL	*	ANYL	TEMP
		ADR|LV		FU8
		TREG		BYTE
			[ZLDB]	[AL],[LO R]
	[IFV]	[ZCLRB]	[HI R]
		[IFR]	[ZORB]	[LO R],[LO R]
		[IFR]	[REL0]	[LAB]

/////////
/ General word case (mem = mem): force the source into a TEMP register, then
/ store it. The TEMP carries the rvalue and is tested in a relational context.
/////////
%	PEFFECT|PRVALUE|PEREL|P_SRT
	WORD		ANYR	*	ANYR	TEMP
		ADR|LV		WORD
		TREG		WORD
%	PLVALUE|P_SRT
	WORD		ANYL	*	ANYL	TEMP
		ADR|LV		WORD
		TREG		WORD
			[ZLD]	[AL],[R]
		[IFR]	[ZOR]	[R],[R]
		[IFR]	[REL0]	[LAB]

%	PEFFECT|PRVALUE|PEREL|P_SRT
	BYTE		ANYR	*	ANYR	TEMP
		ADR|LV		BYTE
		TREG		BYTE
%	PLVALUE|P_SRT
	BYTE		ANYL	*	ANYL	TEMP
		ADR|LV		BYTE
		TREG		BYTE
			[ZLDB]	[AL],[LO R]
		[IFR]	[ZORB]	[LO R],[LO R]
		[IFR]	[REL0]	[LAB]

/////////
/ General long / pointer (mem = mem): force into a TEMP pair, store both halves.
/////////
%	PEFFECT|PRVALUE|P_SRT
	LONG|LPTX|FLT	ANYR	*	ANYR	TEMP
		ADR|LV		LONG|LPTX|FLT
		TREG		LONG|LPTX|FLT
%	PLVALUE|P_SRT
	LONG|LPTX|FLT	ANYL	*	ANYL	TEMP
		ADR|LV		LONG|LPTX|FLT
		TREG		LONG|LPTX|FLT
			[ZLDL]	[AL],[R]

/////////
/ Bit-field store (FIELD lvalue). The Z8000 has no memory-destination logical op,
/ so it is load / clear (AND ~mask) / insert (OR value) / store. modlfld has already
/ preshifted and masked the source into [AR], exactly like the right side of |=.
/ CMASK is ~fieldmask (clear), EMASK is fieldmask (extract) from out.c.
/////////
%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD16
		ADR|IMM		WORD
			[ZLD]	[R],[AL]
			[ZAND]	[R],[LO CMASK]
			[ZOR]	[R],[AR]
			[ZLD]	[AL],[R]
		[IFV]	[ZAND]	[R],[LO EMASK]

%	PEFFECT|PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|LV		FFLD8
		ADR|IMM		WORD
			[ZLDB]	[LO R],[AL]
			[ZANDB]	[LO R],[LO CMASK]
			[ZORB]	[LO R],[LO AR]
			[ZLDB]	[AL],[LO R]
		[IFV]	[ZANDB]	[LO R],[LO EMASK]
