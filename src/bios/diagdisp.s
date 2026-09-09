/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ Diagnostic display and keyboard decode.
	.globl	hrdecode_
	.globl	hrput_
	.globl	bvidchr_

SS	= 0x3F3F

/ hrdecode(scancode) -- keyboard scancode to character decode.
/ table.
hrdecode_:
	dec	r15, $10
	ldm	@rr14, r9, $5
	ld	r13, r15
	ld	r10, SS|0x0e(r13)
	cp	r10, $0x00ff
	jp	z, 8f
	ld	r1, r10
	and	r1, $0x007f
	dec	r1, $1
	ld	r12, r1
	ldb	rl1, kbdtype(r12)
	subb	rh1, rh1
	ld	r11, r1
	.byte	0xa7, 0xb7
	.byte	0xe6, 0x73
	bit	r10, $7
	jr	z, press
	cp	r12, $0x0035
	jr	nz, 1f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0xfffe
	.byte	0xe8, 0x63
1:
	ldb	rl1, msg_rparen
	extsb	rh1
	ld	r9, r1
	cp	r12, r9
	jr	nz, 2f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0xfffd
	jr	setmod
2:
	cp	r12, $0x001c
	jr	nz, 3f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0xfffb
	jr	setmod
3:
	cp	r12, $0x0037
	jp	nz, 8f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0xfff7
	jr	setmod
press:
	ldb	rl1, msg_rparen
	extsb	rh1
	ld	r9, r1
	cp	r12, r9
	jr	nz, 4f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	or	r1, $0x0002
	jr	setmod
4:
	cp	r12, $0x0035
	jr	nz, 5f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	or	r1, $0x0001
	jr	setmod
5:
	cp	r12, $0x001c
	jr	nz, 6f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	or	r1, $0x0004
	jr	setmod
6:
	cp	r12, $0x0037
	jr	nz, 7f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	or	r1, $0x0008
	jr	setmod
7:
	cp	r12, $0x0039
	jr	nz, 1f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	xor	r1, $0x0010
	jr	setmod
1:
	cp	r12, $0x0044
	jr	nz, 8f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	xor	r1, $0x0020
setmod:
	ldb	ddtbss_+0x104, rl1
	jr	8f
normal:
	bit	r10, $7
	jr	nz, 8f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	bit	r1, $2
	jr	z, 3f
	cp	r11, $0x0007
	jr	z, 2f
	cp	r11, $0x0017
	jr	nz, 8f
2:
	ldb	rl1, kbdshift(r12)
	subb	rh1, rh1
	and	r1, $0x001f
	jr	2f
3:
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	ld	r9, r1
	and	r11, r9
	jr	z, 6f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0x0003
	jr	z, 4f
	ld	r1, r11
	and	r1, $0x0030
	jr	nz, 6f
	jr	5f
4:
	ld	r1, r11
	and	r1, $0x0030
	jr	z, 6f
5:
	ldb	rl1, kbdshift(r12)
	jr	7f
6:
	ldb	rl1, kbdmap(r12)
7:
	subb	rh1, rh1
2:
	ld	r12, r1
	cp	r12, $0x00ff
	jr	nz, 1f
8:
	sub	r1, r1
	jr	9f
1:
	cp	r12, $0x00fe
	jr	z, 3f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	bit	r1, $3
	jr	z, 4f
	cp	r12, $0x001b
	jr	nz, 4f
	ld	r12, $0x008f
4:
	ld	r1, r12
	jr	9f
3:
	push	@rr14, r10
	call	padkey
	inc	r15, $2
9:
	ldm	r9, @rr14, $5
	inc	r15, $10
	ret
padkey:
	dec	r15, $6
	ldm	@rr14, r11, $3
	ld	r13, r15
	ld	r12, SS|0x0a(r13)
	ld	r11, r12
	ld	r1, r12
	jr	dispatch
/ ScrollLock -> DEL.
paddel:
	ld	r1, $0x007f
	jr	9f
/ Keypad rows: shift the index down to the 1/2/3 row, then translate
/ through kbdtype2 with the NumLock/shift tests below.
padrow1:
	dec	r11, $1
padrow4:
	dec	r11, $1
padrow7:
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	bit	r1, $5
	jr	nz, 8f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0x0003
	jr	nz, 8f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	bit	r1, $5
	jr	nz, 1f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	and	r1, $0x0003
	jr	nz, 1f
	ldb	rl1, ddtbss_+0x104
	extsb	rh1
	bit	r1, $6
	jr	nz, 8f
1:
	ldb	rl1, kbdtype2(r11)
	extsb	rh1
	ld	r11, r1
	jr	7f
8:
padnul:
	sub	r11, r11
	jr	7f
dispatch:
	sub	r1, $0x003b
	cp	r1, $0x0018
	jr	ugt, 8b
	add	r1, r1
	add	r1, r1
	ldl	rr2, padtab(r1)
	jp	@rr2
/ padtab -- keypad/function-key dispatch, indexed by scancode-0x3b
/ (F1-F10 pass through; NumLock and +/- give NUL; ScrollLock gives DEL;
/ the rest select a keypad row).
padtab:
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padret
	.long	padnul
	.long	paddel
	.long	padrow7
	.long	padrow7
	.long	padrow7
	.long	padnul
	.long	padrow4
	.long	padrow4
	.long	padrow4
	.long	padnul
	.long	padrow1
	.long	padrow1
	.long	padrow1
	.long	padrow1
	.long	padrow1
7:
padret:
	ld	r1, r11
9:
	ldm	r11, @rr14, $3
	inc	r15, $6
	ret

/ hrput(c) -- hi-res console char put: save regs, return address in rr10
/ (altret_), load lo-res cursor + char, fall into bvidchr.
hrput_:
	sub	r15, $0x0020
	ldm	@r15, r0, $16
	lda	rr10, altret_
	ldl	rr12, dbgfmt_+0x12
	ld	r1, SS|0x24(r15)

/ bvidchr -- lo-res 6845 renderer body: cursor rr12==0 -> init 6845 from
/ crtctab + clear screen; then render char (LF/CR/FF/BS/NUL/printable),
/ scroll, write 6845 cursor regs; exits jp @rr10.
bvidchr_:
	cpl	rr12, $0x00000000
	jr	nz, dochar
	clr	r0
	outb	0x0418, rl0
	ldl	rr2, $0x3a000000
	ld	@rr2, $0x0720
	ldl	rr4, rr2
	add	r5, $0x0002
	ld	r8, $0x07d0
	ldir	@rr4, @rr2, r8
	clrb	rh1
	lda	rr4, crtctab
	ld	r3, crtclen
	srl	r3, $1
1:
	ldb	rl0, @rr4
	inc	r5, $1
	outb	0x0410, rl0
	ldb	rl0, @rr4
	inc	r5, $1
	outb	0x0412, rl0
	dec	r3, $1
	jr	nz, 1b
	ldb	rl5, $0x28
	outb	0x0418, rl5
dochar:
	ld	r9, r3
	cpb	rl1, $0x0a
	jr	z, lf
	cpb	rl1, $0x0d
	jr	z, setcur
	cpb	rl1, $0x0c
	jr	z, clear
	cpb	rl1, $0x08
	jr	z, bs
	cpb	rl1, $0x00
	jr	z, setcur
	ld	r3, $0x0050
	ld	r5, r12
	sla	r5, $1
	mult	rr4, r3
	add	r5, r13
	add	r5, r13
	.byte	0x6e, 0x59, 0xba, 0x00, 0x00, 0x00
	cp	r13, $0x004f
	jr	ge, 2f
	inc	r13, $1
2:
	jr	setcur
lf:
	cp	r12, $0x0018
	jr	ge, scroll
	inc	r12, $1
	clr	r13
	jr	setcur
scroll:
	ldl	rr2, $0x3a0000a0
	ldl	rr4, $0x3a000000
	ld	r8, $0x0780
	ldir	@rr4, @rr2, r8
	ldl	rr2, $0x3a000f00
	ld	r8, $0x0050
	ldl	rr4, rr2
	inc	r5, $2
	ld	@rr2, $0x0720
	ldir	@rr4, @rr2, r8
	ldk	r13, $0x0
setcur:
	ld	r3, $0x0050
	ld	r5, r12
	sla	r5, $1
	mult	rr4, r3
	add	r5, r13
	add	r5, r13
	cp	r13, $0x0050
	jr	ge, 9f
	sra	r5, $1
	ldb	rh1, $0x0e
	outb	0x0410, rh1
	outb	0x0412, rh5
	incb	rh1, $1
	outb	0x0410, rh1
	outb	0x0412, rl5
9:
	jp	@rr10
clear:
	ldl	rr12, $0x00000000
	jr	9b
bs:
	test	r13
	jr	z, 9b
	ldb	rl1, 0x0020
	dec	r13, $1
	ld	r3, $0x0050
	ld	r5, r12
	sla	r5, $1
	mult	rr4, r3
	add	r5, r13
	add	r5, r13
	.byte	0x6e, 0x59, 0xba, 0x00, 0x00, 0x00
	jr	setcur
crtctab:
	.byte	0x00, 0x61, 0x01, 0x50, 0x02, 0x52, 0x03, 0x0f
	.byte	0x04, 0x19, 0x05, 0x06, 0x06, 0x19, 0x07, 0x19
	.byte	0x08, 0x02, 0x09, 0x0d, 0x0a, 0x0b, 0x0b, 0x0c
	.byte	0x0c, 0x00, 0x0d, 0x00, 0x0e, 0x00, 0x0f, 0x00
crtclen:
	.byte	0x00, 0x20

.prvd
msg_rparen:
	.byte	0x29, 0x00
kbdmap:
	.byte	0x1b
	.ascii	"1234567890-="
	.byte	0x08
	.ascii	"\tqwertyuiop[]"
	.byte	0x0d, 0xff
	.ascii	"asdfghjkl;'`"
	.byte	0xff
	.ascii	"\\zxcvbnm,./"
	.byte	0xff, 0x2a, 0xff, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	.byte	0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0x2d, 0xfe, 0xfe, 0xfe
	.byte	0x2b, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff
	.byte	0xff, 0xff, 0xff, 0xff, 0x0d, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
kbdshift:
	.byte	0x1b
	.ascii	"!@#$%^&*()_+"
	.byte	0x08
	.ascii	"\tQWERTYUIOP{}"
	.byte	0x0d, 0xff
	.ascii	"ASDFGHJKL:\"~"
	.byte	0xff
	.ascii	"|ZXCVBNM<>?"
	.byte	0xff, 0x2a, 0xff, 0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	.byte	0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xfe, 0xfe, 0x2d, 0xfe, 0xfe, 0xfe
	.byte	0x2b, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff
	.byte	0xff, 0xff, 0xff, 0xff, 0x0d, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00
kbdtype:
	.byte	0x00, 0x03, 0x07, 0x03, 0x03, 0x03, 0x07, 0x03, 0x03, 0x03, 0x03, 0x07
	.byte	0x03, 0x00, 0x00, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17
	.byte	0x17, 0x07, 0x07, 0x00, 0x80
kbdtype2:
	.byte	0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x03, 0x03, 0x07
	.byte	0x80, 0x07, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x03, 0x03, 0x03
	.byte	0x80, 0x03, 0x80, 0x07, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	.byte	0x00, 0x00, 0x00, 0x80
	.ascii	"cccc"
	.byte	0x00, 0x63, 0x63, 0x63, 0x00
	.ascii	"ccccc"
	.blkb	17
paddigits:
	.ascii	"7894561230."
	.byte	0x00
