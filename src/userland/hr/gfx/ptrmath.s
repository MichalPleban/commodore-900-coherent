	.shri
	.globl subptr_
	.globl addptr_
	.globl incptr_
	.globl decptr_


/* mar-26-85	: offset to be added extended to double precision */
addptr_:
	ldl	rr0, SS|4(r15)
	ldl	rr2, SS|8(r15)
	add	r1, r3			/* add offset amount */
	adcb	rh0, rl2		/* add any segment overflow */
	ret	un


/*  mar-26-85	: updated as addptr */
subptr_:
	ldl	rr0, SS|4(r15)
	ldl	rr2, SS|8(r15)
	sub	r1, r3			/* subtract offset amount */
	sbcb	rh0, rl2		/* get the overflow */
	ret	un


incptr_:
	ldl	rr4, SS|4(r15)		/ fetch pointer to srcw pointer
	ldl	rr2, (rr4)		/ fetch the srcw pointer
	exb	rh2, rl2		/ move seg number down
	slll	rr2, $3			/ shift whole thing over three
	ldl	rr4, SS|8(r15)		/ get address of bit position
	add	r3, (rr4)		/ add current bit position
	ldl	rr0, SS|12(r15)		/ get increment value
	addl	rr2, rr0		/ add the increment to current
	ld	r0, r3			/ get copy of low word
	and	r0, $0xf
	ld	rr4, SS|8(r15)		/ get address of bit pointer
	ld	(rr4), r0		/ save off new bit position
	srll	rr2, $3			/ shift back to remove bitpos
	andb	rl3, $0xFE		/ eliminate lowest bit position
	exb	rh2,rl2			/ put segno back in place
	ldl	rr4, SS|4(r15)		/ get address of word pointer
	ldl	(rr4), rr2		/ and save it back
	ret	un


decptr_:
	ldl	rr4, SS|4(r15)		/ fetch pointer to srcw pointer
	ldl	rr2, (rr4)		/ fetch the srcw pointer
	exb	rh2, rl2		/ move seg number down
	slll	rr2, $3			/ shift whole thing over three
	ldl	rr4, SS|8(r15)		/ get address of bit position
	add	r3, (rr4)		/ add current bit position
	ldl	rr0, SS|12(r15)		/ get increment value
	subl	rr2, rr0		/ add the increment to current
	ld	r0, r3			/ get copy of low word
	and	r0, $0xf
	ld	rr4, SS|8(r15)		/ get address of bit pointer
	ld	(rr4), r0		/ save off new bit position
	srll	rr2, $3			/ shift back to remove bitpos
	andb	rl3, $0xFE		/ eliminate lowest bit position
	exb	rh2,rl2			/ put segno back in place
	ldl	rr4, SS|4(r15)		/ get address of word pointer
	ldl	(rr4), rr2		/ and save it back
	ret	un
