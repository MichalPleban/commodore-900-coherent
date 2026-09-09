/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ The resident DDT monitor/debugger, and the alternate-console
/ character renderer.
SS	= 0x3F3F

	.globl	splx_
	.globl	debugger_
	.globl	altput_
	.globl	avidchr_
	.globl	debugcmd_
	.globl	mmuread_
	.globl	altret_
	.globl	mmuwrite_
	.globl	mkfp_
	.globl	splhi_
	.globl	regtab_
	.globl	regtabl_
	.globl	ddtline_
	.globl	cmdtab_
	.globl	ddtscr_
	.globl	ddtmsgs_

/ splx -- restore FCW from arg (interrupt-level exit).
splx_:
	ldctl	r1, FCW
	ld	r0, SS|0x04(r15)
	ldctl	FCW, r0
	ret
/ splhi -- save FCW, mask VI.
splhi_:
	ldctl	r1, FCW
	di	VI
	ret
/ mmuread(buf,n,reg) -- stream MMU descriptors out.
mmuread_:
	ldl	rr0, SS|0x04(r15)
	ldl	rr2, SS|0x08(r15)
	add	r1, r1
	add	r1, r1
	soutb	0x01fc, rl0
	soutb	0x20fc, rh0
	ld	r4, $0x0ffc
	sinirb	@rr2, @r4, r1
	ret
/ debugger -- 2-byte monitor trampoline: trap into DDT via SC #0xfa.
debugger_:
	sc	0xfa
	ret
/ altret -- avidchr epilogue: stash cursor, restore regs
altret_:
	ldl	dbgfmt_+0x12, rr12
	ldm	r0, @r15, $16
	add	r15, $0x0020
	ret
/ altput -- avidchr per-char prologue: save regs, load cursor rr12.
altput_:
	sub	r15, $0x0020
	ldm	@r15, r0, $16
	ldl	rr12, dbgfmt_+0x12
	lda	rr10, altret_
	ld	r1, SS|0x24(r15)
/ avidchr -- alternate-console bitmap character renderer body (VRAM
/ segs 0x3a/0x3b, font glyphs from hrfont_).
avidchr_:
	clr	r0
	cpl	rr12, $0x3a000000
	jr	nz, chrloop
	ldl	rr2, $0x3a000000
	ld	@rr2, $0x0000
	ldl	rr4, rr2
	add	r5, $0x0002
	ld	r8, $0x8000
	ldir	@rr4, @rr2, r8
	ldl	rr2, $0x3b000000
	ld	@rr2, $0x0000
	ldl	rr4, rr2
	add	r5, $0x0002
	ld	r8, $0x4800
	ldir	@rr4, @rr2, r8
	ldb	rl5, $0xff
	outb	0x0404, rl5
/ chrloop -- avidchr state machine: dispatch on char/state r0.
chrloop:
	ld	r9, r3
	cpb	rl1, $0x0a
	jr	z, is_nl
	cpb	rl1, $0x0d
	jp	z, is_cr
	cpb	rl1, $0x0c
	jp	z, is_ff
	cpb	rl1, $0x08
	jp	z, is_bs
	cpb	rl1, $0x00
	jp	z, is_cr
	cp	r0, $0x0003
	jr	nz, glyph
	ldl	rr4, rr12
	andb	rl5, $0x7f
	cpb	rl5, $0x7e
	jp	z, is_cr
/ glyph -- render a printable char: copy 32 font rows into VRAM.
glyph:
	clr	r4
	ld	r5, $0x0042
	clrb	rh1
	mult	rr4, r1
	inc	r5, $10
	ld	r2, r5
	lda	rr4, hrfont_
	add	r5, r2
	ldl	rr2, rr12
	ld	r8, $0x0020
1:
	ldi	@rr2, @rr4, r8
	addl	rr2, $0x0000007e
	cp	r8, $0x0000
	jr	nz, 1b
	cp	r0, $0x0001
	jr	z, is_nl
	cp	r0, $0x0002
	jp	z, bsback
	ld	r5, r13
	andb	rl5, $0x7f
	cpb	rl5, $0x7e
	jr	z, is_cr
	inc	r13, $2
	jr	is_cr
/ is_nl -- newline: blank to cursor, advance a text row.
is_nl:
	cp	r0, $0x0001
	jr	z, advline
	ld	r5, r13
	andb	rl5, $0x7f
	cpb	rl5, $0x7e
	jr	z, advline
	ldk	r0, $0x1
	ldb	rl1, $0x20
	jr	chrloop
/ advline -- step cursor to the next row; wrap/scroll at bank end.
advline:
	clr	r0
	ldl	rr4, rr12
	and	r5, $0xf000
	cpl	rr4, $0x3a00f000
	jr	z, bankb
	cpl	rr4, $0x3b008000
	jr	z, scroll
	ldb	rl2, rh5
	clrb	rh2
	add	r2, $0x0010
	ldb	rh5, rl2
	ldl	rr12, rr4
	jr	is_cr
/ bankb -- bank A exhausted: continue in VRAM bank B (seg 0x3b).
bankb:
	ldl	rr12, $0x3b000000
	jr	is_cr
/ scroll -- scroll both VRAM banks up one row, blank the last.
scroll:
	ldl	rr2, $0x3a001000
	ldl	rr4, $0x3a000000
	ld	r8, $0x7800
	ldir	@rr4, @rr2, r8
	ldl	rr2, $0x3b000000
	ld	r8, $0x0800
	ldir	@rr4, @rr2, r8
	ldl	rr4, $0x3b000000
	ld	r8, $0x4000
	ldir	@rr4, @rr2, r8
	clr	r2
	ld	r8, $0x0800
	ld	@rr4, r2
	ldl	rr2, rr4
	add	r5, $0x0002
	ldir	@rr4, @rr2, r8
	ldl	rr12, $0x3b008000
/ is_cr -- common per-char tail: redraw the cursor, return.
is_cr:
	ld	r3, r9
	cp	r0, $0x0003
	jr	z, avdone
/ drawcur -- draw the '_' cursor glyph (state 3).
drawcur:
	ldl	rr6, rr12
	ldk	r0, $0x3
	ldb	rl1, $0x5f
	jp	chrloop
/ avdone -- restore cursor rr12, return via rr10 (altret).
avdone:
	clr	r0
	ldl	rr12, rr6
	jp	@rr10
/ is_ff -- form feed: home to bank A start and clear the screen.
is_ff:
	ldl	rr12, $0x3a000000
	ldb	rl1, $0x00
	jp	avidchr_
/ is_bs -- backspace: erase previous char (state 2).
is_bs:
	ld	r5, r13
	andb	rl5, $0x7f
	cpb	rl5, $0x00
	jr	z, is_cr
	ldk	r0, $0x2
	ldb	rl1, $0x20
	jp	chrloop
/ bsback -- after erase, step the cursor back one char cell.
bsback:
	clr	r0
	sub	r13, $0x0002
	ld	r3, r9
	jr	drawcur
/ mmuwrite(seg,desc) -- SOTIRB-stream a descriptor into the MMU
mmuwrite_:
	ld	r1, SS|0x04(r15)
	ldl	rr2, SS|0x06(r15)
	sub	r0, r0
	ldctl	r4, FCW
	di	VI
	soutb	0x01fc, rl1
	soutb	0x20fc, rl0
	ldk	r0, $0x4
	ld	r1, $0x0ffc
	sotirb	@r1, @rr2, r0
	ldctl	FCW, r4
	ret
/ putlb(c) -- stash a char in the ASCII-gutter line buffer
/, '.' for non-printables.
putlb:
	dec	r15, $4
	ldl	@rr14, rr12
	ld	r13, r15
	ld	r1, ddtvars_+0x0e
	inc	ddtvars_+0x0e, $1
	ld	r12, r1
	cp	SS|0x08(r13), $0x0020
	jr	lt, 1f
	cp	SS|0x08(r13), $0x007e
	jr	gt, 1f
	ld	r1, SS|0x08(r13)
	jr	2f
1:
	ld	r1, $0x002e
2:
	ldb	ddtvars_+0x10(r12), rl1
	ld	r12, ddtvars_+0x0e
	clrb	ddtvars_+0x10(r12)
	ldl	rr12, @rr14
	inc	r15, $4
	ret
/ flushlb -- print the ASCII-gutter buffer with a leading space.
flushlb:
	dec	r15, $2
	ld	@rr14, r13
	ld	r13, r15
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	ldl	rr0, $ddtvars_+0x10
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	ld	r13, @rr14
	inc	r15, $2
	ret
/ menu -- print the help lines for '?'.
menu:
	dec	r15, $6
	ldl	@rr14, rr12
	ld	r13, r15
	clr	SS|0x04(r13)
	jr	2f
1:
	inc	SS|0x04(r13), $1
2:
	cp	SS|0x04(r13), $0x0012
	jr	ge, 9f
	ld	r12, SS|0x04(r13)
	sla	r12, $2
	pushl	@rr14, cmdtab_(r12)
	call	outstr_
	inc	r15, $4
	jr	1b
9:
	ldl	rr12, @rr14
	inc	r15, $6
	ret
/ gettok -- parse next token from the line buffer: hex value or
/ 'X char literal; 0xffff = end of line, 0xfffe = bad token.
gettok:
	dec	r15, $12
	ldm	@rr14, r8, $6
	ld	r13, r15
1:
	ldl	rr8, ddtvars_+0x30
	cpb	@rr8, $0x20
	jr	nz, 2f
	ldl	rr8, ddtvars_+0x30
	add	r9, $0x0001
	ldl	ddtvars_+0x30, rr8
	sub	r9, $0x0001
	ldb	rh1, @rr8
	jr	1b
2:
	ldl	rr8, ddtvars_+0x30
	cpb	@rr8, $0x0a
	jr	nz, 3f
	ld	r1, $0xffff
	jr	9f
3:
	ldl	rr8, ddtvars_+0x30
	ldb	rl1, @rr8
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jr	pl, 4f
	ldl	rr8, ddtvars_+0x30
	cpb	@rr8, $0x27
	jr	z, 4f
	ld	r1, $0xfffe
	jr	9f
4:
	sub	r12, r12
	ldl	rr8, ddtvars_+0x30
	cpb	@rr8, $0x27
	jr	nz, 5f
	ldl	rr8, ddtvars_+0x30
	add	r9, $0x0001
	ldl	ddtvars_+0x30, rr8
	sub	r9, $0x0001
	ldb	rh1, @rr8
	ldl	rr8, ddtvars_+0x30
	ldb	rl1, @rr8
	extsb	rh1
	sla	r1, $8
	ld	r12, r1
	ldl	rr8, ddtvars_+0x30
	add	r9, $0x0001
	ldl	ddtvars_+0x30, rr8
	sub	r9, $0x0001
	ldb	rh1, @rr8
	ldl	rr8, ddtvars_+0x30
	add	r9, $0x0001
	ldl	ddtvars_+0x30, rr8
	sub	r9, $0x0001
	ldb	rl1, @rr8
	extsb	rh1
	ld	r10, r1
	ld	r1, r12
	add	r1, r10
	ld	r12, r1
	jr	6f
5:
	ldl	rr8, ddtvars_+0x30
	add	r9, $0x0001
	ldl	ddtvars_+0x30, rr8
	sub	r9, $0x0001
	ldb	rl1, @rr8
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	ld	r11, r1
	test	r1
	jr	mi, 6f
	ld	r1, r12
	sla	r1, $4
	add	r1, r11
	ld	r12, r1
	jr	5b
6:
	ldl	rr0, ddtvars_+0x30
	dec	r1, $1
	ldl	ddtvars_+0x30, rr0
	ld	r1, r12
9:
	ldm	r8, @rr14, $6
	inc	r15, $12
	ret
/ getaddr -- parse an address: "seg|off" (also '\'), '.' = the saved
/ dot, "@regname" = saved register value.  Saves the seg part;
/ returns offset, 0xffff = empty, 0xfffe = bad.
getaddr:
	sub	r15, $0x001a
	ldm	@rr14, r6, $8
	ld	r13, r15
1:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 2f
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rh1, @rr6
	jr	1b
2:
	ldl	rr8, ddtvars_+0x30
3:
	ldb	rl1, @rr8
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	ld	r12, r1
	test	r1
	jr	mi, 4f
	ldl	rr6, rr8
	inc	r9, $1
	ldb	rh1, @rr6
	jr	3b
4:
	cpb	@rr8, $0x20
	jr	nz, 5f
	ldl	rr6, rr8
	inc	r9, $1
	ldb	rh1, @rr6
	jr	4b
5:
	cpb	@rr8, $0x7c
	jr	z, 6f
	cpb	@rr8, $0x5c
	jr	nz, getoff
6:
	call	gettok
	ld	ddtscr_, r1
	cp	ddtscr_, $0xfffe
	jp	z, badadr
	ld	r1, ddtscr_
	sla	r1, $8
	ld	ddtscr_, r1
	ldl	rr0, rr8
	inc	r1, $1
	ldl	rr8, rr0
	ldl	ddtvars_+0x30, rr0
/ getoff -- offset part: '.' recalls the saved dot.
getoff:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x2e
	jr	nz, getreg
	ld	r1, ddtscr_+0x02
	ld	ddtscr_, r1
	ldl	rr0, ddtvars_+0x30
	inc	r1, $1
	ldl	ddtvars_+0x30, rr0
	call	gettok
	ld	r7, r1
	ld	r1, ddtscr_+0x04
	add	r1, r7
	jp	retval
/ getreg -- '@regname': collect the register name.
getreg:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x40
	.byte	0xee, 0x7c
	ldl	rr6, ddtvars_+0x30
	inc	r7, $1
	ldl	ddtvars_+0x30, rr6
	ldb	rl1, @rr6
	extsb	rh1
	ld	ddttmp_, r1
	lda	rr0, SS|0x10(r13)
	and	r0, $0x7f00
	ldl	ddtvars_+0x3e, rr0
7:
	cp	ddttmp_, $0x0020
	jr	z, regfind
	cp	ddttmp_, $0x000a
	jr	z, regfind
	lda	rr6, SS|0x19(r13)
	and	r6, $0x7f00
	ldl	rr0, ddtvars_+0x3e
	cpl	rr0, rr6
	jr	nc, 8f
	ldl	rr6, ddtvars_+0x3e
	add	r7, $0x0001
	ldl	ddtvars_+0x3e, rr6
	sub	r7, $0x0001
	ld	r1, ddttmp_
	ldb	@rr6, rl1
8:
	ldl	rr6, ddtvars_+0x30
	inc	r7, $1
	ldl	ddtvars_+0x30, rr6
	ldb	rl1, @rr6
	extsb	rh1
	ld	ddttmp_, r1
	jr	7b
/ regfind -- look the name up in the alias table.
regfind:
	ldl	rr6, ddtvars_+0x3e
	clrb	@rr6
	ldl	rr10, $regtabl_
	jr	2f
1:
	inc	r11, $6
2:
	cpl	rr10, $ddtline_
	jr	nc, 3f
	lda	rr0, rr10(0x0001)
	and	r0, $0x7f00
	pushl	@rr14, rr0
	lda	rr0, SS|0x10(r13)
	and	r0, $0x7f00
	pushl	@rr14, rr0
	call	scmp_
	inc	r15, $8
	test	r1
	jr	z, 1b
3:
	cpl	rr10, $ddtline_
	jr	nz, 4f
/ badadr -- bad address -> return 0xfffe.
badadr:
	ld	r1, $0xfffe
	jr	9f
4:
	ldb	rl5, @rr10
	extsb	rh5
	add	r5, r5
	ldl	rr6, ldrbuf_
	add	r7, r5
	ld	r1, @rr6
	and	r1, $0x7fff
	ld	ddtscr_, r1
	ldb	rl5, @rr10
	extsb	rh5
	add	r5, r5
	inc	r5, $2
	ldl	rr6, ldrbuf_
	add	r7, r5
	ld	r1, @rr6
	jr	retval
/ getval -- (unreferenced) plain-value parse entry.
getval:
	call	gettok
/ retval -- common return: r1 = parsed offset.
retval:
	ld	r12, r1
	ld	r1, r12
9:
	ldm	r6, @rr14, $8
	add	r15, $0x001a
	ret
/ mkfp(seg,off) -- combine seg:off into a 32-bit far pointer
mkfp_:
	dec	r15, $12
	ldm	@rr14, r10, $4
	ld	r13, r15
	ld	r1, SS|0x10(r13)
	sub	r0, r0
	ldl	SS|0x08(r13), rr0
	ld	r11, SS|0x12(r13)
	sub	r10, r10
	ldl	rr0, SS|0x08(r13)
	slal	rr0, $16
	addl	rr0, rr10
	ldm	r10, @rr14, $4
	inc	r15, $12
	ret
/ ptradv(&seg,&off) -- off += 2, carry into the seg field at wrap.
ptradv:
	dec	r15, $8
	ldm	@rr14, r10, $4
	ld	r13, r15
	ldl	rr10, SS|0x10(r13)
	ld	r1, @rr10
	inc	r1, $2
	ldl	rr10, SS|0x10(r13)
	ld	@rr10, r1
	test	r1
	jr	nz, 9f
	ldl	rr10, SS|0x0c(r13)
	ld	r1, @rr10
	add	r1, $0x0100
	ldl	rr10, SS|0x0c(r13)
	ld	@rr10, r1
9:
	ldm	r10, @rr14, $4
	inc	r15, $8
	ret
/ praddrl -- print the dump-line "seg|off-" prefix, reset gutter.
praddrl:
	dec	r15, $4
	ldl	@rr14, rr12
	ld	r13, r15
	push	@rr14, $0x0004
	ld	r12, ddtscr_
	srl	r12, $8
	ld	r1, r12
	sub	r0, r0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x007c
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x000c
	ld	r1, ddtvars_+0x34
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x002d
	call	putchar_
	inc	r15, $2
	clr	ddtvars_+0x0e
	clrb	ddtvars_+0x10
	ldl	rr12, @rr14
	inc	r15, $4
	ret
/ praddr(far) -- print a far value as "seg|off".
praddr:
	dec	r15, $2
	ld	@rr14, r13
	ld	r13, r15
	push	@rr14, $0x0004
	ldl	rr0, SS|0x06(r13)
	sral	rr0, $24
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x007c
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x000c
	ldl	rr0, SS|0x06(r13)
	and	r1, $0xffff
	sub	r0, r0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	ld	r13, @rr14
	inc	r15, $2
	ret
/ memdump -- interactive examine/edit core used by 'e' and 's':
/ 8 words/line in hex + ASCII gutter; typed value overtypes, DEL quits.
memdump:
	dec	r15, $12
	ldm	@rr14, r10, $4
	ld	r13, r15
	clr	SS|0x0a(r13)
	clr	SS|0x08(r13)
	test	line_+0x64
	.byte	0xe6, 0x7d
/ dumploop -- per-word dump loop.
dumploop:
	ld	r1, line_+0x64
	dec	line_+0x64, $1
	test	r1
	jp	z, dumpend
	cp	SS|0x0a(r13), $0x0008
	jr	lt, 1f
	call	flushlb
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	clr	SS|0x0a(r13)
	inc	SS|0x08(r13), $1
	ld	r12, SS|0x08(r13)
	cp	r12, $0x0015
	jr	le, 1f
	clr	SS|0x08(r13)
	call	getchar_
	ld	r12, r1
	ld	ddttmp_, r12
	cp	r12, $0x007f
	jp	z, dumpend
1:
	test	SS|0x0a(r13)
	jr	nz, 2f
	call	praddrl
	jr	3f
2:
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
3:
	push	@rr14, ddtvars_+0x34
	push	@rr14, ddtscr_
	call	mkfp_
	inc	r15, $4
	ldl	ddtvars_+0x38, rr0
	ldl	rr10, ddtvars_+0x38
	ld	r1, @rr10
	ld	ddtvars_+0x36, r1
	push	@rr14, $0x000c
	ld	r1, ddtvars_+0x36
	sub	r0, r0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	ld	r1, ddtvars_+0x36
	srl	r1, $8
	push	@rr14, r1
	call	putlb
	inc	r15, $2
	ld	r1, ddtvars_+0x36
	and	r1, $0x00ff
	push	@rr14, r1
	call	putlb
	inc	r15, $2
	ldl	rr0, $ddtvars_+0x34
	pushl	@rr14, rr0
	ldl	rr0, $ddtscr_
	pushl	@rr14, rr0
	call	ptradv
	inc	r15, $8
	inc	SS|0x0a(r13), $1
	jr	dumploop
/ edline -- edit mode: re-print the current word, read a new value.
edline:
	push	@rr14, ddtvars_+0x34
	push	@rr14, ddtscr_
	call	mkfp_
	inc	r15, $4
	ldl	ddtvars_+0x38, rr0
	clr	SS|0x0a(r13)
	call	praddrl
	ldl	rr10, ddtvars_+0x38
	ld	r1, @rr10
	ld	ddtvars_+0x36, r1
	push	@rr14, $0x000c
	ld	r1, ddtvars_+0x36
	sub	r0, r0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	ld	r1, ddtvars_+0x36
	srl	r1, $8
	push	@rr14, r1
	call	putlb
	inc	r15, $2
	ld	r1, ddtvars_+0x36
	and	r1, $0x00ff
	push	@rr14, r1
	call	putlb
	inc	r15, $2
	call	flushlb
	ldl	rr0, $ddtmsgs_
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	call	hrline_
	ldl	rr0, $ddtbss_
	ldl	ddtvars_+0x30, rr0
	ldl	rr10, ddtvars_+0x30
	cpb	@rr10, $0x0a
	jr	z, 5f
	ldl	rr10, ddtvars_+0x30
	ldb	rl1, @rr10
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jr	pl, 4f
	ldl	rr10, ddtvars_+0x30
	cpb	@rr10, $0x27
	jr	nz, 5f
4:
	call	gettok
	ldl	rr10, ddtvars_+0x38
	ld	@rr10, r1
5:
	ldl	rr0, $ddtvars_+0x34
	pushl	@rr14, rr0
	ldl	rr0, $ddtscr_
	pushl	@rr14, rr0
	call	ptradv
	inc	r15, $8
	ldb	rl1, ddtbss_
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jp	pl, edline
	ldb	rl1, ddtbss_
	extsb	rh1
	ld	r12, r1
	cp	r12, $0x000a
	jp	z, edline
	ldb	rl1, ddtbss_
	extsb	rh1
	ld	r12, r1
	cp	r12, $0x0027
	jp	z, edline
/ dumpend -- flush the pending gutter and finish.
dumpend:
	test	SS|0x0a(r13)
	jr	z, 9f
	call	flushlb
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
9:
	ldm	r10, @rr14, $4
	inc	r15, $12
	ret
/ debugcmd -- DDT main entry from the trap stub: adjust the saved
/ frame, print the breakpoint/signal banner, fall into cmdloop.  Cross-object
debugcmd_:
	sub	r15, $0x003e
	ldm	@rr14, r6, $8
	ld	r13, r15
	ldl	rr0, ldrbuf_
	sub	r1, $0xffd2
	ldl	ldrbuf_, rr0
	ldl	rr6, ldrbuf_
	dec	r7, $12
	ld	r1, @rr6
	add	r1, $0x002a
	ld	@rr6, r1
	call	splhi_
	ld	SS|0x24(r13), r1
	ld	SS|0x30(r13), $0xffff
	ldl	rr0, ldrbuf_
	dec	r1, $4
	ldl	SS|0x18(r13), rr0
	ldl	rr6, SS|0x18(r13)
	ldl	rr0, @rr6
	and	r1, $0xffff
	and	r0, $0x7fff
	ldl	@rr6, rr0
	ldl	rr0, $ddtline_
	jr	2f
1:
	ldl	rr0, SS|0x2c(r13)
	inc	r1, $6
2:
	ldl	SS|0x2c(r13), rr0
	ldl	rr0, SS|0x2c(r13)
	cpl	rr0, $cmdtab_
	jr	nc, 3f
	ldl	rr6, SS|0x2c(r13)
	inc	r7, $2
	testl	@rr6
	jr	z, 1b
	ldl	rr6, SS|0x2c(r13)
	inc	r7, $2
	ldl	rr4, @rr6
	ldl	rr6, SS|0x2c(r13)
	ld	r1, @rr6
	ld	@rr4, r1
	jr	1b
3:
	cp	SS|0x42(r13), $0x000c
	.byte	0xee, 0x77
	ldl	rr6, ldrbuf_
	dec	r7, $8
	ld	r12, @rr6
	cp	r12, $0x0e00
	jr	nz, sigrept
	ldl	rr6, SS|0x18(r13)
	ldl	rr0, @rr6
	subl	rr0, $0x00000002
	ldl	@rr6, rr0
	clr	SS|0x30(r13)
	jr	5f
4:
	inc	SS|0x30(r13), $1
5:
	cp	SS|0x30(r13), $0x0010
	jr	ge, 6f
	ldl	rr6, SS|0x18(r13)
	ld	r1, SS|0x30(r13)
	mult	rr0, $0x0006
	ld	r12, r1
	ldl	rr0, ddtline_+0x02(r12)
	cpl	rr0, @rr6
	jr	nz, 4b
6:
	cp	SS|0x30(r13), $0x0010
	jr	nz, bprept
	ldl	rr0, $ddtmsgs_+0x03
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	ldl	rr6, SS|0x18(r13)
	pushl	@rr14, @rr6
	call	praddr
	jr	7f
/ bprept -- print "Breakpoint <n> at <seg|off>" (+ base-relative).
bprept:
	ldl	rr0, $ddtmsgs_+0x1b
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	push	@rr14, $0x0000
	ld	r1, SS|0x30(r13)
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	ldl	rr0, $ddtmsgs_+0x28
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	ld	r1, SS|0x30(r13)
	mult	rr0, $0x0006
	ld	r12, r1
	pushl	@rr14, ddtline_+0x02(r12)
	call	praddr
	inc	r15, $4
	test	ddtscr_+0x04
	jr	z, 8f
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x000c
	ld	r5, ddtscr_+0x04
	sub	r4, r4
	ldl	rr6, SS|0x18(r13)
	ldl	rr0, @rr6
	subl	rr0, rr4
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	jr	8f
/ sigrept -- print "<pc>: <signame>" from the table.
sigrept:
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	ldl	rr6, SS|0x18(r13)
	pushl	@rr14, @rr6
	call	praddr
	inc	r15, $4
	ldl	rr0, $ddtmsgs_+0x2d
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	ld	r12, SS|0x42(r13)
	sla	r12, $2
	pushl	@rr14, dbgfmt_+0x16(r12)
	call	outstr_
7:
	inc	r15, $4
8:
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	ldl	rr0, $ddtmsgs_+0x30
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	clr	ddtvars_+0x2c
/ cmdloop -- prompt (readline), skip blanks, dispatch the command
/ char; every handler jumps back here.
cmdloop:
	call	hrline_
	ldl	rr0, $ddtbss_
	ldl	ddtvars_+0x30, rr0
1:
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	ld	r12, r1
	ld	ddttmp_, r12
	cp	r12, $0x0020
	jr	z, 1b
	cp	ddttmp_, $0x000a
	jr	z, cmdloop
	ld	r1, ddttmp_
	jp	dispatch
/ hexmath -- 'h a b': print hex sum and difference.
hexmath:
	clr	ddtvars_+0x26
	clr	ddtvars_+0x34
1:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 2f
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	jr	1b
2:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x0a
	jr	z, 7f
	call	gettok
	ld	ddtvars_+0x34, r1
3:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 4f
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	jr	3b
4:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x0a
	jr	z, 5f
	call	gettok
	jr	6f
5:
	ld	r1, ddtvars_+0x34
	ld	line_+0x64, r1
	ld	r1, ddtscr_+0x04
	ld	ddtvars_+0x34, r1
	ld	r1, line_+0x64
6:
	ld	ddtvars_+0x26, r1
	push	@rr14, $0x000c
	ld	r12, ddtvars_+0x34
	add	r12, ddtvars_+0x26
	ld	r1, r12
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x000c
	ld	r12, ddtvars_+0x34
	sub	r12, ddtvars_+0x26
	ld	r1, r12
/ prword -- shared tail: sign-extend r1 and print it,
prword:
	exts	rr0
/ prval -- print rr0,
prval:
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
/ prnl -- newline,
prnl:
	push	@rr14, $0x000a
	call	putchar_
/ poploop -- pop args and return to cmdloop.
poploop:
	inc	r15, $2
	jp	cmdloop
7:
	push	@rr14, $0x000c
	ld	r1, ddtscr_+0x04
	sub	r0, r0
	jr	prval
/ inport -- 'i port': read a port and echo "<port> <value>".
inport:
	call	gettok
	ld	ddtvars_+0x34, r1
	test	ddtvars_+0x34
	jr	pl, inloop
/ synerr -- error 1 "Syntax error",
synerr:
	push	@rr14, $0x0001
/ errout -- print errtab[code] (pmsg_) and return to cmdloop.
errout:
	call	pmsg_
	jr	poploop
/ inloop -- per-port loop: print "<port> <INB(port)> ", read more.
inloop:
	clr	SS|0x3c(r13)
	push	@rr14, $0x000c
	ld	r1, ddtvars_+0x34
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x0004
	push	@rr14, ddtvars_+0x34
	call	inb_
	inc	r15, $2
	ld	r12, r1
	ld	r1, r12
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	call	hrline_
	ldl	rr0, $ddtbss_
	ldl	ddtvars_+0x30, rr0
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x0a
	jr	z, 2f
	ldl	rr6, ddtvars_+0x30
	ldb	rl1, @rr6
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jr	mi, 1f
	ldl	rr6, ddtvars_+0x30
	inc	r7, $1
	ldb	rl1, @rr6
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jr	mi, 1f
	ldl	rr6, ddtvars_+0x30
	inc	r7, $2
	cpb	@rr6, $0x0a
	jr	nz, 1f
	call	gettok
	push	@rr14, r1
	push	@rr14, ddtvars_+0x34
	call	outb_
	inc	r15, $4
	jr	2f
1:
	ld	SS|0x3c(r13), $0x0001
2:
	test	SS|0x3c(r13)
	jr	z, inloop
	jp	cmdloop
/ outport -- 'o port': write value tokens to a port.
outport:
	call	gettok
	ld	ddtvars_+0x34, r1
	test	ddtvars_+0x34
	jp	mi, synerr
/ outloop -- per-value loop: OUTB(port, gettok()).
outloop:
	clr	SS|0x3c(r13)
	push	@rr14, $0x000c
	ld	r1, ddtvars_+0x34
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	call	hrline_
	ldl	rr0, $ddtbss_
	ldl	ddtvars_+0x30, rr0
	ldl	rr6, ddtvars_+0x30
	ldb	rl1, @rr6
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jr	mi, 1f
	ldl	rr6, ddtvars_+0x30
	inc	r7, $1
	ldb	rl1, @rr6
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	test	r1
	jr	mi, 1f
	ldl	rr6, ddtvars_+0x30
	inc	r7, $2
	cpb	@rr6, $0x0a
	jr	nz, 1f
	call	gettok
	push	@rr14, r1
	push	@rr14, ddtvars_+0x34
	call	outb_
	inc	r15, $4
	jr	2f
1:
	ld	SS|0x3c(r13), $0x0001
2:
	test	SS|0x3c(r13)
	jr	z, outloop
	jp	cmdloop
/ remapmmu -- 'S seg base attr len': program one MMU descriptor.
remapmmu:
	ldl	rr8, $ddtbss_
	call	gettok
	ld	SS|0x2a(r13), r1
	call	gettok
	ld	@rr8, r1
	call	gettok
	extsb	rh1
	ldb	rr8(0x0003), rl1
	call	gettok
	extsb	rh1
	ldb	rr8(0x0002), rl1
	test	SS|0x2a(r13)
	jp	mi, synerr
	cp	SS|0x2a(r13), $0x003f
	jp	gt, synerr
	cp	@rr8, $0x3f00
	jp	ugt, synerr
	ldl	rr0, $ddtbss_
	pushl	@rr14, rr0
	push	@rr14, SS|0x2a(r13)
	call	mmuwrite_
	inc	r15, $6
	jp	cmdloop
/ bpcmd -- 'b': read sub-command char, dispatch via bpswitch/bptab.
bpcmd:
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	jp	bpswitch
/ bpclear -- "bc": zero all 16 bp_addr slots.
bpclear:
	ldl	rr0, $ddtline_
	jr	2f
1:
	ldl	rr0, SS|0x2c(r13)
	inc	r1, $6
2:
	ldl	SS|0x2c(r13), rr0
	ldl	rr0, SS|0x2c(r13)
	cpl	rr0, $cmdtab_
	jp	nc, cmdloop
	ldl	rr6, SS|0x2c(r13)
	inc	r7, $2
	subl	rr0, rr0
	ldl	@rr6, rr0
	jr	1b
/ bpdisp -- "bd": list the non-empty breakpoints as "<n> = <seg|off>".
bpdisp:
	clr	SS|0x30(r13)
	jr	2f
1:
	inc	SS|0x30(r13), $1
2:
	cp	SS|0x30(r13), $0x0010
	jp	ge, cmdloop
	ld	r1, SS|0x30(r13)
	mult	rr0, $0x0006
	ld	r12, r1
	testl	ddtline_+0x02(r12)
	jr	z, 1b
	push	@rr14, $0x0000
	ld	r1, SS|0x30(r13)
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	ldl	rr0, $ddtmsgs_+0x51
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	ld	r1, SS|0x30(r13)
	mult	rr0, $0x0006
	ld	r12, r1
	pushl	@rr14, ddtline_+0x02(r12)
	call	praddr
	inc	r15, $4
	test	ddtscr_+0x04
	jr	z, 3f
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	ld	r5, ddtscr_+0x04
	sub	r4, r4
	ld	r1, SS|0x30(r13)
	mult	rr0, $0x0006
	ld	r12, r1
	ldl	rr6, ddtline_+0x02(r12)
	subl	rr6, rr4
	ld	r1, r7
	ld	ddtvars_+0x36, r1
	push	@rr14, $0x000c
	ld	r1, ddtvars_+0x36
	sub	r0, r0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
3:
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	jr	1b
/ bpset -- "bs <n> <addr>": set breakpoint #n (rejects duplicates).
bpset:
	clrb	SS|0x26(r13)
1:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 2f
	ldl	rr0, ddtvars_+0x30
	inc	r1, $1
	ldl	ddtvars_+0x30, rr0
	jr	1b
2:
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	ld	ddtvars_+0x0c, r1
	test	r1
	jp	mi, synerr
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jp	nz, synerr
	call	getaddr
	ld	ddtvars_+0x34, r1
	bit	ddtvars_+0x34, $0
	jr	z, 3f
/ oddadr -- error 2 "Odd address".
oddadr:
	push	@rr14, $0x0002
	jp	errout
3:
	push	@rr14, ddtvars_+0x34
	ld	r1, ddtscr_
	and	r1, $0x7fff
	push	@rr14, r1
	call	mkfp_
	inc	r15, $4
	ldl	SS|0x14(r13), rr0
	ldl	rr0, $ddtline_
	jr	5f
4:
	ldl	rr0, SS|0x2c(r13)
	inc	r1, $6
5:
	ldl	SS|0x2c(r13), rr0
	ldl	rr0, SS|0x2c(r13)
	cpl	rr0, $cmdtab_
	jr	nc, 6f
	ldl	rr6, SS|0x2c(r13)
	inc	r7, $2
	ldl	rr0, @rr6
	cpl	rr0, SS|0x14(r13)
	jr	nz, 4b
	ldb	SS|0x26(r13), $0x01
	push	@rr14, $0x0003
	call	pmsg_
	inc	r15, $2
	jr	4b
6:
	testb	SS|0x26(r13)
	jp	nz, cmdloop
	ld	r1, ddtvars_+0x0c
	mult	rr0, $0x0006
	ld	r12, r1
	ldl	rr0, SS|0x14(r13)
/ bpstore -- store rr0 into bp_addr[n] (also bpremove clear path).
bpstore:
	ldl	ddtline_+0x02(r12), rr0
	jp	cmdloop
/ bpremove -- "br <n>": clear breakpoint #n.
bpremove:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 1f
	ldl	rr0, ddtvars_+0x30
	inc	r1, $1
	ldl	ddtvars_+0x30, rr0
	jr	bpremove
1:
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	ld	ddtvars_+0x0c, r1
	test	r1
	jp	mi, synerr
	ld	r1, ddtvars_+0x0c
	mult	rr0, $0x0006
	ld	r12, r1
	subl	rr0, rr0
	jr	bpstore
/ illerr -- error 0 "Illegal command".
illerr:
	push	@rr14, $0x0000
	jp	errout
/ bpswitch -- CPIR the sub-command char over the 'c d r s' key
/ table, indexed jump via bptab.
bpswitch:
	.byte	0x21, 0x00, 0x00, 0x04
	ldl	rr2, $bpkeys
	cpir	r1, @rr2, r0, z
	jr	nz, illerr
	sub	r3, $bpkeys
	add	r3, r3
	ldl	rr2, bptab(r3)
	jp	@rr2
bpkeys:
	.word	'c
	.word	'd
/ bptab -- bc/bd/br/bs handler address table (raw .long targets).
bptab:
	.word	'r
	.word	's
	.long	bpclear
	.long	bpdisp
	.long	bpremove
	.long	bpset
/ resume -- 'c': plant 0x0e00 breakpoint opcodes at the active bp
/ addrs, restore saved FCW (splx), exit through dbgret -> trap stub IRET.
resume:
	ldl	rr6, ldrbuf_
	dec	r7, $12
	ld	r1, @rr6
	sub	r1, $0x002a
	ld	@rr6, r1
	ldl	rr0, $ddtline_
	jr	2f
1:
	ldl	rr0, SS|0x2c(r13)
	inc	r1, $6
2:
	ldl	SS|0x2c(r13), rr0
	ldl	rr0, SS|0x2c(r13)
	cpl	rr0, $cmdtab_
	jr	nc, 3f
	ldl	rr6, SS|0x2c(r13)
	inc	r7, $2
	testl	@rr6
	jr	z, 1b
	ldl	rr4, SS|0x2c(r13)
	inc	r5, $2
	ldl	rr6, @rr4
	ld	r1, @rr6
	ldl	rr6, SS|0x2c(r13)
	ld	@rr6, r1
	ldl	rr4, SS|0x18(r13)
	ldl	rr6, SS|0x2c(r13)
	inc	r7, $2
	ldl	rr0, @rr6
	cpl	rr0, @rr4
	jr	z, 1b
	ldl	rr4, SS|0x2c(r13)
	inc	r5, $2
	ldl	rr6, @rr4
	ld	@rr6, $0x0e00
	jr	1b
3:
	push	@rr14, SS|0x24(r13)
	call	splx_
	inc	r15, $2
	jp	dbgret
modreg:
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	ld	r12, r1
	ld	ddttmp_, r12
	cp	r12, $0x0020
	jr	z, modreg
	cp	ddttmp_, $0x000a
	jp	z, synerr
	lda	rr0, SS|0x32(r13)
	and	r0, $0x7f00
	ldl	ddtvars_+0x3e, rr0
1:
	cp	ddttmp_, $0x0020
	jr	z, 3f
	cp	ddttmp_, $0x000a
	jr	z, 3f
	lda	rr6, SS|0x3b(r13)
	and	r6, $0x7f00
	ldl	rr0, ddtvars_+0x3e
	cpl	rr0, rr6
	jr	nc, 2f
	ldl	rr6, ddtvars_+0x3e
	add	r7, $0x0001
	ldl	ddtvars_+0x3e, rr6
	sub	r7, $0x0001
	ld	r1, ddttmp_
	ldb	@rr6, rl1
2:
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	ld	ddttmp_, r1
	jr	1b
3:
	ldl	rr6, ddtvars_+0x3e
	clrb	@rr6
4:
	cp	ddttmp_, $0x0020
	jr	nz, 5f
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	ld	ddttmp_, r1
	jr	4b
5:
	push	@rr14, ddttmp_
	call	hexval_
	inc	r15, $2
	ld	ddtvars_+0x0c, r1
	test	r1
	jp	mi, synerr
	subl	rr0, rr0
	ldl	SS|0x10(r13), rr0
6:
	ld	r7, ddtvars_+0x0c
	exts	rr6
	ldl	rr0, SS|0x10(r13)
	slal	rr0, $4
	addl	rr0, rr6
	ldl	SS|0x10(r13), rr0
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	ld	ddttmp_, r1
	push	@rr14, r1
	call	hexval_
	inc	r15, $2
	ld	ddtvars_+0x0c, r1
	test	r1
	jr	pl, 6b
	ldl	rr10, $regtab_
	jr	8f
7:
	inc	r11, $6
8:
	cpl	rr10, $regtabl_
	jr	nc, 1f
	lda	rr0, rr10(0x0002)
	and	r0, $0x7f00
	pushl	@rr14, rr0
	lda	rr0, SS|0x32(r13)
	and	r0, $0x7f00
	pushl	@rr14, rr0
	call	scmp_
	inc	r15, $8
	test	r1
	jr	z, 7b
1:
	cpl	rr10, $regtabl_
	jp	z, synerr
	ldb	rl1, rr10(0x0001)
	extsb	rh1
	jr	regsize
/ setw -- store the value into a word register slot of the frame.
setw:
	ldb	rl1, @rr10
	extsb	rh1
	ld	r12, r1
	add	r12, r12
	ldl	rr6, ldrbuf_
	add	r7, r12
	ld	r1, SS|0x12(r13)
	ld	@rr6, r1
	jp	cmdloop
/ setl -- store the value into a long (rr*) register slot.
setl:
	ldb	rl1, @rr10
	extsb	rh1
	ld	r12, r1
	add	r12, r12
	ldl	rr6, ldrbuf_
	add	r7, r12
	ldl	rr0, SS|0x10(r13)
	ldl	@rr6, rr0
	jp	cmdloop
/ regsize -- dispatch on the is-long flag from the name table.
regsize:
	test	r1
	jr	z, setw
	cp	r1, $0x0001
	jr	z, setl
	jp	cmdloop
/ regdump -- 'r': print all 19 saved registers.
regdump:
	clr	ddttmp_
	jr	2f
1:
	inc	ddttmp_, $1
2:
	cp	ddttmp_, $0x0013
	jp	ge, prnl
	ld	r1, ddttmp_
	and	r1, $0x0003
	jr	z, 3f
	push	@rr14, $0x0020
	jr	4f
3:
	test	ddttmp_
	jr	z, 5f
	push	@rr14, $0x000a
4:
	call	putchar_
	inc	r15, $2
5:
	ld	r1, ddttmp_
	mult	rr0, $0x0006
	ld	r12, r1
	ld	r1, r12
	sub	r0, r0
	addl	rr0, $regtab_
	ldl	rr10, rr0
	lda	rr0, rr10(0x0002)
	and	r0, $0x7f00
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	lda	rr6, rr10(0x0001)
	and	r6, $0x7f00
	testb	@rr6
	jr	nz, 7f
	push	@rr14, $0x000c
	ldb	rl1, @rr10
	extsb	rh1
	ld	r12, r1
	add	r12, r12
	ldl	rr6, ldrbuf_
	add	r7, r12
	ld	r1, @rr6
	exts	rr0
	pushl	@rr14, rr0
6:
	call	puthex_
	inc	r15, $6
	jr	1b
7:
	push	@rr14, $0x001c
	ldb	rl1, @rr10
	extsb	rh1
	ld	r12, r1
	add	r12, r12
	ldl	rr6, ldrbuf_
	add	r7, r12
	pushl	@rr14, @rr6
	jr	6b
/ trace -- hidden 't' command: toggle single-step trace_flag.
trace:
	test	ddtvars_+0x2c
	jr	nz, 1f
	ldk	r1, $0x1
	jr	2f
1:
	sub	r1, r1
2:
	ld	ddtvars_+0x2c, r1
	ldl	rr0, $ddtmsgs_+0x55
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	push	@rr14, $0x0000
	ld	r1, ddtvars_+0x2c
	jp	prword
/ help -- '?': print the command menu.
help:
	call	menu
	jp	cmdloop
/ editmem -- 'e addr [nword]': interactive examine/edit via memdump.
editmem:
	call	getaddr
	ld	ddtvars_+0x34, r1
	cp	ddtvars_+0x34, $0xfffe
	jp	z, synerr
	cp	ddtvars_+0x34, $0xffff
	jp	z, cmdloop
	clr	line_+0x64
1:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 2f
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	jr	1b
2:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x0a
	jr	z, 3f
	call	gettok
	ld	line_+0x64, r1
3:
	bit	ddtvars_+0x34, $0
	jp	nz, oddadr
	test	line_+0x64
	jp	mi, synerr
	call	memdump
	jp	cmdloop
/ stkdump -- 's [nword]': memdump from the saved stack pointer.
stkdump:
	ldl	rr6, ldrbuf_
	dec	r7, $12
	ld	r1, @rr6
	ld	ddtvars_+0x34, r1
	ld	r1, ddtscr_
	ld	ddtbss_+0x100, r1
	ldl	rr6, ldrbuf_
	dec	r7, $14
	ld	r1, @rr6
	ld	ddtscr_, r1
	clr	line_+0x64
1:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x20
	jr	nz, 2f
	ldl	rr6, ddtvars_+0x30
	add	r7, $0x0001
	ldl	ddtvars_+0x30, rr6
	sub	r7, $0x0001
	ldb	rl1, @rr6
	extsb	rh1
	jr	1b
2:
	ldl	rr6, ddtvars_+0x30
	cpb	@rr6, $0x0a
	jr	z, 3f
	call	gettok
	ld	line_+0x64, r1
3:
	test	line_+0x64
	jp	mi, synerr
	ldl	rr0, $ldrbuf_+0x206
	ldl	SS|0x1c(r13), rr0
	push	@rr14, ddtvars_+0x34
	push	@rr14, ddtscr_
	call	mkfp_
	inc	r15, $4
	ldl	SS|0x20(r13), rr0
	ldl	rr0, SS|0x1c(r13)
	cpl	rr0, SS|0x20(r13)
	jr	nc, 4f
	ldl	rr0, SS|0x1c(r13)
	addl	rr0, $0x3e000000
	ldl	SS|0x1c(r13), rr0
4:
	ld	r12, line_+0x64
	add	r12, r12
	ld	r1, r12
	sub	r0, r0
	addl	rr0, SS|0x20(r13)
	cpl	rr0, SS|0x1c(r13)
	jr	ule, 5f
	ldl	rr0, SS|0x1c(r13)
	subl	rr0, SS|0x20(r13)
	exts	rr0
	div	rr0, $0x0002
	ld	line_+0x64, r1
5:
	call	memdump
	ld	r1, ddtbss_+0x100
	ld	ddtscr_, r1
	jp	cmdloop
/ fillmem -- 'f addr nword value': fill area with a word value.
fillmem:
	call	getaddr
	ld	ddtvars_+0x34, r1
	cp	ddtvars_+0x34, $0xfffe
	jp	z, synerr
	cp	ddtvars_+0x34, $0xffff
	jp	z, cmdloop
	call	gettok
	ld	line_+0x64, r1
	bit	ddtvars_+0x34, $0
	jp	nz, oddadr
	test	line_+0x64
	jp	z, synerr
	call	gettok
	ld	ddtvars_+0x36, r1
/ filloop -- write the fill word, ptradv, count times.
filloop:
	ld	r1, line_+0x64
	dec	line_+0x64, $1
	test	r1
	jp	z, cmdloop
	push	@rr14, ddtvars_+0x34
	push	@rr14, ddtscr_
	call	mkfp_
	inc	r15, $4
	ldl	ddtvars_+0x38, rr0
	ld	r1, ddtvars_+0x36
	ldl	rr6, ddtvars_+0x38
	ld	@rr6, r1
	ldl	rr0, $ddtvars_+0x34
	pushl	@rr14, rr0
	ldl	rr0, $ddtscr_
	pushl	@rr14, rr0
	call	ptradv
	inc	r15, $8
	jr	filloop
/ setbase -- 'a addr': set base segment/offset for
/ subsequent displays.
setbase:
	call	getaddr
	ld	ddtscr_+0x04, r1
	bit	ddtscr_+0x04, $0
	jp	nz, oddadr
	ld	r1, ddtscr_
	ld	ddtscr_+0x02, r1
	jp	cmdloop
/ movemem -- 'm addr1 addr2 nword': block move, word at a time.
movemem:
	call	getaddr
	ld	ddtvars_+0x34, r1
	cp	ddtvars_+0x34, $0xfffe
	jp	z, synerr
	cp	ddtvars_+0x34, $0xffff
	jp	z, cmdloop
	ld	r1, ddtscr_
	ld	ddtvars_+0x36, r1
	call	getaddr
	ld	ddtvars_+0x26, r1
	cp	ddtvars_+0x26, $0xfffe
	jp	z, synerr
	cp	ddtvars_+0x26, $0xffff
	jp	z, cmdloop
	bit	ddtvars_+0x34, $0
	jp	nz, oddadr
	bit	ddtvars_+0x26, $0
	jp	nz, oddadr
	ld	r1, ddtscr_
	ld	ddtbss_+0x100, r1
	ld	r1, ddtvars_+0x36
	ld	ddtscr_, r1
	call	gettok
	ld	line_+0x64, r1
	test	line_+0x64
	jp	z, synerr
/ movloop -- copy one word src->dst, ptradv both, count times.
movloop:
	ld	r1, line_+0x64
	dec	line_+0x64, $1
	test	r1
	jp	z, cmdloop
	push	@rr14, ddtvars_+0x34
	push	@rr14, ddtscr_
	call	mkfp_
	inc	r15, $4
	ldl	ddtvars_+0x38, rr0
	push	@rr14, ddtvars_+0x26
	push	@rr14, ddtbss_+0x100
	call	mkfp_
	inc	r15, $4
	ldl	ddtvars_+0x42, rr0
	ldl	rr6, ddtvars_+0x38
	ld	r1, @rr6
	ldl	rr6, ddtvars_+0x42
	ld	@rr6, r1
	ldl	rr0, $ddtvars_+0x34
	pushl	@rr14, rr0
	ldl	rr0, $ddtscr_
	pushl	@rr14, rr0
	call	ptradv
	inc	r15, $8
	ldl	rr0, $ddtvars_+0x26
	pushl	@rr14, rr0
	ldl	rr0, $ddtbss_+0x100
	pushl	@rr14, rr0
	call	ptradv
	inc	r15, $8
	jr	movloop
/ mmudump -- 'M': read all 64 MMU descriptors (mmuread) and list them.
mmudump:
	ldl	rr0, $ddtbss_
	pushl	@rr14, rr0
	push	@rr14, $0x0040
	push	@rr14, $0x0000
	call	mmuread_
	inc	r15, $8
	ldl	rr8, $ddtbss_
	clr	ddttmp_
	jr	2f
1:
	inc	ddttmp_, $1
2:
	cp	ddttmp_, $0x0040
	jp	ge, cmdloop
	push	@rr14, $0x0004
	ld	r1, ddttmp_
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	ldl	rr0, $ddtmsgs_+0x5f
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	push	@rr14, $0x000c
	ld	r1, @rr8
	sub	r0, r0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x0004
	ldb	rl1, rr8(0x0003)
	subb	rh1, rh1
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	push	@rr14, $0x0020
	call	putchar_
	inc	r15, $2
	push	@rr14, $0x0004
	ldb	rl1, rr8(0x0002)
	subb	rh1, rh1
	exts	rr0
	pushl	@rr14, rr0
	call	puthex_
	inc	r15, $6
	inc	r9, $4
	ld	r12, ddttmp_
	and	r12, $0x0003
	cp	r12, $0x0003
	jr	nz, 3f
	push	@rr14, $0x000a
	call	putchar_
	inc	r15, $2
	jr	1b
3:
	ldl	rr0, $ddtmsgs_+0x62
	pushl	@rr14, rr0
	call	outstr_
	inc	r15, $4
	jr	1b
/ dispatch -- CPIR the command char over the 16-entry key table,
/ indexed jump via cmdtab; no match -> illerr.
dispatch:
	ld	r0, $0x0010
	ldl	rr2, $cmdkeys
	cpir	r1, @rr2, r0, z
	jp	nz, illerr
	sub	r3, $cmdkeys
	add	r3, r3
	ldl	rr2, cmdtab(r3)
	jp	@rr2
cmdkeys:
	.word	'?
	.word	'M
	.word	'R
	.word	'S
	.word	'a
	.word	'b
	.word	'c
	.word	'e
	.word	'f
	.word	'h
	.word	'i
	.word	'm
	.word	'o
	.word	'r
/ cmdtab -- command handler address table, one entry per letter above.
cmdtab:
	.word	's
	.word	't
	.long	help
	.long	mmudump
	.long	modreg
	.long	remapmmu
	.long	setbase
	.long	bpcmd
	.long	resume
	.long	editmem
	.long	fillmem
	.long	hexmath
	.long	inport
	.long	movemem
	.long	outport
	.long	regdump
	.long	stkdump
	.long	trace
/ dbgret -- debugcmd epilogue: restore r6-r13, return to trap stub.
dbgret:
	ldm	r6, @rr14, $8
	add	r15, $0x003e
	ret

/ Debugger scratch cells and the shared boot/disk flags.
	.bssd
	.globl	dname_
	.globl	namebuf_
	.globl	ddtbss_
	.globl	curchip_
	.globl	ddttmp_
	.globl	hdf_
	.globl	hr_y_
dname_:	.blkb	2
namebuf_:	.blkb	14
ddtbss_:	.blkb	0x102
curchip_:	.blkb	2
	.blkb	4
hdf_:	.blkw	1
ddttmp_:	.blkb	6
hr_y_:	.blkw	1
