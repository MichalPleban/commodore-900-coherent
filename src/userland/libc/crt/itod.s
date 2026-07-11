rdump:	ret
	.globl	diflt, duflt, dlflt, dvflt
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS

/ converts integer types  to double in rq0
/
/ diflt(i)  int i
/ duflt(i)  unsigned int i
/ dlflt(l)  long l
/ dvflt(v)  unsigned long v

diflt:
	ld	r2, SS|4(r15)
	calr	rdump
	test	r2
	jr	pl, 0f
	neg	r2
	ld	r1, $32768
	jr	un, 1f
duflt:
	ld	r2, SS|4(r15)
	calr	rdump
	test	r2
0:
	jr	z, retz
	sub	r1, r1
1:
	ld	r0, $1022+16
	sub	r3, r3
	jr	un, 9f
dlflt:
	calr	rdump
	ldl	rr2, SS|4(r15)
	testl	r2
	jr	pl, 0f

	ld	r0, $-1
	com	r2
	neg	r3
	sbc	r2, r0
	ld	r1, $32768
	jr	un, 1f
dvflt:
	calr	rdump
	ldl	rr2, SS|4(r15)
	testl	r2
0:
	jr	z, retz
	sub	r1, r1
1:
	ld	r0, $1022+33

0:
	dec	r0
9:
	slll	rr2
	jr	nc, 0b

	calr	rdump
	slll	rr2
	rlc	r0
	slll	rr2
	rlc	r0
	slll	rr2
	rlc	r0
	slll	rr2
	rlc	r0
	calr	rdump

	or	r0, r1

	ld	r1, r2
	ld	r2, r3
	sub 	r3, r3

	ret
retz:
	subl	rr0, rr0
	subl	rr2, rr2
	ret
