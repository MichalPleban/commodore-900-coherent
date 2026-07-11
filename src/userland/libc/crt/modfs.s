rdump:	ret
	.globl	modfs_
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS

/ see modf.c for description
/ modfs( d, dp, e)

/ *dp = d anded with exp-1 highest bits of fraction
/ to calc return value -  mask off *dp from d & shift l until justified

modfs_:
	ldl	rr0, $0xfff00000
	subl	rr2, rr2
	ld	r5, SS|16(r15)
	calr	rdump
	jr	un, 1f
0:
	sral	rr0
	rrc 	r2
	rrc	r3
1:
	djnz	r5, 0b
	calr	rdump

	and	r0, SS|4(r15)
	and	r1, SS|6(r15)
	and	r2, SS|8(r15)
	and	r3, SS|10(r15)
	ldl	rr4, SS|12(r15)
	ldl	(rr4), rr0
	ldl	rr4(4), rr2
	calr	rdump

	xor	r0, SS|4(r15)
	xor	r1, SS|6(r15)
	xor	r2, SS|8(r15)
	xor	r3, SS|10(r15)

	jr	nz, 0f
	testl	rr0
	jr	nz, 0f
	test	r2
	ret	z
0:
	ld	r5, SS|16(r15)
0:
	dec	r5
	slll	rr2
	rlc	r1
	rlc	r0
	bit	r0, $4
	jr	z, 0b

	res	r0, $4
	add	r5, $1022
	sll	r5, $4
	or	r0, r5
	ret
