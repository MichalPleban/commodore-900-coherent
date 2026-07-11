/ Segmented Z8001 Coherent string library
/ Return length of string
/ strlen(s1)
/ char *s1;

	.globl strlen_
	.globl SS

strlen_:
	ldl	rr2, SS|4(r15)		/ rr2 = s1

	subl	rr0, rr0		/ r1 = count, r0 = NULL
	cpirb	rl0, (rr2), r1, eq	/ Scan string for '\0'
	neg	r1			/ r0 = string length (plus NULL)
	dec	r1			/ r0 = string length
	ret
