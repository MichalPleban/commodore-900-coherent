/ Segmented Z8001 Coherent string library
/ compare two strings (n chars significant in comparison)
/ strncmp(s1, s2, n)
/ char *s1, *s2;
	.globl	strncmp_
	.globl	SS

strncmp_:
	ldm	r2, SS|4(r15), $4	/ rr2=s1, rr4=s2
	subl	rr0, rr0		/ r0=count, r1='\0' byte
	cpirb	rl1, (rr2), r0, eq	/ Find length of one string
	neg	r0			/ length including null byte
	sub	r3, r0			/ restore to beg. of s1

	cp	r0, SS|12(r15)		/ min(strlen(s1), n)
	jr	le, 1f			/

	ld	r0, SS|12(r15)		/

1:
	cpsirb	(rr2), (rr4), r0, ne	/ Compare the strings

	ret	ne			/ return 0 if strings the same
	dec	r3
	dec	r5
	ldb	rl0, (rr2)
	cpb	rl0, (rr4)		/ Compare last byte
	jr	ult, 1f			/ Branch if s1 < s2
	inc	r1
	ret

1:
	dec	r1
	ret
