	.globl	dlcmp, drcmp
/ floating point package for segmented z-8001
/ timothy s. murphy  10/84
/ IEEE format
/ double:	63 62		52 51				0
/	      sign  bin exp +1022   fraction (missing hi bit)
/ float:	31 30		23 22				0
/	      sign  bin exp +126    fraction (missing hi bit)
/
	.globl	SS
/ signed compare of doubles a, b
/ return  r1 = 	1  if  a > b
/	  r1 =	0  if  a = b
/	  r1 =  -1 if  a < b
/
/ drcmp( da, db)
/ dlcmp( da, &db)

dlcmp:
	ldl	rr2, SS|12(r15)
	jr	un, 0f
drcmp:
	lda	rr2, SS|12(r15)
0:
	ldl	rr4, (rr2)		/ b
	test	r4
	jr	mi, 0f

	cpl	rr4, SS|4(r15)
	jr	gt, 9f			/ a < b
	jr	lt, 7f			/ a > b
					/ a is positive
	ldl	rr4, rr2(4)
	cpl	rr4, SS|8(r15)
	jr	ugt, 9f
	jr	ult, 7f
	jr	8f
0:					/ b negative
	ldl	rr0, SS|4(r15)
	test	r0
	jr	pl, 7f

	cpl	rr4, rr0
	jr	ugt, 7f			/ abs(b) > abs(a)  so  a > b
	jr	ult, 9f

	ldl	rr4, rr2(4)
	cpl	rr4, SS|8(r15)
	jr	ugt, 7f
	jr	ult, 9f
8:
	sub	r1, r1
	ret
7:
	ldk	r1, $1
	ret
9:
	ld	r1, $-1
	ret
