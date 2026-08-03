	.shri
	.globl sdll_
sdll_:
	ldl	rr4, SS|4(r15)
	ldl	rr0,(rr4)
	ld	r2, SS|8(r15)
	sdll	rr0,r2
	ldl	(rr4), rr0
	ret	un

