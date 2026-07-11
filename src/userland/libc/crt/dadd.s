/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	dradd, dladd, drsub, dlsub

/ add, subtract doubles   return result  a+b  or   a-b   in rq0
/
/ dradd( da, db)
/ dladd( da, &db)
/ drsub( da, db)
/ dlsub( da, &db)
	.globl	SS

dradd:
	sub	r1, r1
	lda	rr4, SS|12(r15)
	jr	un, 0f
dladd:
	sub	r1, r1
	ldl	rr4, SS|12(r15)
	jr	un, 0f
drsub:
	ld	r1, $32768
	lda	rr4, SS|12(r15)
	jr	un, 0f
dlsub:
	ld	r1, $32768
	ldl	rr4, SS|12(r15)
0:
	sub	r15, $16
	ldm	(rr14), r6, $8

	ldl	rr6, SS|20(r15)
	ldl	rr8, SS|24(r15)
	ldl	rr10, (rr4)
	xor	r10, r1
	ldl	rr12, rr4(4)

	ldl	rr0, $0x80008000	/ extract signs
	and	r1, r6
	xor	r6, r1
	and	r0, r10
	xor	r10, r0
	ldb	rl1, rh0

	cpl	rr6, rr10
	jr	ugt, 0f
	jr	ult, 1f
	cpl	rr8, rr12
	jr	uge, 0f
1:
	exb	rh1, rl1
	ex	r6, r10
	ex	r7, r11
	ex	r8, r12
	ex	r9, r13
0:
	ldl	rr2, $0x7ff07ff0	/ extract exponents
	and	r2, r6
	jr	z, retz
	cp	r2, $2047*16
	jr	ge, retinf
	ld	r0, r2			/ will be result exp
	xor	r6, r2
	and	r3, r10
	jr	z, 8f
	xor	r10, r3

	set	r6, $4			/ implicit bits
	set	r10, $4

	sub	r2, r3
	jr	z, 0f			/ exps agree so no alignment
	srl	r2, $4
	cp	r2, $52
	jr	ge, 8f			/ too small - ret larger operand
1:
	srll	r10
	rrc	r12
	rrc	r13
	djnz	r2, 1b
0:
	cpb	rh1, rl1
	jr	eq, 0f			/ signs agree - do add

	subl	rr8, rr12		/ else sub
	sbc	r7, r11
	sbc	r6, r10			/ any underflow is possible
	jr	nz, 1f
	test	r7
	jr	nz, 1f
	testl	rr8
	jr	nz, 1f
retz:
	subl	rr0, rr0
	subl	rr2, rr2
	jr	un, 9f
2:
	dec	r0, $16
	jr	le, retz
	slll	rr8
	rlc	r7
	rlc	r6
1:
	bit	r6, $4
	jr	z, 2b
	jr	un, 8f
0:
	addl	rr8, rr12
	adc	r7, r11
	adc	r6, r10
	bit	r6, $5
	jr	z, 8f
	inc	r0, $16
	cp	r0, $2047*16
	jr	lt, 0f
retinf:
	ld	r0, $2047*16
	orb	rh0, rh1
	sub	r1, r1
	subl	rr2, rr2
	jr	un, 9f
0:
	srll	rr6
	rrc	r8
	rrc	r9
8:
	orb	rh0, rh1
	res	r6, $4
	or	r0, r6
	ld	r1, r7
	ldl	rr2, rr8
9:
	ldm	r6, (rr14), $8
	inc	r15, $16
	ret
