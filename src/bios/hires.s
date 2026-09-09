/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ Hi-res display console glue.

SS	= 0x3F3F

	.globl	putchar_
	.globl	getchar_
	.globl	outstr_
	.globl	hexval_
	.globl	hrline_

/ hexval(c) -- value of a hex digit, -1 if none.
hexval_:
	dec	r15, $4
	ldl	@rr14, rr12
	ld	r13, r15
	ld	r12, SS|0x08(r13)
	cp	r12, $0x0030
	jr	lt, 1f
	cp	r12, $0x0039
	jr	gt, 1f
	ld	r1, r12
	sub	r1, $0x0030
	jr	9f
1:
	cp	r12, $0x0061
	jr	lt, 2f
	cp	r12, $0x0066
	jr	gt, 2f
	ld	r1, r12
	sub	r1, $0x0057
	jr	9f
2:
	cp	r12, $0x0041
	jr	lt, 3f
	cp	r12, $0x0046
	jr	gt, 3f
	ld	r1, r12
	sub	r1, $0x0037
	jr	9f
3:
	ld	r1, $0xffff
9:
	ldl	rr12, @rr14
	inc	r15, $4
	ret

/ hrline() -- line input with '*' prompt, backspace, and echo.
hrline_:
	dec	r15, $8
	ldm	@rr14, r10, $4
	ld	r13, r15
prompt:
	push	@rr14, $0x002a
	call	putchar_
	inc	r15, $2
	ldl	rr0, $ddtbss_
setcur:
	ldl	ddtvars_+0x30, rr0
getkey:
	call	getchar_
	ld	ddttmp_, r1
	cp	ddttmp_, $0x0008
	jr	nz, 1f
	ldl	rr0, ddtvars_+0x30
	cpl	rr0, $ddtbss_
	jr	ule, getkey
	ldl	rr0, ddtvars_+0x30
	dec	r1, $1
	jr	setcur
1:
	cp	ddttmp_, $0x000a
	jr	nz, 2f
	ld	r1, ddttmp_
	ldl	rr10, ddtvars_+0x30
	ldb	@rr10, rl1
	jr	9f
2:
	cp	ddttmp_, $0x0020
	jr	ge, 3f
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	jr	prompt
3:
	ldl	rr0, ddtvars_+0x30
	cpl	rr0, $ddtbss_+0xff
	jr	nc, getkey
	ldl	rr10, ddtvars_+0x30
	add	r11, $0x0001
	ldl	ddtvars_+0x30, rr10
	sub	r11, $0x0001
	ld	r1, ddttmp_
	ldb	@rr10, rl1
	jr	getkey
9:
	ldm	r10, @rr14, $4
	inc	r15, $8
	ret

/ outstr(s) -- print a NUL-terminated string.
outstr_:
	dec	r15, $12
	ldm	@rr14, r8, $6
	ld	r13, r15
	ldl	rr10, SS|0x10(r13)
1:
	ldl	rr8, rr10
	inc	r11, $1
	ldb	rl1, @rr8
	extsb	r1
	ld	r12, r1
	test	r1
	jr	z, 9f
	push	@rr14, r12
	call	putchar_
	inc	r15, $2
	jr	1b
9:
	ldm	r8, @rr14, $6
	inc	r15, $12
	ret
