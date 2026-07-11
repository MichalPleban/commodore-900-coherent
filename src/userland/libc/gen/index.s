/ Segmented Z8001 Coherent string library
/ Return addr of first occurence of char in string (or NULL if not found)
/ index(s1, c)
/ char *s1;
/ char c;

	.globl index_
	.globl SS

index_:
	ldl	rr2, SS|4(r15)		/ rr2 = s1
	ldl	rr4, rr2

	subl	rr0, rr0		/ r0 = count, r1 = NULL
	cpirb	rl1, (rr2), r0, eq	/ Scan string for '\0'
	neg	r0			/ r0 = string length (plus '\0')

	ldl	rr2, rr4		/ rr2 = s1
	ld	r1, SS|8(r15)		/ r1 = c
	cpirb	rl1, (rr2), r0, eq	/ Scan string for c

	jr	ne, 1f			/ Not found
	dec	r3, $1			/ Back to character
	ldl	rr0, rr2		/ Return pointer to c
	ret

1:
	subl	rr0, rr0		/ NULL
	ret
