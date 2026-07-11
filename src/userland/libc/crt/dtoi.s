	.globl	ifix, ufix, lfix, vfix
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS

/ convert double to integer type
/
/ ifix returns int in r1
/ ufix returns unsigned int in r1
/ lfix returns long in rr0
/ vfix returns unsigned long in rr0

/ subroutine dtoi returns  sign in r3   + if 0   - if <>0
/			   exponent in r2
/			   greatest integer <= abs value of arg  in rr0
/				provided  exp <= 32
ifix:
	calr	dtoi
	cp	r2, $15
	jr	ule, 0f
	ld	r1, $0x8000
	ret
0:
	test	r3
	ret	z
	neg	r1
	ret
ufix:
	calr	dtoi
	test	r3
	jr	z, 0f
	sub	r1, r1
	ret
0:
	cp	r2, $16
	ret	ule
	ld	r0, $-1
	ret
lfix:
	calr	dtoi
	cp	r2, $31
	jr	ule, 0f
	ldl	rr0, $0x80000000
	ret
0:
	test	r3
	ret	z

	ld	r3, $-1
	com	r0
	neg	r1
	sbc	r0, r3
	ret
vfix:
	calr	dtoi
	test	r3
	jr	z, 0f
	subl	rr0, rr0
	ret
0:
	cp	r2, $32
	ret	ule
	ldl	rr0, $-1
	ret

dtoi:
	ldl	rr0, SS|10(r15)
	ld	r2, SS|8(r15)

	ld	r3, $32768
	and	r3, r2
	xor	r2, r3
	ldk	r4, $4
0:
	srl	r2
	rrc	r0
	rrc	r1
	djnz	r4, 0b
				/ rr0 now is hi 32 bits of fraction
	sub	r2, $1022
	jr	gt, 0f
	subl	rr0, rr0
	subl	rr2, rr2	/ ret 0 if exp <= 0
	ret
0:
	setflg	C
	rrc	r0
	rrc	r1

	ld	r4, r2
	sub	r4, $32
	ret	ge

	sdll	rr0, r4
	ret
