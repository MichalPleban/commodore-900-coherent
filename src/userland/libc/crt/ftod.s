	.globl	dfpack
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS
dfpack:
	subl	rr2, rr2
	testl	rr0
	ret	z

	sub	r5, r5
	rl	r0
	rrc	r5
	sub	r0, $126*256	/ remove bias

	sra	r0		/ c = 0
	sral	r0
	rrc	r2
	sral	r0
	rrc	r2
	sral	r0
	rrc	r2

	add	r0, $1022*16
	or	r0, r5
	ret
