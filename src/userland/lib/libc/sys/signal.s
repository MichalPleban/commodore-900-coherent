/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface routines for the signal system call.
/ This is not just a direct call, but must save
/ and restore user state before and after processing
/ user signal code.
/
/ int (*(*))() signal(signo, function) int signo; int (*function)();
/ int signo;
/ int (*function)();

NSIG	= 16

.globl	signal_

signal_:
	ld	r1, rr14(4)		/ signo
	cp	r1, $NSIG
	jr	ugt, 0f			/ > NSIG illegal

	ldl	rr2, rr14(6)		/ check function argument
	cpl	rr2, $1			/ is function ?
	jr	ule, 0f			/ 0 or 1 (SIG_DFL or SIG_IGN)

	ldl	rr2, $sigsave		/ pointer
	pushl	(rr14), rr2		/   changed to sigsave
	push	(rr14), r1		/ signo
	pushl	(rr14), rr0		/ and junk PC address
	sys	060			/ 48
	inc	r15, $10		/ pop args
	jr	un, 1f			/ return value

0:	sys	060			/ 48
1:	cp	r1, $-1
	jr	eq, 1f			/ error return

	sub	r2, r2			/ clear high half of rr2
	ld	r3, rr14(4)		/ signo
	add	r3, r3			/ multiply index by 4 to
	add	r3, r3			/ check old signal setting
	addl	rr2, $sigtable-4	/ in sig table
	ldl	rr4, (rr2)		/ Looking at sigtable[signo-1]
	cpl	rr4, $-1		/
	jr	eq, 0f			/ unknown: yield syscall return

	ldl	rr0, (rr2)		/ else yield old user function
0:
	ldl	rr4, rr14(6)		/ Save new user function
	ldl	(rr2), rr4		/ in table
1:
	ret

/ This routine is called after the system has pushed PC, FCW, and
/ signal number onto the stack.  It handles the dirty work.
/ Saves state and calls user function.

sigsave:
	dec	r15, $12		/ make room on stack
	ldm	(rr14), r0, $6		/ save r0-r5
	ld	r3, rr14(12)		/ get signo (pushed by system)
	push	(rr14), r3		/ pass to user function
	add	r3, r3			/ multiply r3
	add	r3, r3			/   times 4
	sub	r2, r2			/ clear high half of rr2
	addl	rr2, $sigtable-4	/
	ldl	rr2, (rr2)
	call	(rr2)			/ (*sigtable[signo-1])(signo)
	inc	r15, $2

	ld	r0, rr14(14)		/ get indicator flags
	ldctlb	FLAGS, rl0		/ restore them
	ldm	r0, (rr14), $6		/ restore r0-r5
	lda	rr14, rr14(16)		/ pop stack, don't disturb indicators
	ret

	.prvd
sigtable:
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
	.long	-1
