/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause


	.globl	scroll_, scantab_, aldir_, linerase_, nulls_, portst_


SEG0	= 0x3a000000		/ first screen segment
SEG1	= 0x3b000000		/ second screen segment
YMAX	= 800			/ scan lines per screen
YSPLIT	= 512			/ scan line that crossed into SEG1
YSCROLL	= 25			/ scrolling increment
MAXLINE = 32			/ YMAX / YSCROLL
WPERSL	= 64			/ words per scan line
BPERSL	= WPERSL * 2		/ bytes per scan line
SS = 0x3F3F			/ system stack 
OS = 0x3e3e
MMU	= 0x00FC
HS	= 0x3A3A
HSBASE	= 0x3E00
HSBASE2	= 0x3F00
/ rr2	pointer to scan line data
/ rr4	reference pointer to screen
/ r6	a word offset into the screen
/ r7	word count
/ rr8	source pointer
/ rr10	destination pointer

portst_:
	pushl	(rr14), rr2
	ld	r2,$HS				/ Do hi-res segments
	soutb	MMU+0x0100,rh2
	ld	r2,$HSBASE
	ld	r3,$HSBASE2
	soutb	MMU+0x0C00,rh2			/ HS base, high
	soutb	MMU+0x0C00,rl2			/ HS base, low
	soutb	MMU+0x0C00,rh3			/ HS2 base, high
	soutb	MMU+0x0C00,rl3			/ HS2 base, low
	popl	rr2, (rr14)
	ret


scroll_:
	pushl	(rr14), rr10
	pushl	(rr14), rr8
	push	(rr14), r7
	push	(rr14), r6
	pushl	(rr14), rr4
	pushl	(rr14), rr2

	ldl	rr2, $scantab_+YSCROLL*2
	ldl	rr4, $SEG0 + YSCROLL*BPERSL
	ld	r8, r4
	ld	r10, r4
	jr	1f

0:	add	r5, $BPERSL
	jr	c, 2f

1:	ld	r6, (rr2)
	xor	r6, $0xFFFF
	ld	OS|[-YSCROLL*2](r3), r6
	inc	r3, $2
	xor	r7, r7
	orb	rl7, rl6
	jr	z, 0b
	ldb	rl6, rh6
	xorb	rh6, rh6
	add	r6, r6
	ld	r9, r5
	add	r9, r6
	ld	r11, r9
	sub	r11, $YSCROLL*BPERSL
	ldir	(rr10), (rr8), r7
	jr	0b

2:	ldl	rr4, $SEG0+0x10000-YSCROLL*BPERSL
	ldl	rr8, $SEG1
	jr	1f

0:	add	r5, $BPERSL
	jr	c, 2f

1:	ld	r6, (rr2)
	xor	r6, $0xFFFF
	ld	OS|[-YSCROLL*2](r3), r6
	inc	r3, $2
	xor	r7, r7
	orb	rl7, rl6
	jr	z, 0b
	ldb	rl6, rh6
	xorb	rh6, rh6
	add	r6, r6
	ld	r11, r5
	add	r11, r6
	ld	r9, r11
	add	r9, $YSCROLL*BPERSL
	ldir	(rr10), (rr8), r7
	jr	0b

2:	ldl	rr4, $SEG1+YSCROLL*BPERSL
	ld	r10, r4
	jr	1f

0:	add	r5, $BPERSL
	cp	r3, $scantab_+YMAX*2
	jr	z, 2f

1:	ld	r6, (rr2)
	xor	r6, $0xFFFF
	ld	OS|[-YSCROLL*2](r3), r6
	inc	r3, $2
	xor	r7, r7
	orb	rl7, rl6
	jr	z, 0b
	ldb	rl6, rh6
	xorb	rh6, rh6
	add	r6, r6
	ld	r9, r5
	add	r9, r6
	ld	r11, r9
	sub	r11, $YSCROLL*BPERSL
	ldir	(rr10), (rr8), r7
	jr	0b

2:	ldl	rr8, $nulls_
	ld	r11, $[[MAXLINE-1]*YSCROLL-YSPLIT]*BPERSL
	ld	r7, $YSCROLL*WPERSL
	ldir	(rr10), (rr8), r7

	popl	rr2, (rr14)
	popl	rr4, (rr14)
	pop	r6, (rr14)
	pop	r7, (rr14)
	popl	rr8, (rr14)
	popl	rr10, (rr14)
	ret

aldir_:
	dec	r15, $12
	ldm	(rr14), r8, $6
	ld	r13, r15
	ldl	rr10, SS|16(r13)
	ldl	rr8, SS|20(r13)
	ld	r12, SS|24(r13)
	ldir	(rr10), (rr8), r12
	ldm	r8, (rr14), $6
	inc	r15, $12
	ret	


////////
/
/ erase text line
/	linerase( line)
/ YSCROLL scan lines are filled with nulls, starting at text line `line'.
/
linerase_:
	sub	r15, $16
	ldm	(rr14), r0, $8
	ld	r7, SS|20(r15)
	ld	r1, $YSCROLL
	mult	rr6, r1 
	sub	r0, r0
 
0:	ldl	rr4, $SEG0
	cp	r7, $YSPLIT
	jr	c, 1f
	ld	r0, $1
	ldl	rr4, $SEG1
	sub	r7, $YSPLIT

1:	ldl	rr2, rr6
	mult	rr2, $BPERSL
	ld	r5, r3
	ldl	rr2, $nulls_
	ld	r6, $WPERSL
	ldir	(rr4), (rr2), r6
	inc	r7, $1
	cp	r0, $1
	jr	z, 2f
	djnz	r1, 0b
	jr	un, 3f
 
2:	djnz	r1, 1b

3:
	ldm	r0, (rr14), $8
	add	r15, $16
	ret


	.bssd
nulls_:	.blkw	YSCROLL * WPERSL
