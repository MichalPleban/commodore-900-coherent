/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/ Assembler support for Commodore Z8000 memory-mapped video routines.
/

sp 	= rr14			/ stack pointer
spo	= r15			/ stack pointer offset
INDX6845= 0x410			/ 6845 display chip index register
DATA6845= 0x412			/ 6845 display chip data register
CRTCNTRL= 0x418			/ CRT control register
CRTON	= 0x28			/ Enable CRT video & blink
CRTOFF	= 0			/ Disable CRT video & blink

.globl	SS			/ Defined in md.s

/ Take arguments off stack pointer
/
#define args(n) SS|2+2*[n](spo)



.globl	mmstuff_

/ Stuff `n' copies of `value' into the memory-mapped screen
/ starting `segoff' bytes in from `base'.
/	mmstuff(segoff, value, n, base)
/	    int segoff, value, n;
/	    char *base;
/
mmstuff_:
	ld	r0, args(1)	/ Load offset,
	ld	r1, args(2)	/ value,
	ld	r2, args(3)	/ count,
	ldl	rr4, args(4)	/ and screen memory base.
	pushl	(sp), rr6
/	exb	rh1, rl1	/ Change from 8086 to z8000 byte order.
	test	r2		/ If count = 0, quit.
	jr	eq, 0f
	add	r5, r0		/ Point to destination.
	ld	@rr4, r1	/ and write value.
	dec	r2		/ Decrement count
	jr	eq, 0f		/ and if more to write,
	ldl	rr6, rr4	/ use what was just written as the source,
	add	r7, $2		/ make the next word the destination,
	ldir	@rr6, @rr4, r2	/ and stuff memory.
	
0:	popl	rr6, (sp)
	ret



.globl	mmcopy_

/ Copy `n' screen characters from `from' byte offset to `to' byte offset
/ in from 'base'.  The `flag' is non-zero for downward direction, zero
/ for upwards.	The count is in 2 byte character and attribute units.
/	mmcopy(from, to, n, flag, base)
/	   int from, to, n, flag;
/	   char *base;
/
mmcopy_:
	ld	r0, args(1)		/ Load source offset,
	ld	r1, args(2)		/ destination offset,
	ld	r2, args(3)		/ count,
	ld	r3, args(4)		/ direction,
	ldl	rr4, args(5)		/ and screen memory base.
	pushl	(sp), rr6
	test	r2			/ If non-zero count,
	jr	eq, 1f
	ldl	rr6, rr4		/ point to source & destination
	add	r5, r0
	add	r7, r1
	test	r3			/ and move words upwards/downwards.
	jr	ne, 0f
	ldir	@rr6, @rr4, r2
	jr	1f
0:	lddr	@rr6, @rr4, r2
	
1:	popl	rr6, (sp)
	ret



.globl	mmcrt_

/ Send a word to the specified register of
/ the 6845 CRT controller.  This puts out
/ high and low parts.
/	mmcrt(regno, val)
/	  int regno, val;
/
mmcrt_:
	ld	r0, args(1)	/ Load register number
	ld	r1, args(2)	/ and value.
	outb	INDX6845, rl0	/ Select register and
	outb	DATA6845, rh1	/ write high data.
	incb	rl0		/ Select register+1 and
	outb	INDX6845, rl0
	outb	DATA6845, rl1	/ write low data.
	ret


.globl	mmvoff_
.globl	mmvonn_

/ Turn video off/on before/after diddling controller or video ram.
/
mmvoff_:
	ldb	rl0, $CRTOFF
	outb	CRTCNTRL, rl0
	ret

mmvonn_:
	ldb	rl0, $CRTON
	outb	CRTCNTRL, rl0
	ret
