/ leaves.t - Z8001 CONVERT / CAST / FIXUP / LEAF.
/ Leaf loads, type conversions, casts, and the universal fixup safety-net.
/ DEEPENED draft (cf. the i8086 ~1075-line original): covers the integer load +
/ widen + narrow core that codegen hits most. Z8000 idioms vs i8086:
/   - zero a register      -> CLR / CLRB           (i8086: SUB r,r)
/   - sign-extend byte->wd -> EXTSB Rd             (i8086: implicit CBW)
/   - sign-extend word->lg -> EXTS  RRd            (i8086: implicit CWD)
/   - zero-extend (unsigned widen) -> CLR/CLRB the high half
/ Opcodes verified in ../../../generated/opcode.h. Float<->double conversions are
/ the inline dfpack/fdpack ZCALL rules below; the full fixup safety-net web is still
/ partial (see end). Pair halves use the n2 [LO R]/[HI R] selector macros.

CONVERT:
CAST:
FIXUP:
LEAF:

/////////
/ Truth test / EQUALITY vs 0: `if(x)' / `while(x)' / `!x' / `x==0' / `x!=0'.  TEST
/ sets Z/S from the operand in ONE instruction (register or memory, no load/temp),
/ then [REL0] branches.  EQUALITY ONLY (PEREL): on the Z8000 the V flag is the shared
/ P/V (parity/overflow) flag -- a LOGICAL op like TEST sets it to PARITY, not overflow.
/ The signed ordering condition codes use S XOR V, so `x>0 / x>=0 / x<0 / x<=0' off a
/ TEST would read parity-garbage (e.g. x=5 -> even parity -> V=1 -> wrong).  Equality
/ uses only Z, which is parity-independent, so it stays on TEST.
/////////
%	PEREL
	WORD		NONE	*	*	NONE
		ADR		WORD
		*		*
			[ZTEST]	[AL]
			[REL0]	[LAB]

/ Truth test of a BYTE in memory (`if(c)' / `while(c)' / `!c' / `c==0', c a char):
/ TESTB the memory byte directly, instead of widening it to a word (LDB + EXTSB +
/ TEST) first.  Must be ZTESTB (not a bare ZTEST, which encodes a WORD test that
/ would read the adjacent byte too -- e.g. mis-detecting a string's NUL).  Equality
/ only -- the ordering caveat for the WORD rule above does not apply (reads Z).
/ The node is WORD when the truth test arrives through the C-semantics int widen
/ (plain `if(c)': the CONVERT wrapper is the matched node) but BYTE when a
/ short-circuit && / || hands the bare char leaf to the flow context directly --
/ both shapes are the same byte-vs-zero test, so match both.
%	PEREL
	WORD|BYTE	NONE	*	*	NONE
		ADR		BYTE
		*		*
			[ZTESTB]	[AL]
			[REL0]	[LAB]

/ SIGNED/UNSIGNED ORDERING vs 0 (x>0 / x>=0 / x<0 / x<=0): must COMPARE against an
/ explicit 0 (CP sets V = real overflow, and C for unsigned), not TEST.  Same single
/ memory operand, so it stays one instruction (CP mem,#0 -- the EA-immediate form).
%	PNEREL
	WORD		NONE	*	*	NONE
		ADR		WORD
		*		*
			[ZCP]	[AL],[CONST 0]
			[REL0]	[LAB]

/ Truth test of a FAR POINTER or 32-bit LONG (if(p)/while(l)/!p/l?:): a seg:offset
/ pair or a long is true iff non-zero.  TESTL has no memory form here, so load the
/ pair then TESTL it (the dedicated zero test).  Matching the ADR leaf directly
/ terminates the selfix<->iselect fixup loop (otherwise FIXUP(LONG|LPTR) in MEQ
/ recurses to a stack overflow -- the same gap for both 32-bit pair types).
/ EQUALITY (==0/!=0/if/while): TESTL sets Z (parity/overflow-independent) -- 1 insn.
%	PEREL
	LONG|LPTX	NONE	*	*	NONE
		ADR		LONG|LPTX
		*		*
			[ZTESTL]	[AL]
			[REL0]	[LAB]
/ A long/far-pointer already in a register pair (a deref or post-dec result): TESTL it.
%	PEREL
	LONG|LPTX	ANYR	ANYR	*	TEMP
		TREG		LONG|LPTX
		*		*
			[ZTESTL]	[R]
			[REL0]	[LAB]
/ ORDERING (l>0 / l<0 / ...): TESTL sets S and Z but NOT V or C (it is a logical op),
/ so the signed (S XOR V) and unsigned (C) condition codes would read STALE flags.
/ Clear V and C after TESTL (RESFLG V,C = mask 9) so they read 0 -- exactly what a CPL
/ against 0 sets (no overflow, no borrow).  +1 one-word insn vs the equality form.
%	PNEREL
	LONG|LPTX	NONE	*	*	NONE
		ADR		LONG|LPTX
		*		*
			[ZTESTL]	[AL]
			[ZRESFLG]	[CONST 9]
			[REL0]	[LAB]
%	PNEREL
	LONG|LPTX	ANYR	ANYR	*	TEMP
		TREG		LONG|LPTX
		*		*
			[ZTESTL]	[R]
			[ZRESFLG]	[CONST 9]
			[REL0]	[LAB]

/////////
/ Load the constant 0.  CLR is the shortest/fastest zero on the Z8000;
/ SUBL zeroes a whole register pair (long / segmented pointer) in one op.
/////////
%	PRVALUE
	WORD		ANYR	*	*	TEMP
		0|MMX		*
		*		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		0|MMX		*
		*		*
			[ZCLR]	[R]

%	PRVALUE
	LONG|LPTX	ANYR	*	*	TEMP
		0|MMX		*
		*		*
			[ZSUBL]	[R],[R]

/////////
/ EFFECT-only discard: a non-float value already in a register, in a context that
/ wants only the side effect (a statement like `*a=*b;` or a call whose result is
/ unused). Emit nothing. This is the TERMINATOR for the selfix<->iselect MEFFECT
/ loop (a discarded computed value had no PEFFECT match -> infinite recursion).
/ Verbatim from the i8086 reference (its NFLT/RREG discard).
/////////
%	PEFFECT
	NFLT		NONE	*	*	NONE
		RREG|MMX	NFLT
		*		*
			;

/ EFFECT-only discard of a FLOAT/DOUBLE value (a discarded soft-float statement like
/ `d;', `d+1.0;', or a double-returning call whose result is unused).  The i8086 NFLT
/ rule above excludes floats (its x87 needs an FSTP pop); the Z8001 soft-float keeps
/ F32/F64 in register pairs/RQ0, so a discard is a no-op -- emit nothing.  Without this
/ a discarded float had no PEFFECT terminator and selfix<->iselect recursed to a stack
/ overflow (cc1 SEGV), the float twin of the LONG|LPTX truth-test terminator above.
%	PEFFECT
	FLT|DBL		NONE	*	*	NONE
		RREG|MMX	FLT|DBL
		*		*
			;

/////////
/ Load a simple lvalue / immediate into a register (by width).
/////////
%	PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|IMM		WORD
		*		*
%	PLVALUE
	WORD		ANYL	*	*	TEMP
		ADR|IMM		WORD
		*		*
			[ZLD]	[R],[AL]

%	PRVALUE
	BYTE		ANYR	*	*	TEMP
		ADR|IMM		BYTE
		*		*
			[ZLDB]	[LO R],[AL]

/ Far pointer (LPTR/LPTX, 2-word seg:offset PAIR) -- the segmented (VLARGE) model
/ the C900 actually uses (kernel + userland: LDL/PUSHL/@RRn everywhere).
/ Identity FIRST: a far pointer ALREADY in a temp register pair (e.g. the LDA
/ result of an address-of) is its own value -- share the temp, emit nothing.
/ Terminates the selfix FIXUP(REG) re-selection (else it re-loads forever and
/ recurses to a stack overflow).
%	PRVALUE|P_SLT
	LPTX		ANYR	ANYR	*	TEMP
		TREG		LPTX
		*		*
%	PLVALUE|P_SLT
	LPTX		ANYL	ANYL	*	TEMP
		TREG		LPTX
		*		*
			;

/ Address-of off the stack frame: &lvalue is a FAR (LPTX) pointer. amd.c flags an
/ FP/base-relative address T_LSS. LDA computes the 16-bit OFFSET into the LOW half
/ of the pair; the SEGMENT goes in the HIGH half (CLR = flat seg 0).
/ MUST precede the generic ADR load below: a T_LSS address is NOT isadr, so the
/ ADR-load pattern would "match" it only by COERCING (selrv) the address into a
/ register -- which cascades into a store-list overflow. The LSS pattern matches
/ T_LSS exactly (MMX) and emits LDA directly, no coercion.
/ The SEGMENT half = R14, the stack segment (the SP register pair RR14 = R14:R15 holds
/ seg:offset; table1.c reserves R14 as "SP seg").  A stack local lives in the stack
/ segment, so its far pointer's segment is R14 -- NOT a hardcoded 0 (which is only
/ correct when the stack happens to sit in segment 0, e.g. our flat sim harness).  In
/ the flat model R14=0 so this stays seg 0 (no change); on the segmented C900 it carries
/ the real stack segment.
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		LSS|MMX		LPTX
		*		*
%	PLVALUE
	LPTX		ANYL	*	*	TEMP
		LSS|MMX		LPTX
		*		*
			[ZLDA]	[LO R],[NSE AL]
			[ZLD]	[HI R],[REGNO R14]

/ Load a far pointer from memory / immediate into a register PAIR (LDL).
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		ADR|IMM		LONG|LPTX
		*		*
%	PLVALUE
	LPTX		ANYL	*	*	TEMP
		ADR|IMM		LONG|LPTX
		*		*
			[ZLDL]	[R],[AL]

/ Materialize a 16-bit INT as a far pointer (CONVERT/CAST int->LPTX, e.g. the 3.2
/ kernel's `dp = FP_OFF(...)' small-model idiom, or `(char *)n').  Donor-mirrored
/ (i8086 int-constant->LPTX): the int is the OFFSET word, the SEGMENT is ZERO --
/ an int value carries no segment.  Code that needs a real segment must convert
/ through long (pair identity) or pointer arithmetic (segment flows in the pair).
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		ADR|IMM		WORD
		*		*
%	PLVALUE
	LPTX		ANYL	*	*	TEMP
		ADR|IMM		WORD
		*		*
			[ZLD]	[LO R],[AL]
			[ZCLR]	[HI R]

/ ... and from a WORD already in a register (a register variable cast to a far
/ pointer, e.g. the kernel's `cvirt(op)' with a register op): same materialization.
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		RREG|LREG|MMX	WORD
		*		*
%	PLVALUE
	LPTX		ANYL	*	*	TEMP
		RREG|LREG|MMX	WORD
		*		*
			[ZLD]	[LO R],[AL]
			[ZCLR]	[HI R]

/ Far pointer as a FUNCTION ARGUMENT (PFNARG): materialize the pair, PUSHL it.
/ Address-of argument:
%	PFNARG
	LPTX		ANYR	*	*	NONE
		LSS|MMX		LPTX
		*		*
			[ZLDA]	[LO R],[NSE AL]
			[ZLD]	[HI R],[REGNO R14]
			[ZPUSHL]	[R]
/ A far pointer already addressable / in a pair:
%	PFNARG
	LPTX		ANYR	*	*	NONE
		ADR|IMM		LONG|LPTX
		*		*
			[ZLDL]	[R],[AL]
			[ZPUSHL]	[R]

/ LONG (32-bit int) value argument: materialize the pair, PUSHL it (same shape as a
/ far pointer).  Needed e.g. for dlflt(long)/dvflt(ulong) long->double conversions.
%	PFNARG
	LONG		ANYR	*	*	NONE
		ADR|IMM		LONG
		*		*
			[ZLDL]	[R],[AL]
			[ZPUSHL]	[R]

/ WORD CONSTANT argument: push the immediate directly (PUSH @R15,#imm), no temp
/ load.  The original MWC backend does this for every constant arg; the generic
/ rule below would waste an `LD R0,#imm' before the push.  Must precede it (first
/ match wins).  NONE = no temp register needed.
%	PFNARG
	WORD		NONE	*	*	NONE
		IMM|MMX		WORD
		*		*
			[ZPUSH]	[AL]

/ WORD value argument (the common case: int / near-pointer-sized). Materialize in
/ a temp, PUSH it.
%	PFNARG
	WORD		ANYR	*	*	NONE
		ADR|IMM		WORD
		*		*
			[ZLD]	[R],[AL]
			[ZPUSH]	[R]

/ BYTE value argument: load (zero/sign already handled by promotion), push the
/ word-aligned temp.
%	PFNARG
	BYTE		ANYR	*	*	NONE
		ADR|IMM		BYTE
		*		*
			[ZLDB]	[LO R],[AL]
			[ZPUSH]	[R]

/ Load a LONG (32-bit integer) into a register pair.  A 32-bit IMMEDIATE is loaded
/ as two word LDs via the [HI]/[LO] halves: gen1.c emits a long constant one 16-bit
/ half per operand (offs picks upper/lower), so ZLDL #imm32 would only get one half.
/ (Same total size as LDL #imm32; correct either way.)
%	PRVALUE
	LONG		ANYR	*	*	TEMP
		IMM|MMX		LONG
		*		*
			[ZLD]	[HI R],[HI AL]
			[ZLD]	[LO R],[LO AL]

/ A LONG from MEMORY is two contiguous words -> a single LDL.
%	PRVALUE
	LONG		ANYR	*	*	TEMP
		ADR		LONG
		*		*
			[ZLDL]	[R],[AL]

/ DOUBLE (F64) function argument: pin RQ0 (the KD quad allocator can't hand out a
/ free quad temp, so use the fixed pair-halves RR0/RR2 like mul.t pins RQ0). Load
/ the high pair (offset 0) + low pair (offset 4), then PUSHL low then high so the
/ 8-byte double lands word0-at-lowest-address on the stack.
%	PFNARG
	DBL		RQ0	*	*	NONE
		ADR		DBL
		*		*
			[ZLDL]	[REGNO RR0],[HI AL]
			[ZLDL]	[REGNO RR2],[LO AL]
			[ZPUSHL]	[REGNO RR2]
			[ZPUSHL]	[REGNO RR0]

/ DOUBLE (F64) load from memory into the pinned RQ0 (the soft-float "accumulator"):
/ two LDLs, high pair (offset 0) then low pair (offset 4).  Used to materialize a
/ plain double leaf (e.g. `return a;`) into the RQ0 return register.
%	PRVALUE
	DBL		RQ0	*	*	RQ0
		ADR		DBL
		*		*
			[ZLDL]	[REGNO RR0],[HI AL]
			[ZLDL]	[REGNO RR2],[LO AL]
/ A double already in RQ0 (a soft-float call result) is its own value -- emit nothing.
%	PRVALUE
	DBL		RQ0	*	*	RQ0
		TREG		DBL
		*		*

/////////
/ Float <-> double conversions.  The murphy float package (libc/crt) converts with
/ a REGISTER convention -- dfpack: float in rr0 -> double in rq0; fdpack: double in
/ rq0 -> float in rr0.  Like the i386 backend's inline [ZCALL][GID _dfcvt], the
/ CONVERT lowers to: set up the operand register, then call the routine by name.
/ No stack arg, no wrapper -- the register setup is the conversion code.
/////////
/ Widen float -> double: load the 4-byte float into rr0, call dfpack (-> rq0).
%	PRVALUE
	DBL		RQ0	*	*	RQ0
		ADR		FLT
		*		*
			[ZLDL]	[REGNO RR0],[AL]
			[ZCALL]	[GID dfpack]
/ Narrow double -> float: get the double into rq0, call fdpack (-> float in rr0).
/ The double already in rq0 (a computed value):
%	PRVALUE
	FLT		RR0	*	*	RR0
		TREG		DBL
		*		*
			[ZCALL]	[GID fdpack]
/ The double in memory: load it into rq0 first (high pair, low pair), then call.
%	PRVALUE
	FLT		RR0	*	*	RR0
		ADR		DBL
		*		*
			[ZLDL]	[REGNO RR0],[HI AL]
			[ZLDL]	[REGNO RR2],[LO AL]
			[ZCALL]	[GID fdpack]

/////////
/ Widen byte -> word.
/   signed:   load the byte, EXTSB sign-extends it through the word.
/   unsigned: load the byte, clear the high byte.
/////////
%	PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR		FS8
		*		*
			[ZLDB]	[LO R],[AL]
			[ZEXTSB]	[R]

%	PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR		FU8
		*		*
			[ZLDB]	[LO R],[AL]
			[ZCLRB]	[HI R]

/ Already in a register (selection put the byte in a temp, e.g. a postinc's old
/ value via LDB) -> extend it through the word: a bare LDB leaves the high byte
/ holding whatever the register last held, so re-tagging alone would read garbage
/ in a word consumer (e.g. `if(c++)' testing the full word).  Signed sign-extends
/ (EXTSB), unsigned clears the high byte, matching the memory-operand widen above.
%	PRVALUE|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		FS8
		*		*
			[ZEXTSB]	[R]

%	PRVALUE|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		FU8
		*		*
			[ZCLRB]	[HI R]

/////////
/ Widen word -> long.
/   signed:   load low word, EXTS sign-extends into the high word.
/   unsigned: load low word, clear the high word.
/////////
%	PRVALUE
	LONG		ANYR	*	*	TEMP
		ADR|IMM		FS16
		*		*
			[ZLD]	[LO R],[AL]
			[ZEXTS]	[R]

%	PRVALUE
	LONG		ANYR	*	*	TEMP
		ADR|IMM		UWORD
		*		*
			[ZLD]	[LO R],[AL]
			[ZCLR]	[HI R]

/////////
/ Widen byte -> long (compose byte->word->long).
/////////
%	PRVALUE
	LONG		ANYR	*	*	TEMP
		ADR		FS8
		*		*
			[ZLDB]	[LO LO R],[AL]
			[ZEXTSB]	[LO R]
			[ZEXTS]	[R]

%	PRVALUE
	LONG		ANYR	*	*	TEMP
		ADR		FU8
		*		*
			[ZLDB]	[LO LO R],[AL]
			[ZCLRB]	[HI LO R]
			[ZCLR]	[HI R]

/////////
/ Narrow long -> word: grab the low word.
/////////
%	PRVALUE
	WORD		ANYR	*	*	TEMP
		ADR|IMM		LONG|LPTX
		*		*
			[ZLD]	[R],[LO AL]

/////////
/ Narrow long -> byte / word -> byte (range reduction).
/   signed result keeps EXTSB; unsigned clears the high byte.
/////////
/ Narrow a WORD held in a register with NO BYTE HALF.  [LO AL] dials the
/ operand's BYTE half, and only R0..R7 have one; a `register' variable placed in
/ R8..R12 (bind.c's callee-saved pool) has r_lohalf == -1, which no byte
/ operand can encode.  A WORD register
/ move is exactly as cheap as the byte move -- LD Rd,Rs and LDB RLd,RLs are both
/ one word -- and the EXTSB/CLRB below overwrites the other half either way, so
/ this costs nothing.  MMX is load-bearing: without it a flag MISS does not reject
/ the pattern, it COERCES the operand, so these rules swallowed every memory
/ operand of a byte narrow and forced it through a register.
%	PRVALUE
	FS8		ANYR	*	*	TEMP
		NBH|MMX		WORD
		*		*
			[ZLD]	[R],[AL]
			[ZEXTSB]	[R]

%	PRVALUE
	FU8		ANYR	*	*	TEMP
		NBH|MMX		WORD
		*		*
			[ZLD]	[R],[AL]
			[ZCLRB]	[HI R]

%	PRVALUE
	FS8		ANYR	*	*	TEMP
		ADR		WORD
		*		*
			[ZLDB]	[LO R],[LO AL]
			[ZEXTSB]	[R]

%	PRVALUE
	FU8		ANYR	*	*	TEMP
		ADR		WORD
		*		*
			[ZLDB]	[LO R],[LO AL]
			[ZCLRB]	[HI R]

%	PRVALUE
	FS8		ANYR	*	*	TEMP
		ADR		LONG|LPTX
		*		*
			[ZLDB]	[LO R],[LO LO AL]
			[ZEXTSB]	[R]

/ Narrow a WORD already in a register down to a byte.  A char/byte bit-field
/ read extracts the field into a temp (shifts/masks), so the value to narrow is
/ a register, not memory (the ADR rules above).  Signed: EXTSB sign-extends the
/ low byte through the word; unsigned: clear the high byte.
%	PRVALUE|P_SLT
	FS8		ANYR	ANYR	*	TEMP
		TREG		WORD
		*		*
			[ZEXTSB]	[R]

%	PRVALUE|P_SLT
	FU8		ANYR	ANYR	*	TEMP
		TREG		WORD
		*		*
			[ZCLRB]	[HI R]

/////////
/ Same-width signedness relabel (identity): a CONVERT/CAST between the two
/ signednesses of one machine kind is a pure reinterpretation -- same register,
/ same bits, no instruction.  These reach selection only when the convert sits
/ on a sign-sensitive op (DIV/REM/SHR and the op-assigns): modoper keeps that
/ boundary as a node (relabeling the child would change its divide/shift kind,
/ and dropping it would hand a sign-sensitive PARENT the opposite signedness).
/ Share the operand's temp, emit nothing.  Also the FIXUP identity terminator
/ for a word/long value already in its temp (cf. the LPTX/float terminators).
/////////
%	PRVALUE|P_SLT
	WORD		ANYR	ANYR	*	TEMP
		TREG		WORD
		*		*
			;

%	PRVALUE|P_SLT
	LONG		ANYR	ANYR	*	TEMP
		TREG		LONG
		*		*
			;

/////////
/ Segmented-pointer conversions (LPTX <-> LONG share the 2-word layout; the
/ offset is the low word, the segment the high word).  [DRAFT -- full seg:offset
/ widen/narrow (e.g. int -> far ptr building a segment) needs the doc-01 S3
/ addressing design.]
/////////
%	PRVALUE
	LPTX		ANYR	*	*	TEMP
		ADR|IMM		LONG|LPTX
		*		*
			[ZLDL]	[R],[AL]

/////////
/ TODO (parity with the i8086 original):
/   - floating point (F32/F64/F80) loads + int<->float conversions: emit Z8000
/     EPA "extended instructions" for the Z8070 FPU/EPU (load EPU<-mem, internal
/     op, store EPU->mem). The SAME stream runs on a real Z8070 (FCW.EPA=1) or via
/     the Extended-Instruction trap + soft-float emulator (EPA=0) -- one binary,
/     no hand-rolled CALLs. (Untestable until the sim models the EPU.)
/   - the full FIXUP web: the safety-net entries that coerce any operand into a
/     register when no other table matched (the "work carefully" comment in the
/     original).  Each is a TREG load like the byte->word re-tag above.
/////////
