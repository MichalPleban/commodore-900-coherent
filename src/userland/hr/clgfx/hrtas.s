/ hrtas.s - atomic test-and-set for the hrgui drawing-lock fast path.
/
/ hr_tas(short *w):  the ONE userland-atomic primitive the futex fast path needs.
/ Z8001 TSET is a bus-locked test-and-set: it copies the operand top bit into
/ the S flag and THEN forces the operand to all ones.  So on a free word (0x0000,
/ top bit 0) it returns 0 and leaves the word 0xFFFF == we just took the lock; on
/ a held word (0xFFFF) it returns 1 and leaves it held.  LD does not touch flags,
/ so the pl/mi test still reflects the TSET.  Release is a plain word store (see
/ hr_unlock in hrlock.c) and the kernel slow path test-sets under sphi(), where
/ nothing can preempt it -- so this is the only place a real TSET is required.

	.globl	hr_tas_

hr_tas_:
	ldl	rr2, rr14(4)		/ rr2 = &w  (first arg)
	tset	(rr2)			/ S = old top bit; (rr2) := 0xFFFF
	ld	r1, $0			/ assume was free -> return 0
	ret	pl			/ top bit was 0 (free): acquired, r1 = 0
	ld	r1, $1			/ was held -> return 1
	ret
