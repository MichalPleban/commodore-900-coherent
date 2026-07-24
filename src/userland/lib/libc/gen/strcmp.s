/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Coherent string library
/ compare two strings
/ strcmp(s1, s2)
/ char *s1, *s2;
	.globl	strcmp_
	.globl	SS

strcmp_:
	ldm	r2, SS|4(r15), $4	/ rr2 = s1, rr4 = s2

	subl	rr0, rr0		/ r0=count, r1=null byte
	cpirb	rl1, (rr2), r0, eq	/ Find length of `s1' string
	neg	r0			/ Adjust string length

	sub	r3, r0			/ Put rr2 back at beg. of s2
	cpsirb	(rr2), (rr4), r0, ne	/ Compare the strings

	ret	ne			/ Return 0 if strings the same

	dec	r3
	dec	r5
	ldb	rl0, (rr2)
	cpb	rl0, (rr4)		/ check last byte
	jr	ult, 1f			/ Branch if s1 < s2
	inc	r1
	ret

1:
	dec	r1
	ret
