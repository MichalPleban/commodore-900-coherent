/ hrlock.s - the hrgui global drawing lock (GUI.md race fix).
/
/ A single mutual-exclusion lock, taken with the Z8001 TSET instruction (an
/ atomic, bus-locked test-and-set), living in a word of the shared VRAM tail
/ (shmem.h SHM_LOCK) that every process maps.  It is the one mechanism this V7
/ kernel offers for userland mutual exclusion: no system call is involved.
/
/ Held (0xFFFF) while ANY party touches the framebuffer or the clip/z-order
/ state: a client draw primitive, or a server layout/redraw op.  The kernel hr
/ driver does not take it (it cannot spin at interrupt level) -- it only reads
/ it and defers its async cursor redraw one tick while it is held.
/
/   hr_lock(short *lock)     spin (TSET) until acquired, then return.
/   hr_unlock(short *lock)   release (store 0).
/
/ TSET sets S from the operand top bit, THEN sets the operand to all ones; a
/ following LD does not touch flags, so ret-pl returns iff the lock was free.
/ The spin is bounded (~4M tries) so a client that dies holding the lock cannot
/ freeze the whole GUI forever -- after the budget it proceeds best-effort.

	.globl	hr_lock_
	.globl	hr_unlock_

hr_lock_:
	ldl	rr2, rr14(4)		/ rr2 = &lock
	ldl	rr0, $0x00400000	/ deadlock-breaker budget (~4M spins)
1:
	tset	(rr2)			/ S = old top bit; (rr2) := 0xFFFF
	ret	pl			/ was free -> acquired
	subl	rr0, $1
	jr	nz, 1b			/ retry while budget remains
	ret				/ budget spent: proceed (owner presumed dead)

hr_unlock_:
	ldl	rr2, rr14(4)		/ rr2 = &lock
	ld	r3, $0
	ld	(rr2), r3		/ *lock = 0
	ret
