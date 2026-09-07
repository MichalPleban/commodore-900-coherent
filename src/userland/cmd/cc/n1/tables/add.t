/ add.t - Z8001 instruction-selection rules for binary '+'  (operator ADD:).
/ Grammar is the
/ MWC .t format (10-field % header + emit body); opcodes are the verified Z*
/ names from ../../../generated/opcode.h. Field order per header:
/   %  <p_flag>
/      <p_ntype> <p_ntemp> <p_ltemp> <p_rtemp> <p_result>
/          <p_lflag> <p_ltype>            (LEFT subtree)
/          <p_rflag> <p_rtype>            (RIGHT subtree)
/
/ Z8001 vs i8086 notes: (1) the 0x80 reg-form bit etc. is handled in n2 -- here we
/ just name the Z* opcode + operands. (2) Z8000 has a NATIVE 32-bit ADDL, so the
/ long add is ONE instruction, not the i8086 ADD-low/ADC-high pair. (3) Z8000 INC
/ takes a 1..16 immediate count, so '+1'/'+2' fold to INC Rd,#n.

ADD:

/ ---- x + 1  ->  INC Rd,#1  (RVALUE or LVALUE target) ----
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		1|MMX	*
%	PLVALUE|P_SLT
	WORD	ANYL	ANYL	*	TEMP
		TREG	WORD
		1|MMX	*
		[ZINC]	[R],[CONST 1]
	[IFR]	[REL0]	[LAB]

/ ---- x + 2  ->  INC Rd,#2 ----
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		2|MMX	*
		[ZINC]	[R],[CONST 2]
	[IFR]	[REL0]	[LAB]

/ ---- general 16-bit add:  ADD Rd, <reg|imm|@Rs|addr|addr(Rs)> ----
%	PEFFECT|PRVALUE|PSREL|P_SLT
	WORD	ANYR	ANYR	*	TEMP
		TREG	WORD
		ADR|IMM	WORD
		[ZADD]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ ---- byte add:  ADDB RHd/RLd, <...> ----
%	PEFFECT|PRVALUE|PSREL|P_SLT
	BYTE	ANYR	ANYR	*	TEMP
		TREG	BYTE
		ADR|IMM	BYTE
		[ZADDB]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ ---- 32-bit add:  ADDL RRd, <...>  (single Z8000 instruction).  ANYR/TEMP lets the LONG
/ type drive the allocator to a register pair, as the LPTX pointer rules below do. ----
/ NODE and LEFT types admit LPTX as well as LONG, and there is an LVALUE arm.
/ `*(char *)(<long constant> + (long)i)' is a 32-bit sum used as an ADDRESS.  Building
/ the address tree gives the sum's base the segmented-pointer type (setofstype, and
/ the node type follows from fixaddtype), while the sum itself keeps the C type of
/ `long' -- so the ADD arrives with LPTX and LONG mixed across the node and its left
/ operand in whichever way that expression happened to fold.  Both are the same
/ two-word seg:offset pair in a register, and ADDL is the right instruction for every
/ combination; with LONG-only types NO rule matched such a sum, select() fell through
/ to seltree(MRVALUE) + selfix, and selfix's FIXUP has no lvalue rule either, so the
/ two would re-enter each other until the stack ran out.
/ The PLVALUE arm builds the sum straight into the pair the deref will use as a base
/ instead of routing it through an auto temp (one instruction fewer).  ANYL keeps it
/ out of RR0, which the Z8000 may not use as a base register.
%	PEFFECT|PRVALUE|PSREL|P_SLT
	LONG|LPTX	ANYR	ANYR	*	TEMP
		TREG	LONG|LPTX
		ADR|IMM	LONG
%	PLVALUE|P_SLT
	LONG|LPTX	ANYL	ANYL	*	TEMP
		TREG	LONG|LPTX
		ADR|IMM	LONG
		[ZADDL]	[R],[AR]
	[IFR]	[REL0]	[LAB]

/ ---- pointer + int  (segmented far pointer LPTX + scaled WORD offset) ----
/ A far pointer is seg:offset in a register PAIR (HI = segment, LO = 16-bit offset),
/ and the front end has already scaled the index by the element size.  Adding the
/ WORD to the OFFSET half alone WRAPS inside the segment, which is what a segmented
/ pointer does: a segment is one object's world, no object crosses a boundary, and an
/ offset that carries must wrap rather than name the next segment.  So this is the
/ form for an address and for a value alike, and it is ONE instruction -- `LDL pair ;
/ ADD off' is the 1985 compiler's own shape for `bp = &((char *)ap)[size]'.
/ modswap canonicalizes the sum to this order (pointer LEFT, index RIGHT) whichever
/ way cc0 spelled it, so `p[i]', `p+i' and `p-k' all arrive here.  The two pointer
/ forms it leaves on the right have their own rules below: a static base, which is
/ indexed in X-mode, and the frame register, which is a near base.
%	PEFFECT|PRVALUE|P_SLT
	LPTX	ANYR	ANYR	*	TEMP
		TREG	LPTX
		ADR|IMM	WORD
%	PLVALUE|P_SLT
	LPTX	ANYL	ANYL	*	TEMP
		TREG	LPTX
		ADR|IMM	WORD
		[ZADD]	[LO R],[AR]

/ Index form `int + FRAME-base' (`q[i]' where q is a LOCAL array).  cc0 folds q's frame
/ displacement into the index and leaves `int + FP', where the pointer operand is the bare
/ frame register R13 -- flag T_SREG (set in amd.c when a REG's reg is FP/SP), NOT a loaded
/ far pointer.  The element address is FP + int, a near offset in the STACK segment.
/ Materialize it as a far PAIR: int -> result pair LOW half (ltemp=LOTEMP), ADD the frame
/ reg [AR] (= R13) to it, then set the segment half from R14, the stack-segment register.
/ (R14 holds 0 in the flat userland model and the stack segment under VKERN, so this
/ is correct in both; a hardcoded 0 segment half is only correct in the flat model.)
/ `SREG|MMX' matches the frame register SPECIFICALLY and must precede
/ the generic `REG' form below (T_REG includes T_SREG, so REG would otherwise grab it and
/ emit a bogus [LO AR]/[HI AR] pair split on a single near register).  Any residual q offset
/ rides in the @RRn deref displacement.
%	PEFFECT|PRVALUE
	LPTX	PAIR	LOTEMP	*	TEMP
		TREG	WORD
		SREG|MMX	LPTX
%	PLVALUE
	LPTX	ANYL	LOTEMP	*	TEMP
		TREG	WORD
		SREG|MMX	LPTX
		[ZADD]	[LO R],[AR]
		[ZLD]	[HI R],[REGNO R14]

/ Index form `int + ptr', for a far pointer that modswap did NOT move left -- a sum of
/ two far pointers, and whatever the generic swap rule leaves in this order.  int ->
/ result pair LOW half (ltemp=LOTEMP), ADD the pointer's offset half to it and take the
/ segment half across, so the sum stays INSIDE the segment exactly as the pointer-LEFT
/ rule above makes it.  Both forms of the same address must be the same 32-bit VALUE:
/ zeroing the segment half and ADDLing the whole pointer in addresses the same bytes
/ (the carry lands in the ignored low byte of the segment word) but compares unequal,
/ and malloc's arena walk is built on those comparisons -- `mp = mblockp(cp)' (p - 2,
/ which carries for every offset >= 2) then `nmp == __a_scanp' in realloc,
/ `__a_top == mp' in newarena.
/ NOTE: a pool-mediated GLOBAL far pointer must never reach this rule un-loaded --
/ [AR] on its memory operand reads its ADDRESS-POOL slot
/ (&p + i instead of p + i; glob2's `dirn + dirp' smashed the dirn pointer cell this
/ way).  modswap forces far-ptr GID variables LEFT so the TREG-LPTX rule above
/ loads them first.
%	PEFFECT|PRVALUE
	LPTX	PAIR	LOTEMP	*	TEMP
		TREG	WORD
		REG	LPTX
%	PLVALUE
	LPTX	ANYL	LOTEMP	*	TEMP
		TREG	WORD
		REG	LPTX
		[ZADD]	[LO R],[LO AR]
		[ZLD]	[HI R],[HI AR]
