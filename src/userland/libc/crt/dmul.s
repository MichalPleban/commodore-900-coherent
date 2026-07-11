/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	drmul, dlmul
	.globl	SS

/ double multiply   product returned in rq4
/
/ drmul( da, db)
/ dlmul( da, &db)
/
/ complexified by lack of unsigned multiply in z8000

drmul:
	lda	rr4, SS|12(r15)
	jr	un, 0f
dlmul:
	ldl	rr4, SS|12(r15)
0:
	sub	r15, $16
	ldm	(rr14), r6, $8

	ld	r0, SS|20(r15)

	ld	r8, (rr4)

	ld	r1, $32768
	ld	r3, r1
	and	r1, r8
	xor	r8, r1
	cp	r8, $16
	jr	lt, retz
	cp	r8, $2047*16
	jr	ge, retinf
	and	r3, r0
	xor	r0, r3
	cp	r0, $16
	jr	lt, retz
	cp	r0, $2047*16
	jr	ge, retinf
	xor	r1, r3

	ldl	rr2, SS|22(r15)
	ld	r6, SS|26(r15)
	sub	r7, r7

	ldl	rr12, rr4(2)
	ld	r10, rr4(6)
	sub	r11, r11

	ldb	rl1, $4
0:
	srl	r0
	rrc	r2
	rrc	r3
	rrc	r6
	rrc	r7

	srl	r8
	rrc	r12
	rrc	r13
	rrc	r10
	rrc	r11

	dbjnz	rl1, 0b

	setflg	C
	rrc	r2
	rrc	r3
	rrc	r6
	rrc	r7

	setflg	C
	rrc	r12
	rrc	r13
	rrc	r10
	rrc	r11

	sub	r0, $1022
	add	r0, r8
	jr	le, retz	/ exp will only get smaller
	cp	r0, $2047
	jr	gt, retinf	/ by at most 1

	srll	rr2		/ force fractions to be positive
	rrc	r6
	rrc	r7
	srll	rr6

	srll	rr12
	rrc	r10
	rrc	r11
	srll	rr10

	multl	rq4, rr12
	multl	rq8, rr2
	addl	rr4, rr8
	subl	rr6, rr6
	slll	rr4

	ldl	rr10, rr2
	multl	rq8, rr12
	addl	rr10, rr4
	adc	r9, r6
	adc	r8, r7

	slll	rr10		/ undo effect of adjustment
	rlc	r9
	rlc	r8
	slll	rr10
	rlc	r9
	rlc	r8
	slll	rr10
	rlc	r9
	rlc	r8
	jr	c, 0f		/ justified
	dec	r0
	jr	le, retz
	slll	rr10
	rlc	r9
	rlc	r8
0:
	cp	r0, $2047
	jr	ge, retinf
	ldb	rl1, $4		/ pack result
0:
	slll	rr10
	rlc	r9
	rlc	r8
	rlc	r0
	dbjnz	rl1, 0b

	or	r0, r1
	ld	r1, r8
	ld	r2, r9
	ld	r3, r10
9:
	ldm	r6, (rr14), $8
	add	r15, $16
	ret
retz:
	subl	rr0, rr0
	subl	rr2, rr2
	jr	un, 9b
retinf:
	ld	r0, $2047*16
	or	r0, r1
	sub	r1, r1
	subl	rr2, rr2
	jr	un, 9b
