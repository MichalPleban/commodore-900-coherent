/ Copyright (c) 2026 Kevin Dedon.
/ SPDX-License-Identifier: BSD-3-Clause
/ C run time: reset vectors, power-on startup, port I/O and block
/ helpers, trap/interrupt dispatch.
	.globl	SS
SS	= 0x3F3F

/ RAMWINK -- kilobytes reserved at the top of physical RAM for the data
/ segment of this ROM (hardware segment 0x3f, mapped by sizeram below):
/ initialised data, then bss, then the stack growing down from STKTOP.
/ The 1985 ROM reserved 7K (data+bss 0x1016, stack top 0x121c); this
/ build has larger data, so the window is 12K.
RAMWINK	= 12
STKTOP	= 12*1024

	.globl	inb_
	.globl	inw_
	.globl	outb_
	.globl	outw_
	.globl	jmp_
	.globl	ramtest_
	.globl	serkey_
	.globl	edata_
	.globl	end_

/ ----------------------------------------------------------------------------
/ Reset + trap/IRQ vector area.  On Z8001 reset the CPU
/ loads FCW from 0x0002 and PC (seg:off) from 0x0004:0x0006 -> start.
/ Each trap entry is a reserved word + FCW, then the segmented handler PC.
/ ----------------------------------------------------------------------------
psa:
	.word	0xe81b
	.word	0xc000
	.word	0x8000
	.word	start
	.word	0x0000, 0xc000
	.word	0x8000, tepa
	.word	0x0000, 0xc000
	.word	0x8000, tprv
	.word	0x0000, 0xc000
	.word	0x8000, tsys
	.word	0x0000, 0xc000
	.word	0x8000, tseg
	.word	0x0000, 0xc000
	.word	0x8000, tnmi
	.word	0x0000, 0xc000
	.word	0x8000, tnvi

/ ----------------------------------------------------------------------------
/ start (ROM 0x0038) -- power-on / reset entry.  Sets up refresh + the program
/ status area, sizes RAM with an aa/55 pattern test (low->R13, high->R12 in
/ 512-byte clicks), reads the console-select straps, probes the serial console,
/ sets the stack (seg 0x3f), and calls main(ramlow, ramhigh).  If main returns,
/ jump back to RESET.
/ ----------------------------------------------------------------------------
	.globl	main_
start:
	ld	r0, $0x0080
	soutb	0x00fc, rl0
	soutb	0x11f8, rl0
	soutb	0x13f8, rl0
	soutb	0x14f8, rl0
	ld	r0, $0x9e00
	ldctl	REFRESH, r0
	ldar	r0, psa
	ldctl	PSAPSEG, r0
	ldctl	PSAPOFF, r1
	sub	r2, r2
	ld	r3, $0x0040
	ldl	rr0, $0x0000ff00
	soutb	0x20f8, rl0
	soutb	0x01f8, rl2
1:
	soutb	0x0ff8, rh0
	soutb	0x0ff8, rl0
	soutb	0x0ff8, rh1
	soutb	0x0ff8, rl1
	incb	rh0, $1
	djnz	r3, 1b
	ldb	rl0, $0xc0
	soutb	0x00fc, rl0
	ldk	r0, $1
	jp	ramtest_
/ sizeram -- RAM-sizing continuation; ramtest.o jumps back here after
/ the config display (cross-object entry).
	.globl	sizeram_
sizeram_:
	ldl	rr2, $RAMBASE
	ldb	rl4, rh3
	ldb	rh4, rh2
	ld	r12, r4
	srl	r12, $2
1:
	ld	@rr2, $0xaaaa
	cp	@rr2, $0xaaaa
	jr	nz, 2f
	ld	@rr2, $0x5555
	cp	@rr2, $0x5555
	jr	nz, 2f
	ld	r1, $0x01ff
	ldl	rr4, rr2
	inc	r5, $2
	clr	@rr2
	ldir	(rr4), (rr2), r1
	inc	r3, $2
	jr	nz, 1b
	incb	rh2, $1
	jr	1b
2:
	ldb	rl4, rh3
	ldb	rh4, rh2
	ld	r13, r4
	srl	r13, $2
	ld	r0, $RAMWINK
	ld	r1, r13
	sub	r1, r0
	sll	r1, $2
	ld	r0, $0x003f
	soutb	0x01fc, rl0
	soutb	0x08fc, rh1
	soutb	0x08fc, rl1
	ld	r0, $edata_
	ld	r2, $0x8000
	ld	r3, $ROMDATA
	ld	r4, $0x3f00
	sub	r5, r5
	ldirb	(rr4), (rr2), r0
/ zero the bss (edata_ .. end_) in the data segment
	ld	r4, $0x3f00
	ld	r5, $edata_
	ld	r0, $end_
	sub	r0, r5
	jr	z, 9f
8:
	clrb	@rr4
	inc	r5, $1
	dec	r0, $1
	jr	nz, 8b
9:
	ldk	r0, $1
	soutb	0x01fc, rl0
	soutb	0x08fc, rh1
	soutb	0x08fc, rl1
	ldctl	r0, NSPOFF
	cpb	rl0, $0x01
	jr	nz, 1f
	ldb	romconf_+0x0f, $0x01
	ldctl	r3, NSPSEG
	ld	r2, $0x3a00
	jr	2f
1:
	clrb	romconf_+0x0f
2:
	cpb	rl0, $0x02
	jr	nz, 3f
	ldb	ddtvars_, $0x01
	ldctl	r3, NSPSEG
	clrb	rh2
	ldb	rl2, rh3
	clrb	rh3
	jr	4f
3:
	clrb	ddtvars_
4:
	ldl	dbgfmt_+0x12, rr2
	testb	rh0
	jr	z, 5f
	ldb	abflags_, $0xff
5:
	ldb	rh1, $0x0a
probe:
	inb	rl1, 0x0101
	bitb	rl1, $0
	jr	z, 2f
	inb	rl1, 0x0111
	resb	rl1, $7
	cpb	rl1, $0x31
	jr	z, 1f
	dbjnz	r1, probe
	jr	2f
1:
	inb	rl1, 0x0101
	bitb	rl1, $0
	jr	z, 2f
	inb	rl1, 0x0111
	resb	rl1, $7
	cpb	rl1, $0x39
	jr	nz, probe
	ldb	abflags_+0x01, $0xff
	clrb	ddtvars_
	clrb	romconf_+0x0f
	jr	gomain
2:
	clrb	rl2
	lda	rr10, gotkey
3:
	ld	r5, $0x03e8
4:
	jp	serkey_
gotkey:
	testb	rl1
	jr	z, 7f
	resb	rl1, $7
	cpb	rl1, $0x4f
	jr	nz, 5f
	ldb	rl2, $0x01
	jr	3b
5:
	cpb	rl1, $0x49
	jr	nz, 6f
	testb	rl2
	jr	z, 3b
	ldb	abflags_+0x01, $0xff
	jr	gomain
6:
	clrb	rl2
	jr	3b
7:
	djnz	r5, 4b
gomain:
	ld	r14, $0x3f00
	ld	r15, $STKTOP
	ld	ldrbuf_+0x04, $0x8000
	push	@rr14, r13
	push	@rr14, r12
	call	main_
	jp	psa

/ ----------------------------------------------------------------------------
/ int inb(adr)  --  ROM 0x020a.  Read one byte from I/O port `adr`,
/ zero-extended to int.  Thin wrapper over the Z8001 inb instruction.
/ ----------------------------------------------------------------------------
inb_:
	ld	r1, rr14(4)
	inb	rl1, @r1
	subb	rh1, rh1
	ret

/ ----------------------------------------------------------------------------
/ int inw(adr)  --  ROM 0x0214.  Read one word from I/O port `adr`.
/ ----------------------------------------------------------------------------
inw_:
	ld	r1, rr14(4)
	in	r1, @r1
	ret

/ ----------------------------------------------------------------------------
/ outb(adr, data)  --  ROM 0x021c.  Write the low byte of `data` to port `adr`.
/ ----------------------------------------------------------------------------
outb_:
	ld	r1, rr14(4)
	ld	r0, rr14(6)
	outb	@r1, rl0
	ret

/ ----------------------------------------------------------------------------
/ outw(adr, data)  --  ROM 0x0228.  Write the word `data` to port `adr`.
/ ----------------------------------------------------------------------------
outw_:
	ld	r1, rr14(4)
	ld	r0, rr14(6)
	out	@r1, r0
	ret

/ ----------------------------------------------------------------------------
/ jmp(adr)  --  ROM 0x0234.  Segmented jump to the 32-bit address `adr`
/ (used by the run/boot path of the monitor).  Does not return.
/ ----------------------------------------------------------------------------
jmp_:
	ldl	rr2, rr14(4)
	jp	@rr2

	.globl	segload_
	.globl	ldirb_

/ ----------------------------------------------------------------------------
/ segload(a, b, c)  --  ROM 0x023a (boot.c fsload caller name).  Streams the
/ argument bytes out the hardware diagnostic ports (0x01fc front byte, 0x0ffc
/ data) -- used by the early boot/panic paths before the console is up.
/ ----------------------------------------------------------------------------
segload_:
	ld	r0, rr14(4)
	ld	r1, rr14(6)
	ld	r2, rr14(8)
	ld	r3, $0xff03
	soutb	0x01fc, rl0
	soutb	0x0ffc, rh1
	soutb	0x0ffc, rl1
	soutb	0x0ffc, rh3
	soutb	0x0ffc, rl3
	ld	r3, $0xff02
	soutb	0x0ffc, rh2
	soutb	0x0ffc, rl2
	soutb	0x0ffc, rh3
	soutb	0x0ffc, rl3
	ret

/ ----------------------------------------------------------------------------
/ ldirb(src, dst, len)  --  ROM 0x0274.  Block-copy `len` bytes from `src` to
/ `dst` using the Z8001 ldirb auto-repeat instruction.
/ ----------------------------------------------------------------------------
ldirb_:
	ldl	rr2, rr14(4)
	ldl	rr4, rr14(8)
	ld	r1, rr14(12)
	ldirb	(rr4), (rr2), r1
	ret

	.globl	pclear_
	.globl	trap_

/ ----------------------------------------------------------------------------
/ pclear(ptr, len)  --  ROM 0x0286.  Zero-fill `len` bytes at the segmented
/ pointer `ptr`: clear the first byte, then ldirb-propagate the zero forward.
/ ----------------------------------------------------------------------------
pclear_:
	ldl	rr2, rr14(4)
	ldl	rr4, rr2
	ld	r1, rr14(8)
	clrb	@rr2
	inc	r5, $1
	dec	r1, $1
	jr	z, 9f
	ldirb	(rr4), (rr2), r1
9:
	ret

/ ----------------------------------------------------------------------------
/ Trap / interrupt handlers.  Each vectored entry pushes a
/ one-word trap identifier and falls into the common dispatcher at trapcom, which
/ saves the full register file + the normal-stack pointer, reads the MMU/trap
/ status latches (special I/O 0x0cfc), calls TrapHandler (trap.o ), then
/ restores and IRETs.  These are the targets of the .long vectors at 0x000c..0x0034.
/ ----------------------------------------------------------------------------
tepa:
	push	@rr14, $0x000c
	jr	trapcom
tprv:
	push	@rr14, $0x000d
	jr	trapcom
tsys:
	push	@rr14, $0x0007
	jr	trapcom
tseg:
	soutb	0x11f8, rl0
	push	@rr14, $0x000b
	jr	trapcom
tnmi:
	push	@rr14, $0x000f
	jr	trapcom
tnvi:
	push	@rr14, $0x000a
trapcom:
	sub	r15, $0x0020
	ldm	@rr14, r0, $16
	ldctl	r0, NSPSEG
	ldctl	r1, NSPOFF
	pushl	@rr14, rr0
	ldl	ldrbuf_, rr14
	ld	r0, $0x3d3d
	soutb	0x01fc, rh0
	sinb	rh3, 0x0cfc
	sinb	rl3, 0x0cfc
	sinb	rh2, 0x0cfc
	sinb	rl2, 0x0cfc
	pushl	@rr14, rr2
	nop
	call	trap_
	di	VI
	popl	rr0, @rr14
	ld	r2, $0x3d3d
	soutb	0x01fc, rh2
	soutb	0x0cfc, rh1
	soutb	0x0cfc, rl1
	soutb	0x0cfc, rh0
	soutb	0x0cfc, rl0
	popl	rr2, @rr14
	ldctl	NSPSEG, r2
	ldctl	NSPOFF, r3
	ldm	r0, @rr14, $14
	add	r15, $0x0022
	iret
