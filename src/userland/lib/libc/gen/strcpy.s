/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Coherent string library
/ Copy one string to another (s2 to s1)
/ strcpy(s1, s2)
/ char *s1, *s2;

	.globl strcpy_
	.globl SS

strcpy_:
	ldm	r2, SS|4(r15), $4	/ rr2 = s1, rr4 = s2

	subl	rr0, rr0		/ r0 = count, r1 = NULL
	cpirb	rl1, (rr4), r0, eq	/ Scan string for NULL
	neg	r0			/ r0 = string length
	sub	r5, r0			/ rr4 back to beg. of s2

	ldirb	(rr2), (rr4), r0	/ Copy string

	ldl	rr0, SS|4(r15)		/ rr0 = s1
	ret
