rdump:
	ret
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS

	.globl	frexp_

/ frexp( real, ep)
/ double real
/ int *ep
/
/ returns fraction in rq0   stores exp at ep

frexp_:
	ldl	rr0, SS|4(r15)
	ldl	rr4, SS|12(r15)
	calr	rdump
	test	r0
	jr	nz, 0f
	subl	rr2, rr2
	ld	(rr4), $-1022
	ret
0:

	ld	r2, $2047*16
	and	r2, r0
	xor	r0, r2

	or	r0, $1022*16	/ fraction has exponent 0
	calr	rdump

	srl	r2, $4
	sub	r2, $1022
	calr	rdump
	ld	(rr4), r2

	ldl	rr2, SS|8(r15)
	ret
