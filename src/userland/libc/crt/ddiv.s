/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS

	.globl	drdiv, dldiv
/
/ divide a/b  result in rq4
/ see knuth "art of computer programming" vol.2
/  for explanation of double precision divide
/ life is complicated further by the lack of unsigned multply & divide
/
/ drdiv( da, db)
/ dldiv( da, &db)

drdiv:
	lda	rr2, SS|12(r15)
	jr	un, 0f
dldiv:
	ldl	rr2, SS|12(r15)
0:
	sub	r15, $16
	ldm	(rr14), r6, $8

	ld	r0, SS|20(r15)
	ldl	rr4, SS|22(r15)
	ld	r6, SS|26(r15)
	sub	r7, r7

	ld	r8, (rr2)
	ldl	rr12, rr2(2)
	ld	r10, rr2(6)
	sub	r11, r11

	ld	r1, $32768
	ld	r9, r1
	and	r1, r0
	xor	r0, r1
	cp	r0, $16
	jr	lt, retz
	and	r9, r8
	xor	r8, r9
	xor	r1, r9		/ result sign

	cp	r8, $16
	jr	lt, retinf
	cp	r0, $2047*16
	jr	ge, retinf
	cp	r8, $2047*16
	jr	ge, retz
	ldb	rl1, $4		/ form exponents, fractions
0:
	srl	r0
	rrc	r4
	rrc	r5
	rrc	r6
	rrc	r7
	srl	r8
	rrc	r12
	rrc	r13
	rrc	r10
	rrc	r11
	dbjnz	rl1, 0b

	setflg	C
	rrc	r4
	rrc	r5
	rrc	r6
	rrc	r7

	setflg	C
	rrc	r12
	rrc	r13
	rrc	r10
	rrc	r11

	sub	r0, r8
	add	r0, $1023
	jr	le, retz	/ exp will only get smaller	
	cp	r0, $2048
	jr	ge, retinf	/ by at most 1

	srll	rr12		/ make divisor positive
	rrc	r10
	rrc	r11

	srll	rr4		/ make quotient defined & positive
	rrc	r6
	rrc	r7
	srll	rr4
	rrc	r6
	rrc	r7
	srll	rr4
	rrc	r6
	rrc	r7

	divl	rq4, rr12	/ rr6 = good approx to hi part of quotient
				/ we adjust using 1st term of power series
				/ as in knuth
	srll	rr10
	ld	r9, $-1
	com	r10
	neg	r11
	sbc	r10, r9

	multl	rq8, rr6	/ - vl x wwm  ( cf knuth)
	srll	rr4, $1
	addl	rr8, rr4	/ + r ( no ovflw since opp signs)
	sral	rr8
	rrc	r10
	rrc	r11
	divl	rq8, rr12
	extsl	rq8

	slll	rr10		/ undo shifts for positiveness
	rlc	r9
	slll	rr10
	rlc	r9

	addl	rr8, rr6	/ form quotient of fractions
	
	slll	rr10
	rlc	r9
	rlc	r8
	slll	rr10
	rlc	r9
	rlc	r8
	jr	c, 0f
	slll	rr10
	rlc	r9
	rlc	r8
	dec	r0
	jr	le, retz
0:	
	cp	r0, $2047
	jr	ge, retinf

	slll	rr10
	rlc	r9
	rlc	r8
	rlc	r0
	slll	rr10
	rlc	r9
	rlc	r8
	rlc	r0
	slll	rr10
	rlc	r9
	rlc	r8
	rlc	r0
	slll	rr10
	rlc	r9
	rlc	r8
	rlc	r0

	or	r0, r1
	ld	r1, r8
	ld 	r2, r9
	ld 	r3, r10
9:
	ldm	r6, (rr14), $8
	inc	r15, $16
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
