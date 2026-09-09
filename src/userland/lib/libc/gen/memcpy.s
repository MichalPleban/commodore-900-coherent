/ Copyright (c) 2026 OpenCoherent contributors.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Coherent string library
/ copy n bytes from src to dst, return dst
/ char *memcpy(dst, src, n)
/ char *dst, *src;
/ unsigned n;
	.globl	memcpy_
	.globl	SS

memcpy_:
	ldm	r2, SS|4(r15), $4	/ rr2 = dst, rr4 = src
	ld	r0, SS|12(r15)		/ r0 = n
	test	r0			/ n == 0: copy nothing (a count of 0
	jr	z, 1f			/  would mean 65536 to ldir/ldirb)
	ld	r1, r0			/ fast word copy iff count AND both
	or	r1, r3			/  addresses are even
	or	r1, r5			/
	rr	r1			/ carry set if anything odd
	jr	c, 2f			/ odd -> byte copy
	srl	r0			/ words = n / 2
	ldir	(rr2), (rr4), r0	/ word copy
	jr	1f
2:
	ldirb	(rr2), (rr4), r0	/ byte copy
1:
	ldl	rr0, SS|4(r15)		/ return dst
	ret
