/ Segmented Z8001 Coherent string library
/ Concatenate one string to another (s2 to s1)
/ strcat(s1, s2)
/ char *s1, *s2;

	.globl strcat_
	.globl SS

strcat_:
	ldm	r2, SS|4(r15), $4	/ rr2 = s1, rr4 = s2

	subl	rr0, rr0		/ r0 = count, r1 = NULL
	cpirb	rl1, (rr4), r0, eq	/ Scan string (s2) for NULL
	neg	r0			/ r0 = string length of s2

	sub	r5, r5			/ r5 = count
	cpirb	rl1, (rr2), r5, eq	/ Scan string (s1) for NULL
	dec	r3			/ backup to point at '\0' byte

	ld	r5, SS|10(r15)		/ rr4 = s2 (restore offset)
	ldirb	(rr2), (rr4), r0	/ Copy string

	ldl	rr0, SS|4(r15)		/ rr0 = s1
	ret
