/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ Memory diagnostic: print the banner, size RAM, run the march
/ passes, report failures.
SS	= 0x3F3F

/ memdiag -- print an unsigned word in decimal.
	.globl	memdiag_
	.globl	coninit_
	.globl	ramtest_

memdiag_:
	sub	r15, $0x0014
	ldm	@rr14, r9, $5
	ld	r13, r15
	ld	r12, SS|0x18(r13)
	sub	r11, r11
1:
	ld	r9, r11
	inc	r11, $1
	ld	r1, r13
	add	r1, r9
	ld	r10, r1
	ld	r1, r12
	exts	rr0
	div	rr0, $0x000a
	add	r0, $0x0030
	ldb	SS|0x0a(r10), rl0
	ld	r1, r12
	exts	rr0
	div	rr0, $0x000a
	ld	r12, r1
	jr	gt, 1b
2:
	dec	r11, $1
	jr	lt, 9f
	ld	r1, r13
	add	r1, r11
	ld	r10, r1
	ldb	rl1, SS|0x0a(r10)
	extsb	rh1
	push	@rr14, r1
	call	putchar_
	inc	r15, $2
	jr	2b
9:
	ldm	r9, @rr14, $5
	add	r15, $0x0014
	ret
/ coninit -- (re)program the console SCC (base port 0x0100) from
/ contab, flag console ready (1|0x041e = 0xff), return via the porttest
/ register-restore tail (porttest rets_).
coninit_:
	sub	r15, $0x0020
	ldm	@r15, r0, $16
	lda	rr10, rets_
/ sccload -- shared tail: outib contab (conlen bytes) to the SCC, then
/ jump through rr10 (ramtest enters here with rr10 = flush).
sccload:
	ld	r1, $0x0100
	lda	rr4, contab
	ld	r3, conlen
	srl	r3, $1
1:
	ldb	rl1, @rr4
	inc	r5, $1
	outib	@r1, @r4, r3
	jr	po, 1b
	ldb	put_init_, $0xff
	jp	@rr10
/ ramtest -- memory-diagnostic entry.  Clear the console-type code
/ (NSPOFF = 0), load the SCC, then continue at flush.
ramtest_:
	clr	r0
	ldctl	NSPOFF, r0
	lda	rr10, flush
	jr	sccload
/ flush -- drain up to 10 stale SCC RX bytes, strobe the diagnostic ports
/ (POST display at 0x001d), then run the porttest SCC-init body (e_3ee2)
/ and continue at memtest via the gomem thunk.
flush:
	ldb	rh1, $0x0a
1:
	inb	rl1, 0x0101
	bitb	rl1, $0
	jr	z, 2f
	inb	rl1, 0x0111
	dbjnz	r1, 1b
2:
	ldk	r2, $0x1
	outb	0x0001, rl2
	clrb	rl2
	outb	0x0001, rl2
	out	0x0051, r2
	out	0x0057, r2
	out	0x001d, r2
	ldb	rl2, $0x80
	outb	0x0003, rl2
	lda	rr10, gomem
	jp	sccprog_
gomem:
	jp	memtest
/ contab -- console-SCC init byte pairs (register, value); conlen holds the
/ table byte count.
contab:
	.byte	0x13, 0xc0, 0x09, 0x4c, 0x05, 0x14, 0x07, 0xc0
	.byte	0x0b, 0xe2, 0x0d, 0x00, 0x0f, 0x00, 0x13, 0x00
	.byte	0x15, 0x00, 0x17, 0x56, 0x19, 0x11, 0x1b, 0x00
	.byte	0x1d, 0x02, 0x1d, 0x03, 0x07, 0xc1, 0x0b, 0xea
	.byte	0x1f, 0x00, 0x01, 0x10, 0x01, 0x10, 0x13, 0x01
	.byte	0x03, 0x00
/ conlen -- contab byte count word.  (local symbol).
conlen:
	.byte	0x00, 0x2a
/ banner -- "\r\nCommodore C900 diagnostics V1.0 (6/14/85)\r\nMemory test: "
banner:
	.byte	0x0d, 0x0a
	.ascii	"Commodore C900 diagnostics V1.0 (6/14/85)"
	.byte	0x0d, 0x0a
	.ascii	"Memory test: "
	.byte	0x00
/ errmsg -- "\r\nRAM ERROR   BANK:CHIP= "
errmsg:
	.byte	0x0d, 0x0a
	.ascii	"RAM ERROR   BANK:CHIP= "
	.byte	0x00, 0x00
/ memtest -- main test body.  Probe HR VRAM (seg 0x3e); if present
/ set console code NSPOFF=1 and map it, else try the LR board.
memtest:
	ld	r2, $0x3e00
	clr	r3
	ld	@rr2, $0xffff
	cp	@rr2, $0xffff
	jr	nz, trylr
	clr	@rr2
	cp	@rr2, $0x0000
	jr	nz, trylr
	ldk	r0, $0x1
	ldctl	NSPOFF, r0
	ldl	rr0, $0x3e00ff00
	jr	mapvid
/ trylr -- probe LR VRAM (seg 0x37); if present set NSPOFF=2 and clear the
/ LR cursor state, else fall through to the serial-console banner.
trylr:
	ld	r2, $0x3700
	clr	@rr2
	cp	@rr2, $0x0000
	jr	nz, sbanner
	ld	@rr2, $0xffff
	cp	@rr2, $0xffff
	jr	nz, sbanner
	ldk	r0, $0x2
	clr	dbgfmt_+0x12
	clr	dbgfmt_+0x14
	ldctl	NSPOFF, r0
	ldl	rr0, $0x3700ff00
/ mapvid -- program MMU descriptors (special-I/O 0x20fc/0x01fc/0x0ffc) for
/ the two video segments in rr0, then print the banner on the detected
/ console (avidchr_ / bvidchr_ / serial).
mapvid:
	ldb	rl2, $0x3a
	ldctl	r4, FCW
	di	VI
	soutb	0x20fc, rl0
	soutb	0x01fc, rl2
	soutb	0x0ffc, rh0
	soutb	0x0ffc, rl0
	soutb	0x0ffc, rh1
	soutb	0x0ffc, rl1
	incb	rh0, $1
	soutb	0x0ffc, rh0
	soutb	0x0ffc, rl0
	soutb	0x0ffc, rh1
	soutb	0x0ffc, rl1
	ldctl	FCW, r4
	ldctl	r0, NSPOFF
	cpb	rl0, $0x01
	jr	nz, bbanner
	ld	r12, $0x3a00
	clr	r13
	lda	rr14, banner
	lda	rr10, aputs
/ aputs -- HR banner put-string loop (avidchr_ returns through rr10).
aputs:
	ldb	rl1, @rr14
	inc	r15, $1
	testb	rl1
	jp	nz, avidchr_
/ bbanner -- print the banner via bvidchr_ if the LR console was detected.
bbanner:
	cpb	rl0, $0x02
	jr	nz, sbanner
	clr	r12
	clr	r13
	lda	rr6, banner
	lda	rr10, bputs
/ bputs -- LR banner put-string loop (bvidchr_ returns through rr10).
bputs:
	ldb	rl1, @rr6
	inc	r7, $1
	testb	rl1
	jp	nz, bvidchr_
/ sbanner -- no video board: print the banner over the console SCC.
sbanner:
	ldctl	r0, NSPOFF
	testb	rl0
	jr	nz, sizeram
	lda	rr10, banner
1:
	ldb	rl1, @rr10
	testb	rl1
	jr	z, sizeram
2:
	inb	rl0, 0x0101
	bitb	rl0, $2
	jr	z, 2b
	outb	0x0111, rl1
	inc	r11, $1
	jr	1b
/ sizeram -- find top of RAM: write/readback 0xaaaa at the base of each
/ segment from 0x08 upward until a segment fails to respond.
sizeram:
	lda	rr6, 0x08|0x0000
	ld	r9, $0xaaaa
1:
	ld	@rr6, r9
	ld	r4, @rr6
	cp	r4, r9
	jr	nz, chktop
	addb	rh6, $0x01
	jr	1b
/ chktop -- complement-verify the first failing segment (open bus, not a
/ stuck cell), require more than 8 good segments (top > seg 0x0f), keep the
/ top segment in r5, then background-fill segs 0x08..top with 0xaaaa.
chktop:
	com	@rr6
	cp	r4, @rr6
	jr	nz, ramerr
	cpb	rh6, $0x0f
	jr	le, ramerr
	ld	r5, r6
	lda	rr2, 0x07|0x0000
1:
	incb	rh2, $1
	cp	r2, r5
	jr	z, march1
	clr	r3
	ld	@rr2, $0xaaaa
	ldl	rr6, rr2
	add	r7, $0x0002
	ld	r8, $0x8000
	ldir	@rr6, @rr2, r8
	jr	1b
/ march1 -- pass 1: per segment write a 0x5555 marker word, then verify the
/ rest of RAM still reads the 0xaaaa background (m1cmp/m1step inner loop).
march1:
	lda	rr2, 0x07|0x0000
m1seg:
	incb	rh2, $1
	clr	r3
	cp	r2, r5
	jp	z, march2
	ld	@rr2, $0x5555
	ld	r9, $0xaaaa
	ldl	rr6, rr2
	add	r7, $0x0002
	ld	r8, $0x7fff
m1cmp:
	ld	r4, @rr6
	cp	r9, r4
	jp	z, m1step
/ ramerr -- report a RAM failure: find the failing bit (expected r9 vs read
/ r4), fold in the segment to a bank:chip code, post it on port 0x001d,
/ format two hex digits, print "RAM ERROR   BANK:CHIP= xx" on the active
/ console, then return to the monitor (e_0090).
ramerr:
	ld	r2, r9
	ld	r3, r4
	xor	r9, r4
	clr	r4
1:
	rrc	r9, $1
	jr	c, 2f
	inc	r4, $1
	jr	1b
2:
	ld	r0, r6
	srl	r0, $7
	add	r4, r0
	out	0x001d, r4
	sll	r4, $4
	srlb	rl4, $4
	addb	rh4, $0x30
	cpb	rh4, $0x39
	jr	le, 3f
	addb	rh4, $0x07
3:
	addb	rl4, $0x30
	cpb	rl4, $0x39
	jr	le, 4f
	addb	rl4, $0x07
4:
	ldctl	r0, NSPOFF
	ldb	rh0, $0x01
	ldctl	NSPOFF, r0
	cpb	rl0, $0x01
	jr	nz, berr
	ldctl	NSPSEG, r4
	lda	rr14, errmsg
	lda	rr10, aemsg
/ aemsg -- HR error-message put-string loop, then the two hex digits.
aemsg:
	ldb	rl1, @rr14
	inc	r15, $1
	testb	rl1
	jp	nz, avidchr_
	ldctl	r4, NSPSEG
	lda	rr10, aelo
	ldb	rl1, rh4
	jp	avidchr_
aelo:
	lda	rr10, aedone
	ldctl	r1, NSPSEG
	clrb	rh1
	jp	avidchr_
aedone:
	ldctl	r4, NSPSEG
/ berr -- LR console error report (bvidchr_), same shape as the HR path.
berr:
	cpb	rl0, $0x02
	jr	nz, serr
	ldctl	NSPSEG, r4
	lda	rr6, errmsg
	lda	rr10, bemsg
bemsg:
	ldb	rl1, @rr6
	inc	r7, $1
	testb	rl1
	jp	nz, bvidchr_
	ldctl	r4, NSPSEG
	lda	rr10, belo
	ldb	rl1, rh4
	jp	bvidchr_
belo:
	lda	rr10, bedone
	ldctl	r1, NSPSEG
	clrb	rh1
	jp	bvidchr_
bedone:
	ldctl	r4, NSPSEG
/ serr -- stash the (LR) cursor, then either return to the monitor (video
/ consoles already printed) or emit the error report over the SCC.
serr:
	ldctl	r0, NSPOFF
	cpb	rl0, $0x02
	jr	nz, errout
	sll	r12, $8
	add	r13, r12
errout:
	ldctl	NSPSEG, r13
	testb	rl0
	jp	nz, sizeram_
	lda	rr10, errmsg
1:
	ldb	rl1, @rr10
	testb	rl1
	jr	z, 3f
2:
	inb	rl0, 0x0101
	bitb	rl0, $2
	jr	z, 2b
	outb	0x0111, rl1
	inc	r11, $1
	jr	1b
3:
	inb	rl0, 0x0101
	bitb	rl0, $2
	jr	z, 3b
	outb	0x0111, rh4
4:
	inb	rl0, 0x0101
	bitb	rl0, $2
	jr	z, 4b
	outb	0x0111, rl4
5:
	inb	rl0, 0x0101
	bitb	rl0, $2
	jr	z, 5b
	ldb	rl4, $0x0d
	outb	0x0111, rl4
6:
	inb	rl0, 0x0101
	bitb	rl0, $2
	jr	z, 6b
	ldb	rl4, $0x0a
	outb	0x0111, rl4
	jp	sizeram_
/ m1step -- march-pass-1 helper: skip the block just verified, resume the
/ compare (m1cmp) or advance to the next segment (m1seg).
m1step:
	ldi	@rr6, @rr2, r8
	jp	po, m1cmp
	jp	m1seg
/ march2 -- pass 2: per segment write a 0xaaaa marker word, verify the rest
/ of RAM still reads the 0x5555 markers left by pass 1.
march2:
	lda	rr2, 0x07|0x0000
1:
	incb	rh2, $1
	clr	r3
	cp	r2, r5
	jp	z, pass
	ld	@rr2, $0xaaaa
	ld	r9, $0x5555
	ldl	rr6, rr2
	add	r7, $0x0002
	ld	r8, $0x7fff
2:
	ld	r4, @rr6
	cp	r9, r4
	jp	nz, ramerr
	ldi	@rr6, @rr2, r8
	jr	po, 2b
	jr	1b
/ pass -- memory test passed: restore the cursor, post success code 0x0090
/ on port 0x001d, return to the monitor (e_0090).
pass:
	ldctl	r0, NSPOFF
	cpb	rl0, $0x02
	jr	nz, 1f
	sll	r12, $8
	add	r13, r12
1:
	ldctl	NSPSEG, r13
	ld	r2, $0x0090
	out	0x001d, r2
	jp	sizeram_
