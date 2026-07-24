/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 C Library
/ Setjmp and longjmp -- non local goto.
/
/ #include <setjmp.h>
/ 
/ setjmp(env);
/ jmp_buf env;
/
/ longjmp(env, val)
/ jmp_buf env;
/

	.globl	setjmp_
	.globl	longjmp_

setjmp_:
	popl	rr4, (rr14)	/ Return PC
	ldl	rr2, (rr14)	/ Address of the environment
	ldm	(rr2), r4, $12	/ Save PC, r6-r15
	sub	r1, r1
	jp	(rr4)

longjmp_:
	ldl	rr2, rr14(4)	/ Get address of the environment
	ld	r1, rr14(8)	/ Get return value
	ldm	r4, (rr2), $12	/ Restore PC, r6-r15
	jp	(rr4)
