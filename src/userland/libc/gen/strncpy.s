/ Segmented Z8001 Coherent string library
/ Copy one string to another (s2 to s1)
/ Copy at most n characters
/ strncpy(s1, s2, n)
/ char *s1, *s2;

	.globl strncpy_
	.globl SS

strncpy_:
	ldl	rr2, SS|4(r15)		/ rr2 = s1
	ldl	rr4, rr2		/ second copy of s1
	ld	r0, SS|12(r15)		/ n
	sub	r1, r1			/ clear '\0'
	test	r0			/ any to clear
	jr	eq, 1f			/ br if no

	ldb	(rr2), rl1		/ clear first byte
	inc	r3			/ point to next byte
	dec	r0			/ reduce count
	jr	eq, 1f			/ branch if no

	ldirb	(rr2), (rr4), r0	/ clear array

1:
	ldm	r2, SS|4(r15), $4	/ rr2 = s1, rr4 = s2
	sub	r0, r0			/ infinite count

	cpirb	rl1, (rr4), r0, eq	/ Scan string for NULL
	neg	r0			/ r0 = count
	sub	r5, r0			/ point rr4 back to beginning of s2

	cp	r0, SS|12(r15)		/ strlen(s1) < n ?
	jr	le, 1f			/ branch if yes

	ld	r0, SS|12(r15)		/ min(strlen(s1), n)
1:
	ldirb	(rr2), (rr4), r0	/ Copy string

	ldl	rr0, SS|4(r15)		/ rr0 = s1
	ret
