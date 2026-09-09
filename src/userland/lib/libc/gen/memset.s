/ Copyright (c) 2026 OpenCoherent contributors.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Coherent string library
/ set n bytes at s to the byte c, return s
/ char *memset(s, c, n)
/ char *s;
/ int c;
/ unsigned n;
	.globl	memset_
	.globl	SS

memset_:
	ldm	r2, SS|4(r15), $2	/ rr2 = s (working destination)
	ld	r0, SS|10(r15)		/ r0 = n
	test	r0			/ n == 0: store nothing
	jr	z, 1f
	ld	r1, SS|8(r15)		/ rl1 = fill byte c
	ldl	rr4, rr2		/ rr4 = source = s
	ldb	(rr2), rl1		/ s[0] = c
	dec	r0			/ one byte done
	jr	z, 1f			/ n == 1: finished
	inc	r3, $1			/ destination = s + 1
	ldirb	(rr2), (rr4), r0	/ propagate c through s[1..n-1]
1:
	ldl	rr0, SS|4(r15)		/ return s
	ret
