	.globl	fdpack

/ convert double in rq0  to  float in rr0

fdpack:
	sub	r3, r3
	rl	r2
	rlc	r1
	rlc	r0
	rrc	r3	/ save sign

	sub	r0, $[1022-126]*32	/ adjust bias
	jr	pl, 0f

	subl	rr0, rr0  / exponent underflow
	ret
0:
	cp	r0, $256*32
	jr	ult, 0f
	ld	r0, $255*128	/ exponent overflow
	or	r0, r3
	ret
0:
	rl	r2
	rlc	r1
	rlc	r0
	rl	r2
	rlc	r1
	rlc 	r0

	or	r0, r3
	ret
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1024   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +128    fraction (missing hi bit)
/
	.globl	SS
