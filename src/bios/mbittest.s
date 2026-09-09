/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ Chip initialization and keyboard/serial poll.
	.globl	chipinit_
	.globl	rets_
	.globl	sccprog_
	.globl	serkey_
	.globl	hrkey_

SS	= 0x3F3F

sccinit:
	.byte	0x41, 0x1a, 0x43, 0x00, 0x45, 0x00, 0x47, 0xff
	.byte	0x49, 0x00, 0x4b, 0x80, 0x4d, 0x00, 0x4f, 0x80
	.byte	0x05, 0x08, 0x11, 0xe0, 0x0b, 0x00, 0x0d, 0x07
	.byte	0x0f, 0x00, 0x1f, 0x70
scclen:	.word	0x001c
retv:
	ld	r15(0x0002), r1
rets_:
	ldm	r0, @r15, $16
	add	r15, $0x0020
	ret
chipinit_:
	sub	r15, $0x0020
	ldm	@r15, r0, $16
	lda	rr10, rets_
sccprog_:
	ldk	r1, $0x0
	lda	rr4, sccinit
	ld	r3, scclen
	srl	r3, $1
1:
	ldb	rl1, @rr4
	inc	r5, $1
	outib	@r1, @r4, r3
	jr	po, 1b
	inb	rl1, 0x0003
	orb	rl1, $0x14
	outb	0x0003, rl1
	ldb	rl1, $0x02
	outb	0x0205, rl1
	clrb	rl1
	outb	0x001f, rl1
	ldb	rl1, $0x08
	outb	0x001f, rl1
	jp	@rr10
/ hrkey() -- poll the keyboard for a scancode; 0 if none.
hrkey_:
	sub	r15, $0x0020
	ldm	@r15, r0, $16
	lda	rr10, retv
serkey_:
	inb	rl1, 0x0011
	andb	rl1, $0x02
	jr	z, 2f
	ldb	rl1, $0x20
	outb	0x0011, rl1
	inb	rl1, 0x001b
	andb	rl1, $0x7f
	inb	rh1, 0x001f
	andb	rh1, $0x04
	jr	z, 1f
	orb	rl1, $0x80
1:
	clrb	rh1
	outb	0x001f, rh1
	ldb	rh1, $0x08
	outb	0x001f, rh1
2:
	clrb	rh1
	jp	@rr10
