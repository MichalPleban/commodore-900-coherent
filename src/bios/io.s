/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ Low-level I/O and machine glue.

SS	= 0x3F3F

	.globl	outstr_
	.globl	ddtmsgs_
	.globl	putchar_
	.globl	inb_
	.globl	outb_
	.globl	pmsg_
	.globl	scmp_
	.globl	multmic_
	.globl	khalt_
	.globl	sphi_
	.globl	spl_
	.globl	rdport_
	.globl	wrport_

/ pmsg(idx) -- print a newline, message-table entry idx, and a newline.
pmsg_:
	dec	r15, $4
	ldl	@rr14, rr12
	ld	r13, r15
	ldl	rr0, $ddtmsgs_+0xa3
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	ld	r12, SS|0x08(r13)
	sla	r12, $2
	pushl	@rr14, ddtscr_+0x06(r12)
	call	outstr_
	inc	r15, $4
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	ldl	rr12, @rr14
	inc	r15, $4
	ret

/ scmp(a, b) -- compare two strings; return 1 if equal up to a NUL.
scmp_:
	dec	r15, $16
	ldm	@rr14, r6, $8
	ld	r13, r15
	ldl	rr10, SS|0x14(r13)
	ldl	rr8, SS|0x18(r13)
1:
	ldl	rr6, rr8
	inc	r9, $1
	ldb	rl5, @rr6
	extsb	r5
	ldl	rr6, rr10
	inc	r11, $1
	ldb	rl1, @rr6
	extsb	r1
	ld	r12, r1
	cp	r1, r5
	jr	nz, 2f
	test	r12
	jr	nz, 1b
	ldk	r1, $1
	jr	9f
2:
	sub	r1, r1
9:
	ldm	r6, @rr14, $8
	inc	r15, $16
	ret

/ multmic() -- test the multi-micro input line: 0 if high, else 0xff.
multmic_:
	clr	r1
	mbit
	ret	pl
	ldb	rl1, $0xff
	ret

/ khalt() -- halt; on wake clear the abort flag and return.
khalt_:
	halt
	ldb	abflags_, $0x00
	ldb	abflags_, $0x00
	ldb	abflags_, $0x00
	ret

/ sphi()/spl() -- set priority high (disable VI) / low (enable VI).
sphi_:
	di	VI
	ret
spl_:
	ei	VI
	ret

/ rdport() -- read the front panel port.
rdport_:
	dec	r15, $4
	ld	@rr14, r13
	ld	r13, r15
	push	@rr14, $0x001d
	call	inb_
	inc	r15, $2
	ldb	SS|0x02(r13), rl1
	ldb	rl1, SS|0x02(r13)
	extsb	r1
	ld	r13, @rr14
	inc	r15, $4
	ret

/ wrport(data) -- write the front panel port.
wrport_:
	dec	r15, $2
	ld	@rr14, r13
	ld	r13, r15
	push	@rr14, SS|0x06(r13)
	push	@rr14, $0x001d
	call	outb_
	inc	r15, $4
	ld	r13, @rr14
	inc	r15, $2
	ret
