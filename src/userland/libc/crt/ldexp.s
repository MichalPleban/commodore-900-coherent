/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS

	.globl	ldexp_
/ ldexp( m, e)
/ double m
/ int e
/
/ returns  m * 2^e

ldexp_:
	ld	r0, SS|4(r15)
	ld	r1, r0
	res	r0, $15
	srl	r0, $4
	jr	nz, 0f
retz:
	subl	rr0, rr0
	subl	rr2, rr2
	ret
0:
	cp	r0, $2047
	jr	lt, 0f
retinf:
	ld	r0, $2047*32
	rl 	r1
	rrc	r0
	sub	r1, r1
	subl	rr2, rr2
	ret
0:
	add	r0, SS|12(r15)
	jr	le, retz

	cp	r0, $2047
	jr	ge, retinf

	sll	r0, $4
	and	r1, $0x800f	/ preserve sign and fraction
	or	r0, r1

	ld	r1, SS|6(r15)
	ld	rr2, SS|8(r15)
	ret
