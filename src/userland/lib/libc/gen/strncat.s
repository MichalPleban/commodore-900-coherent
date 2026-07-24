/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Coherent string library
/ Concatenate one string to another (s2 to s1)
/ Copy at most n characters
/ strncat(s1, s2, n)
/ char *s1, *s2;

	.globl strncat_
	.globl SS

strncat_:
	ldm	r2, SS|4(r15), $4	/ rr2 = s1, rr4 = s2

	subl	rr0, rr0		/ Put addr of s1's NULL in rr2
	cpirb	rl1, (rr2), r0, eq
	dec	r3

	sub	r0, r0			/ find strlen(s2)
	cpirb	rl1, (rr4), r0, eq	/ Scan string for NULL
	neg	r0
	sub	r5, r0			/ point to beginning of s2
	dec	r0			/ actual strlen(s2)

	cp	r0, SS|12(r15)		/ min(n, strlen(s2))
	jr	le, 1f
	ld	r0, SS|12(r15)

1:
	ldirb	(rr2), (rr4), r0	/ Copy string

	ldb	(rr2), rl1		/ Append a '\0' to it
	ldl	rr0, SS|4(r15)		/ return s1
	ret
