/ Copyright (c) 2026 OpenCoherent contributors.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Coherent string library
/ compare n bytes of a and b; <0, 0 or >0
/ int memcmp(a, b, n)
/ char *a, *b;
/ unsigned n;
	.globl	memcmp_
	.globl	SS

memcmp_:
	ldm	r2, SS|4(r15), $4	/ rr2 = a, rr4 = b
	ld	r0, SS|12(r15)		/ r0 = n
	clr	r1			/ default result = 0 (equal)
	test	r0			/ n == 0: equal (a 0 count would mean
	ret	z			/  65536 to cpsirb)
	cpsirb	(rr2), (rr4), r0, ne	/ compare until a difference or count out
	ret	ne			/ ran to the end, all equal: return 0
	dec	r3			/ step both back onto the differing byte
	dec	r5
	ldb	rl0, (rr2)
	cpb	rl0, (rr4)		/ a[i] : b[i]
	jr	ult, 1f			/ a[i] < b[i]
	ld	r1, $1			/ a[i] > b[i]
	ret
1:
	ld	r1, $-1			/ a[i] < b[i]
	ret
