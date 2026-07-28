	.shri
	.globl fcpy_

	/* fast copy, fcpy(destination, source, number of WORDS */
fcpy_:
	ldl	rr2, SS|4(r15)
	ldl	rr4, SS|8(r15)
	ld	r1,  SS|12(r15)
	ldir	(rr2), (rr4), r1
	ret	un

