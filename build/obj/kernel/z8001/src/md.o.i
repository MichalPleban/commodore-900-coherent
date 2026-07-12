




/ (lgl-
/ 	The information contained herein is a trade secret of Mark Williams
/ 	Company, and  is confidential information.  It is provided  under a
/ 	license agreement,  and may be  copied or disclosed  only under the
/ 	terms of  that agreement.  Any  reproduction or disclosure  of this
/ 	material without the express written authorization of Mark Williams
/ 	Company or persuant to the license agreement is unlawful.
/ 
/ 	COHERENT Version 0.7.3
/ 	Copyright (c) 1982, 1983, 1984.
/ 	An unpublished work by Mark Williams Company, Chicago.
/ 	All rights reserved.
/ -lgl)





/ Machine-dependent code in Coherent on the Commodore
/ M-series Z8001 processor running in segmented mode
/ (both user and system).

UPASIZE =	2048		/ Size of U-area in bytes
NHUSEG	=	48		/ Number of hardware user segments
EFAULT =	14		/ Taken from <errno.h>
SIGSYS =	 7		/ Taken from <msig.h>
SIGBPT =	10
SIGSEG =	11
SIGEPA =	12
SIGPRV =	13
SIGNVI =	14
SIGNMI =	15

.globl	SS
USTACK	= 0x0000
USEG	= 0x0303
ES	= 0x3D3D		/ Must be OS-1
OS	= 0x3E3E		/ Overlay (driver,buffer,clist,inode) seg
SS	= 0x3F3F
sp	= rr14			/ stack pointer
spo	= r15			/ stack pointer offset
/ argument off stack pointer



/ Commodore-specific hardware:
/ Definitions for the system clock which uses CT3 of Z8036 #1,
/ and Centronics Line Printer which uses ports of 8036 #1 and #2.
MMU	=	0x00FC			/ Access MMU # 1 (T#2 is unused)
SSTEP	=	0x0203			/ single step logic port address

CLKVAL0	=	20000			/ 4MHz / 100 / 2
CLKVAL1	=	30000			/ 6 MHz / 100 / 2
LPIRQ	=	0x70			/ LP interrupt vector #
ZCIO2	=	0x80			/ Z8036 #2 base port
MICR	=	0x01			/ Master interrupt control port
MCCR	=	0x03			/ Master configuration control port
CTIV	=	0x09			/ Interrupt vector
CT3CS	=	0x19			/ Command and status
CT3CH	=	0x35			/ time constant high
CT3CL	=	0x37			/ Time constant low
CT3MS	=	0x3D			/ CT3 Mode Select
PBMS	=	0x51			/ Port B Mode specification reg.
PBHS	=	0x53			/ Port B Handshake specification
PBDPP	=	0x55			/ Port B data path polarity
PBDD	=	0x57			/ Port B data direction
PBSIOC	=	0x59			/ Port B special I/O control
PBDATA	=	0x1D			/ Port B data
PBPPR	=	0x5B			/ Port B pattern polarity reg.
PBPTR	=	0x5D			/ Port B pattern transition
PBPMR	=	0x5F			/ Port B pattern mask reg.
PBCS	=	0x13			/ Port B command and status
PBIV	=	0x07			/ Port B interrupt vector
PCDPP	=	0x0B			/ Port C data path polarity
PCDD	=	0x0D			/ Port C data direction
PCSIOC	=	0x0F			/ Port C special I/O control
PCDATA	=	0x1F			/ Port C data

/ Bits in the FCW
FCW_SEG	=	0x8000			/ Segmented mode
SGBIT	=	15			/ Bit # of segmented/non-segmented bit
FCW_SYS	=	0x4000			/ System (vs. normal) mode
SBIT	=	14			/ Bit # of system mode bit
FCW_EPA	=	0x2000			/ Enable Extended processor
FCW_VIE	=	0x1000			/ Enable vectored interrupts
VIBIT	=	12
FCW_NVIE =	0x0800			/ Enable non-vectored interrupts
NVIBIT	=	11
TFCW	=	FCW_SEG+FCW_SYS				/ New FCW after traps

/ The Program Status Area.  This file must be loaded
/ first and this data must be first in order to
/ ensure that it is on a mod-256 byte boundary.
/ The format of this table is suitable only for
/ Z8001 and Z8003 processors.  The z8002 uses fewer
/ bytes because there are no segment numbers.
/ This table is in code because that is the way the
/ hardware fetches it.
.shri
.globl	cmdblk_
cmdblk_:
ldar	rr2, start		/ Gets segment number properly
jp	(rr2)
.blkb	[3*512]+cmdblk_-.	/ cmdblk size is 3 512 byte blocks
/ first for commands, second for ECC,
/ third is I/O buffer
psa:
ldar	rr2, start		/ Gets segment number properly
jp	(rr2)
.word	0			/ ...
.long	TFCW			/ Extended instruction trap
.long	tepa
.long	TFCW			/ Privileged instruction trap
.long	tprv
.long	TFCW			/ System call trap
.long	tsys
.long	TFCW			/ Segment trap
.long	tseg
.long	TFCW			/ Non-maskable interrupt
.long	tnmi
.long	TFCW			/ Non-vectored interrupt
.long	tnvi

/ Vectored interrupts
.long	TFCW			/ FCW for all vectored interrupts

psavi:
.long	vint			/ Vector #0 (clock)
.long	vint			/ Vector #2
.long	vint			/ Vector #4
.long	vint			/ Vector #6
.long	vint			/ Vector #8
.long	vint			/ Vector #10
.long	vint			/ Vector #12
.long	vint			/ Vector #14
.long	vint			/ Vector #16
.long	vint			/ Vector #18
.long	vint			/ Vector #20
.long	vint			/ Vector #22
.long	vint			/ Vector #24
.long	vint			/ Vector #26
.long	vint			/ Vector #28
.long	vint			/ Vector #30
.long	vint			/ Vector #32
.long	vint			/ Vector #34
.long	vint			/ Vector #36
.long	vint			/ Vector #38
.long	vint			/ Vector #40
.long	vint			/ Vector #42
.long	vint			/ Vector #44
.long	vint			/ Vector #46
.long	vint			/ Vector #48
.long	vint			/ Vector #50
.long	vint			/ Vector #52
.long	vint			/ Vector #54
.long	vint			/ Vector #56
.long	vint			/ Vector #58
.long	vint			/ Vector #60
.long	vint			/ Vector #62
.long	vint			/ Vector #64
.long	vint			/ Vector #66
.long	vint			/ Vector #68
.long	vint			/ Vector #70
.long	vint			/ Vector #72
.long	vint			/ Vector #74
.long	vint			/ Vector #76
.long	vint			/ Vector #78
.long	vint			/ Vector #80
.long	vint			/ Vector #82
.long	vint			/ Vector #84
.long	vint			/ Vector #86
.long	vint			/ Vector #88
.long	vint			/ Vector #90
.long	vint			/ Vector #92
.long	vint			/ Vector #94
.long	vint			/ Vector #96
.long	vint			/ Vector #98
.long	vint			/ Vector #100
.long	vint			/ Vector #102
.long	vint			/ Vector #104
.long	vint			/ Vector #106
.long	vint			/ Vector #108
.long	vint			/ Vector #110
.long	vint			/ Vector #112
.long	vint			/ Vector #114
.long	vint			/ Vector #116
.long	vint			/ Vector #118
.long	vint			/ Vector #120
.long	vint			/ Vector #122
.long	vint			/ Vector #124
.long	vint			/ Vector #126
.long	vint			/ Vector #128
.long	vint			/ Vector #130
.long	vint			/ Vector #132
.long	vint			/ Vector #134
.long	vint			/ Vector #136
.long	vint			/ Vector #138
.long	vint			/ Vector #140
.long	vint			/ Vector #142
.long	vint			/ Vector #144
.long	vint			/ Vector #146
.long	vint			/ Vector #148
.long	vint			/ Vector #150
.long	vint			/ Vector #152
.long	vint			/ Vector #154
.long	vint			/ Vector #156
.long	vint			/ Vector #158
.long	vint			/ Vector #160
.long	vint			/ Vector #162
.long	vint			/ Vector #164
.long	vint			/ Vector #166
.long	vint			/ Vector #168
.long	vint			/ Vector #170
.long	vint			/ Vector #172
.long	vint			/ Vector #174
.long	vint			/ Vector #176
.long	vint			/ Vector #178
.long	vint			/ Vector #180
.long	vint			/ Vector #182
.long	vint			/ Vector #184
.long	vint			/ Vector #186
.long	vint			/ Vector #188
.long	vint			/ Vector #190
.long	vint			/ Vector #192
.long	vint			/ Vector #194
.long	vint			/ Vector #196
.long	vint			/ Vector #198
.long	vint			/ Vector #200
.long	vint			/ Vector #202
.long	vint			/ Vector #204
.long	vint			/ Vector #206
.long	vint			/ Vector #208
.long	vint			/ Vector #210
.long	vint			/ Vector #212
.long	vint			/ Vector #214
.long	vint			/ Vector #216
.long	vint			/ Vector #218
.long	vint			/ Vector #220
.long	vint			/ Vector #222
.long	vint			/ Vector #224
.long	vint			/ Vector #226
.long	vint			/ Vector #228
.long	vint			/ Vector #230
.long	vint			/ Vector #232
.long	vint			/ Vector #234
.long	vint			/ Vector #236
.long	vint			/ Vector #238
.long	vint			/ Vector #240
.long	vint			/ Vector #242
.long	vint			/ Vector #244
.long	vint			/ Vector #246
.long	vint			/ Vector #248
.long	vint			/ Vector #250
.long	vint			/ Vector #252
.long	vint			/ Vector #254

/ Interrupt dispatch table for loadable drivers.
/ This table is set/cleared by setivec/clrivec.
/ Some go to C routines, others to `vret' routine.
/ The clock (timer) is handled specially.
/ There are only 128 vectored interrupts on the z8001
/ versus 256 on the z8002.
.shrd
.globl	vecs_, vmaps_

vecs_:
.long	vret_			/ Where setivec puts its vectors
.blkl	127			/ Remaining vectors

vmaps_:
.blkw	128			/ Maps for overlay segment

.shri
/ Segmentation prototype for the system
/ Segment 30 code is code size of coherent, system only
/ Segment 31 data is full 64K, system only
/ Segments 32 and 33 are clist and buffer-mapped segments
/ Segment 39 is the Western Digital Disc Mailbox and buffers
/ Segments 3A and 3B are accessible to everyone for the bitmap
/ Segments 3C, 3D, 3E are extra and overlay segments, full 64K, system only
/ Segment 3F stack is 2K, system only
/ Other segments are CPU inhibited.
sattr:	.byte	0x02, 0x02, 0x02, 0x02, 0x04, 0x04, 0x04, 0x04
.byte	0x04, 0x02, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02
slen:	.byte	      0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00
.byte	0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07
.even

.globl	u_
u_	= 0x3F000000			/ SS | 0

/ The ROM has set the machine up and initialised most
/ CPU and hardware things.  The processor is running
/ in 1 MMU mode (segmented) with code in segment 0x30
/ and data bootstrapped into segment 0x31.
/ We start by setting up a stack pointer at the
/ end of the U-area.
.shri
.globl	etext_, edata_, end_
.globl	corebot_, romconf_
.globl	main_, commodore_, stand_

start:
ldl	rr14, $u_+UPASIZE	/ Put stack at end of U-area
sub	r0, r0			/ Set user stack (only for emulator)
ldctl	NSPOFF, r0
ldctl	NSPSEG, r0

ldl	rr2, $vecs_		/ vector #0
ldl	rr4, rr2		/ rr2 is pointer to ...
inc	r5, $4			/  next vector entry
ld	r0, $127*2		/ Fill all 128 vectors with ...
ldir	(rr4), (rr2), r0	/ copy of vector #0

ldar	rr0, clk		/ Special clock routine goes ...
ldl	vecs_+0, rr0		/   into vector slot #0

ldar	rr0, psa		/ This is in code segment
ldctl	PSAPSEG, r0		/ System data is actually code here
ldctl	PSAPOFF, r1

/ Load of system segment attributes.
ldar	rr2, sattr			/ Pointer to 16 attributes
ldl	rr0, $0x00300010		/ segment IS, 16 segments
soutb	MMU+0x0100, rl0			/
ld	r4, $MMU+0x0E00			/ Write attributes
sotirb	(r4), (rr2), r1			/ Write out seg. attributes

/ Load system segment lengths.
ldar	rr2, slen			/ Pointer to 15 lengths
ldl	rr0, $0x0031000F		/ segment IS+1, 15 segments
soutb	MMU+0x0100, rl0			/ Set segment number
ld	r4, $MMU+0x0D00			/ Set limit field
sotirb	(r4), (rr2), r1			/ Write out seg. limits

call	commodore_		/ Do machine-specific initialisation

call	vidsel			/ Probe video RAM, choose console driver

/ Set up U-area mapping at corebot

ld	r0, corebot_		/ mapping base
add	r0, r0			/ <<1 to hardware base
add	r0, r0			/ <<2 to hardware base
ld	r1, $SS
soutb	MMU+0x0100, rh1		/ Set segment number to U-area
soutb	MMU+0x0800, rh0		/ Write base field high
soutb	MMU+0x0800, rl0		/ Write field low

call	lpclk			/ Initialise clock and LP

/ Call main with interrupts on
/ When main returns, go into
/ user process at location 0.
ei	VI
call	main_			/ Interrupts enabled
di	VI

inc	depth_			/ Depth is one for user mode
ld	r0, $0xFFF0		/ save top 16 bytes of user stack
ldctl	NSPOFF, r0
ld	r0, $USTACK
ldctl	NSPSEG, r0
ldps	0f			/ Enter user mode for init process

0:	.word	0, FCW_VIE+FCW_SEG	/ Non-segmented, user mode, int. on
.word	USEG, 0			/ PC at 0 in User code segment
/ Never goes past here

/ Turn on clock.  It is specially set to vector #0.
/ The clock is in Z8036 chip #1 which also has the
/ keyboard scanning port on it.
/ Initialise ports on 8036 #1 and #2 for centronics line printer.
lpclk:

ld	r0, $0x01		/ Reset
outb	MICR, rl0		/ Z8036 #1
outb	ZCIO2+MICR, rl0		/ Z8036 #2
outb	MICR, rh0		/ Turn off reset
outb	ZCIO2+MICR, rh0		/
outb	PBSIOC, rh0		/ Init centronics data lines
outb	PBDPP, rh0
outb	PBDD, rh0
outb	PBDATA, rh0
outb	PCDPP, rh0		/ set port C non-inverting
ld	r0, $0x07		/ pc0, pc1, & pc2 are input
outb	PCDD, rl0		/ pc3 is output mode
outb	PCSIOC, rh0		/ port C normal mode operation
ld	r0, $0x70		/ mask to disable writing....
outb	PCDATA, rl0		/  on pc0, pc1, & pc2




ld	r0, $0x80		/ Continuous single cycle
ld	r1, $CLKVAL0		/ freq. at 1/2 PCLK rate 4 MHz
test	romconf_+14		/ Clock type ? [romconf.rom_ctype]
jp	eq, 0f			/ branch if 4 Mhz
ld	r1, $CLKVAL1		/ freq. at 1/2 PCLK rate 6 Mhz
0:

outb	CT3MS, rl0		/ Set counting and cycling options
sub	r2, r2
outb	CTIV, rl2		/ Vector #0 for clock
outb	CT3CH, rh1		/ Set counter frequency
outb	CT3CL, rl1		/ and low half
ld	r2, $0x20		/ Clear IP and IUS
outb	CT3CS, rl2
ld	r2, $0xC6		/ Set IE, Trigger Counter, gate
outb	CT3CS, rl2
ldb	rl0, $0x80		/ Set master ...
outb	MICR, rl0		/   interrupt enable
ldb	rl0, $0x90		/ Enable counter timer #3 ...
outb	MCCR, rl0		/   and port c
ldb	rl0, $0x06		/ start it up...
outb	CT3CS, rl0		/  trigger counter

/ Port 2 initialisation
ld	r0, $0x02
ld	r1, $0x04
outb	ZCIO2+PBMS, rl1		/ OR pattern recognition mode
outb	ZCIO2+PBHS, rh0
outb	ZCIO2+PBDPP, rl0	/ Ack line should be inverted
outb	ZCIO2+PBDD, rl0		/ Ack line is for input
outb	ZCIO2+PBSIOC, rl0	/ Normal I/O
outb	ZCIO2+PBPPR, rl0	/ Interrupt on ACK 1
outb	ZCIO2+PBPTR, rh0
outb	ZCIO2+PBPMR, rl0
ldb	rl1, $LPIRQ		/ Vector #
outb	ZCIO2+PBIV, rl1		/ Set into chip
ldb	rl1, $0x20
outb	ZCIO2+PBCS, rl1		/ Clear IP & IUS
ldb	rl1, $0xC0
outb	ZCIO2+PBCS, rl1		/ Enable port B interrupts
outb	ZCIO2+PCDPP, rh0	/ Strobe should not be inverted
outb	ZCIO2+PCDD, rh0		/ Strobe is output
outb	ZCIO2+PCSIOC, rh0	/ Normal I/O
ld	r1, $0x08
outb	ZCIO2+PCDATA, rl1	/ Strobe starts out as 1
outb	ZCIO2+CT3MS, rh0	/ No counter/timer
ldb	rl1, $0x80

outb	ZCIO2+MICR, rl1		/ Master interrupt enable

ldb	rl1, $0x90
outb	ZCIO2+MCCR, rl1		/ Enable ports B and C

ret

/ Trap and interrupt linkage
/ All routines end up building approximately the
/ same type of linkage information on the stack
/ so that higher level parts of the system do
/ most of the sophisticated work.  The stack looks
/ like this when C `trap' has been called:
/
/	|----------|
/	|pc offset |
/	|----------|
/	|pc segment|
/	|----------|
/	|    fcw   |
/	|----------|
/	|    id    |		pushed by hardware
/	|----------|
/	|trap type |		real for traps--junk for interrupts
/	|----------|
/	|    r15   |
/	|----------|
/	     ...
/	|----------|
/	|    r0    |
/	|----------|
/	| NSP OFF  |
/	|----------|
/	| NSP SEG  |
/	|----------|
/	|  OS Map  |
/	|----------|
/	|  ES Map  |
/	|----------|
/

tepa:	push	(sp), $SIGEPA	/ Extended processor trap
jp	0f

tprv:	push	(sp), $SIGPRV	/ Privileged instruction
jp	0f

tnmi:	push	(sp), $SIGNMI	/ Non-maskable interrupt
jp	0f

/ Non-vectored interrupt is for single-stepping in
/ ptrace system call.  Fiddle with the saved FCW (turn
/ off NVIE, enable VI)

tnvi:	
set	SS|2(spo), $VIBIT	/ Enable vectored interrupt and ...
res	SS|2(spo), $NVIBIT	/  disallow more steps by default
push	(sp), $SIGBPT		/ Generate a breakpoint
jp	0f

/ Special case for system mode segmentation violations
/ If so, and the PC points to certain special user-system
/ block move routines, then arrange so that the faulting
/ routine cleanly return an error.

tseg:
ex	r0, (sp)		/ Save r0
sinb	rl0, MMU+0x0200		/ Read violation type register
ex	r0, (sp)		/ into identifier and put r0 back
soutb	MMU+0x1100, rl0		/ Reset violation
bit	SS|2(spo), $SBIT	/ System mode?
jp	eq, 1f			/ User mode, not checked here
cp	SS|6(spo), $pgetw_	/ In special user-system
jp	ult, 1f			/ character-at-a-time routines?
cp	SS|6(spo), $kkcopy_	/
jp	uge, 2f			/ No -- check for block moves

ex	r0, SS|2(spo)		/
ldctl	FCW, r0			/ Restore FCW
ex	r0, SS|2(spo)		/
inc	spo, $8			/ Clear trap from stack
ldb	u_, $EFAULT		/ Set u.u_error code
ld	r1, $-1			/ return(-1)
ret				/ Return to caller

2:
cp	SS|6(spo), $kkfix	/ in block move routine?
jp	uge, 1f			/ no

ex	r0, SS|2(spo)		/
ldctl	FCW, r0			/ Re-load FCW
ex	r0, SS|2(spo)		/
inc	spo, $8			/ clear away trap
dec	r5			/ Compensate for last faulting ld byte
ldb	u_, $EFAULT		/ Set u.u_error
jp	kkfix			/ Let it clean up

1:
push	(sp), $SIGSEG		/ Segmentation violation
jp	0f

/ Common processing for all traps that haven't been
/ filtered out.  Build a special calling sequence for
/ the C routine `trap'.  This routine also handles `ddt'.

tsys:
push	(sp), $SIGSYS		/ System call
0:
dec	depth_			/ Down count soft interrupt level
sub	spo, $32		/ Save all registers.
ldm	(sp), r0, $16
ldctl	r0, NSPSEG		/ Save normal mode sp segment
ldctl	r1, NSPOFF		/ Save part of normal mode sp
pushl	(sp), rr0





ld	r0, $ES			/ Set ES segment
soutb	MMU+0x0100, rh0		/   ES + OS maps
sinb	rh3, MMU+0x0C00		/ ES high
sinb	rl3, MMU+0x0C00		/ ES low
sinb	rh2, MMU+0x0C00		/ OS high
sinb	rl2, MMU+0x0C00		/ OS low
pushl	(sp), rr2		/ push ES+OS

ei	VI
call	trap_			/ At last get to C
jp	0f			/ Dismiss trap

/ All vectored interrupts from devices come here.
/ The id comes from the vector # and is an even
/ integer in the range 0-254.  A fake trap type
/ is used to allow interrupts and traps to
/ return through the same code.
vint:
sub	spo, $32+2		/ Save registers and fake type
ldm	(sp), r0, $14		/ Save them (but not system SP)
ldctl	r0, NSPSEG		/ Save normal sp seg
ldctl	r1, NSPOFF		/ Save offset of normal sp
pushl	(sp), rr0

ldl	ddtregp_, sp		/ For ddt to access registers

ld	r0, $ES			/ Save OS and ES
soutb	MMU+0x0100, rh0		/   segment value
sinb	rh3, MMU+0x0C00		/ ES high
sinb	rl3, MMU+0x0C00		/ ES low
sinb	rh2, MMU+0x0800		/ OS high (no incr)
sinb	rl2, MMU+0x0800		/ OS low (no incr)
pushl	(sp), rr2		/ push OS+ES

dec	depth_			/ Down count int. level
ld	r2, SS|42(spo)		/ Identifier of vector (0-255)
and	r2, $0xFE		/ Make even, low byte
push	(sp), r2		/ Push id for interrupt routine
ld	r0, vmaps_(r2)		/ Vector mappings for OS
soutb	MMU+0x0800, rh0		/ high base
soutb	MMU+0x0800, rl0		/ Low base



add	r2, r2			/ <<1 to make room for 4-byte table
ldl	rr2, vecs_(r2)		/
call	(rr2)			/ Call interrupt routine
inc	spo, $2			/ Throw away junk

/ Dismiss sequence.
/ This is common sequence for interrupts and traps.
/ Attempt to reschedule.  If returning to normal mode,
/ run low priority part of clock (stand).
/ Restore context and return.
0:
test	depth_			/ Depth==0 means normal mode
jp	ne, 1f			/ Don't call stand if system mode

ldl	rr0, cprocp_		/ SELF==NULL ignored
cpl	rr0, iprocp_		/ Idle process interrupted?
jp	eq, 0f

bit	SS|44(spo), $SBIT	/ Retuurning to kernel process?
jp	ne, 1f			/ Then don't call stand
0:
call	stand_			/ Low-priority timers, etc.

1:
di	VI			/ Keep out vectored interrupts
inc	depth_			/ interrupts off from this to iret

popl	rr0, (sp)		/ rr0 = OS+ES
ld	r2, $ES			/ Restore OS + ES ...
soutb	MMU+0x0100, rh2		/   segment maps
soutb	MMU+0x0C00, rh1		/ ES high
soutb	MMU+0x0C00, rl1		/ ES low
soutb	MMU+0x0C00, rh0		/ OS high
soutb	MMU+0x0C00, rl0		/ OS low

popl	rr2, (sp)		/ rr2 = NSP
ldctl	NSPSEG, r2		/ Segment of NSP
ldctl	NSPOFF, r3

ldm	r0, (sp), $14		/ Re-load R0-R13
add	spo, $32+2		/ Pop saved regs + trap type

/ When NVIE bit in FCW is on, this indicates
/ a ptrace single step request has just been issued.
/ Turn off the normal interrupts and arm the special
/ delay circuit in the System Control latch.  This
/ will then go to `tnvi' after 1 user instruction has
/ been issued which will turn vectored interrupts on again.
bit	SS|2(spo), $NVIBIT	/ Is it ptrace?
jp	eq, 0f			/ no
res	SS|2(spo), $VIBIT	/ Turn off vectored interrupts
outb	SSTEP, rl0		/ Single stepper (just before iret)
0:
iret				/ At last

/ Special routine that interfaces to C clock routine.
/ diddles the clock hardware to dismiss the interrupt
/ and pushes user/system mode and user PC.
/ The PC and FCW are those pushed by the hardware and
/ found again by us by magic groping around!
/
/ Also empty interrupt dispatch routine
.globl	vret_, clock_

clk:
push	(sp), $0		/ Assume system mode
bit	SS|52(spo), $SBIT	/ Correct ?
jp	ne, 0f			/ Yes
inc	(sp)			/ Make normal (user) mode

0:
pushl	(sp), SS|54(spo)	/ User PC offset for profiler
call	clock_			/ Call portable code
inc	spo, $6			/ Clean up
ld	r0, $0x24		/ Clear IP and IUS to ...
outb	CT3CS, rl0		/ dismiss the interrupt

vret_:
ret

/ halt()
/ Called from panic and anywhere else
/ as an entry-point to `ddt'.  Fakes up
/ a trap sequence on the stack.
.globl	halt_, ddt_

halt_:







di	VI
jp	.


/ Routines to do the context switch.  The
/ switch takes two forms:
/ 1) an environment (`MENV')
/ 2) an context (`MCON') which requires reloading
/    of the U-area base as well as the rest of the
/    environment.
/ Except for a little bit of futsing around, these
/ two are really quite the same thing.
/ The switch assumes that C will save the other
/ registers in the course of things.

/ consave(ptr)
/ MCON *ptr;
/
/ conrest(u, o)
/ saddr_t u;
/ unsigned o;
/
/ envsave(ptr)
/ MENV *ptr;
/
/ envrest(ptr)
/ MENV *ptr;
.globl	consave_, conrest_
.globl	envsave_, envrest_

envsave_:
consave_:
ldl	rr2, SS|2+2*[1](spo)		/ pointer
ldm	(rr2), r6, $10		/ save r6-r15
add	r3, $32			/ end of structure
ld	r1, $ES			/ Segment for ES + OS
ldctl	r4, FCW			/ Save FCW before disable
di	VI
soutb	MMU+0x0100, rh1		/ set segment
sinb	rh0, MMU+0x0C00		/ ES high
sinb	rl0, MMU+0x0C00		/ ES low
sinb	rh1, MMU+0x0C00		/ OS high
sinb	rl1, MMU+0x0C00		/ OS low
ldctl	FCW, r4			/ Restore state
pushl	(rr2), rr0		/ Save it away
ld	r5, depth_		/ Save interrupt depth 
pushl	(rr2), rr4		/ Save FCW & depth
pushl	(rr2), (sp)		/ Save PC segment + offset
sub	r1, r1			/ return 0 here
ret

envrest_:
ldl	rr2, SS|2+2*[1](spo)		/ pointer
jp	0f

conrest_:
dec	depth_			/ Fake depth/no return to user
ldl	rr0, SS|2+2*[1](spo)		/ r0=user area, r1=offset
lda	rr2, u_(r1)		/ Address of context in U-area

di	VI
soutb	MMU+0x0100, rh2		/ Set map for SS segment
add	r0, r0			/ saddr_t <<1 to hardware
add	r0, r0			/ saddr_t <<2 to hardware
soutb	MMU+0x0800, rh0		/ Load base address ..
soutb	MMU+0x0800, rl0		/  of SS segment

0:
ldm	r6, (rr2), $10		/ Re-load r6-r15
add	r3, $20			/ after saved registers
popl	(sp), (rr2)		/ Save PC segment & offset
popl	rr4, (rr2)		/ Restore FCW and depth.
ld	depth_, r5		/ load depth, save FCW for later

ld	r1, $ES			/ Map segment
di	VI			/ MMU load indivisible
soutb	MMU+0x0100, rh1		/ Segment #
popl	rr0, (rr2)		/ GET ES & OS maps
soutb	MMU+0x0C00, rh0		/ ES high
soutb	MMU+0x0C00, rl0		/ ES low
soutb	MMU+0x0C00, rh1		/ OS high
soutb	MMU+0x0C00, rl1		/ OS low

ldctl	FCW, r4			/ Re-load FCW from long ago
ld	r1, $1			/ return 1
ret

/ spl(fcw)
/ Stuff new `fcw' into the FCW and return the
/ old one.  This is used to implement splo(), and
/ spl(f)
.globl	spl_, sphi_

spl_:
ldctl	r1, FCW			/ Get old FCW
ld	r0, SS|2+2*[1](spo)		/ Reload new FCW
ldctl	FCW, r0
ret

sphi_:
ldctl	r1, FCW			/ Old FCW
di	VI
ret

/ I/O Instructions
/ This is very messy on this machine because
/ there are so many instructions, most of which
/ might be used once or twice.  What we do is
/ to provide in/out for byte/word for normal i/o
/ space.  Special I/O is done only through assembler
/ routines for accessing the MMU.
/ Usage:
/	in(port)		inb (port)
/	out(port, valueue)	outb(port, value)
/
/	Block moves in/out for words/bytes.
/	inwcopy(port, paddr, nb)	inbcopy(port, paddr, nb)
/	unsigned port, nb;		unsigned port, nb;
/	paddr_t paddr;			paddr_t paddr;
/
/	outwcopy(port, paddr, nb)	outbcopy(port, paddr, nb)
/	unsigned port, nb;		unsigned port, nb;
/	paddr_t paddr;			paddr_t paddr;
/
/ outwcopy and outbcopy should have underbars after their name.
.globl	in_, inb_
.globl	out_, outb_
.globl	inwcopy_, inbcopy_
.globl	outwcopy_, outbcopy_

in_:
ld	r1, SS|2+2*[1](spo)
in	r1, (r1)
ret

inb_:
ld	r1, SS|2+2*[1](spo)
inb	rl1, (r1)
subb	rh1, rh1
ret

out_:
ld	r1, SS|2+2*[1](spo)		/ port
ld	r0, SS|2+2*[2](spo)		/ value
out	(r1), r0
ret

outb_:
ld	r1, SS|2+2*[1](spo)		/ port
ld	r0, SS|2+2*[2](spo)		/ value
outb	(r1), rl0
ret

inwcopy_:
ld	r1, SS|2+2*[1](spo)		/ port address
ldl	rr2, SS|2+2*[2](spo)		/ destination address
ld	r0, SS|2+2*[4](spo)		/ number of bytes
srl	r0, $1			/ convert to words
inir	(rr2), (r1), r0
ret

inbcopy_:
ld	r1, SS|2+2*[1](spo)		/ port address
ldl	rr2, SS|2+2*[2](spo)		/ destination address
ld	r0, SS|2+2*[4](spo)		/ number of bytes
inirb	(rr2), (r1), r0
ret

outwcopy_:
ld	r1, SS|2+2*[1](spo)		/ port address
ldl	rr2, SS|2+2*[2](spo)		/ source address
ld	r0, SS|2+2*[4](spo)		/ number of bytes
srl	r0, $1			/ convert to words
otir	(r1), (rr2), r0
ret

outbcopy_:
ld	r1, SS|2+2*[1](spo)		/ port address
ldl	rr2, SS|2+2*[2](spo)		/ source address
ld	r0, SS|2+2*[4](spo)		/ number of bytes
otirb	(r1), (rr2), r0
ret

/ Cross-space move routines, character, block,
/ word, and pointer.  There is a subterfuge to
/ make segmented and unsegmented things look
/ similar.  Also the order of these routines is
/ important.  From pgetw to kkcopy is checked
/ by the segmentation violation handler to
/ return faults on stuff copied to/from another
/ space.  From `kkcopy_' to `ppfix' is also handled
/ specially.  All other system mode segmentation
/ violations panic the system.

/ Get/put words/pointers/bytes into a physical space.
/ Macros in header files make this either user space
/ or auxiliary segment.
/
/ pgetw(p)		pgetp(p)		pgetb(p)
/ char *p;		char *p;		char *p;
/
/ pputw(p, i)		pputp(p, ptr)		pputb(p, c)
/ char *p;		char *p;		char *p;
/ int i;		char *ptr;		char c;
/
.globl	pgetw_, pgetp_, pgetb_
.globl	pputw_, pputp_, pputb_

pgetw_:
ldl	rr2, SS|2+2*[1](spo)		/ address
ld	r1, (rr2)		/ fetch value
ret

pgetb_:
ldl	rr2, SS|2+2*[1](spo)		/ address
sub	r1, r1
ldb	rl1, (rr2)
ret

pgetp_:
ldl	rr2, SS|2+2*[1](spo)		/ address
ldl	rr0, (rr2)		/ fetch pointer
ret

pputw_:
ldl	rr2, SS|2+2*[1](spo)		/ address
ld	r1, SS|2+2*[3](spo)		/ value
ld	(rr2), r1		/ store away
ret

pputb_:
ldl	rr2, SS|2+2*[1](spo)		/ address
ld	r1, SS|2+2*[3](spo)		/ value
ldb	(rr2), rl1		/ store away
ret

pputp_:
ldl	rr2, SS|2+2*[1](spo)		/ address
ldl	rr0, SS|2+2*[3](spo)		/ pointer value
ldl	(rr2), rr0		/ store away
ret

/ Kernel (system) address space copy and clear routines
/ NOTE: for speed, ukcopy has a special entry point here.
/
/ kclear(bp, n)			kkcopy(f, t, n)
/ char *bp;			char *f, *t;
/ unsigned n;			unsigned n;
.globl	kclear_, kkcopy_, ukcopy_

ukcopy_:
ldl	rr2, SS|2+2*[1](spo)		/ from pointer
and	r2, $~0xE0FF		/ ufix it
jp	0f

kkcopy_:
ldl	rr2, SS|2+2*[1](spo)		/ from pointer
0:
ldl	rr4, SS|2+2*[3](spo)		/ to pointer
ld	r0, SS|2+2*[5](spo)		/ count
test	r0
jp	eq, kkfix		/ No words to copy
ld	r1, r0			/ Test for even counters ...
or	r1, r3			/   and addresses
or	r1, r5			/
rr	r1			/ Set carry if anything odd
jp	c, 1f			/ Branch if slow (odd) mode
srl	r0			/ Divide by 2
ldir	(rr4), (rr2), r0	/ Do word copy
jp	kkfix

1:
ldirb	(rr4), (rr2), r0	/ Do word copy

kkfix:
ld	r1, r5			/ current destination offset
sub	r1, SS|2+2*[4](spo)		/ beginning destination pointer
ret

kclear_:
ldl	rr2, SS|2+2*[1](spo)		/ Kernel segmented pointer
ld	r0, SS|2+2*[3](spo)		/ number of bytes
srl	r0			/ to words
jp	nc, 1f			/ Branch if even
clrb	(rr2)			/ Clear first (odd) byte

1:
dec	r0			/ Funny way copy occurs
jp	mi, 1f			/ None to copy
clr	(rr2)
jp	eq, 1f			/ Done if count <4
lda	rr4, rr2(2)

ldir	(rr4), (rr2), r0	/ clear memory
1:
ret

/ Fixup a user address to prevent system accesses.
/ Map a paddr_t in segment `sn' and return a char * mapped in.
/ pfix is for setting up permanent segments that won't move much because
/ most segments are not context switched.
/ char *		char *
/ pfix(sn, p))		ufix(p)
/ paddr_t p;		char *p;
/ NOTE: ukcopy has its own version of ufix for speed.

.globl	pfix_, ufix_

pfix_:
ldctl	r4, FCW
ldb	rh0, SS|3+2*[1](spo)		/ segment # to map
subb	rl0, rl0		/ clear segment # low byte
ldl	rr2, SS|2+2*[2](spo)		/ paddr_t
ld	r1, r3			/ offset
and	r1, $0x3FF		/ within one saddr_t unit
srll	rr2, $10		/ to saddr_t
add	r3, r3			/ <<2 to hardware
add	r3, r3			/ ...
di	VI			/ Interrupts disabled for fiddling
soutb	MMU+0x0100, rh0		/ Segment #
soutb	MMU+0x0800, rh3		/ base high
soutb	MMU+0x0800, rl3		/ base low
ldctl	FCW, r4			/ Restore interrupts
ret

ufix_:
ldl	rr0, SS|2+2*[1](spo)		/ ptr
and	r0, $~0xE0FF		/ Turn off bottom, non-user seg bits
ret

/ vidsel - probe for a video card and select the init console driver.
/ Called once from `start' after commodore_ (the MMU is up) and before
/ main_ copies icode into user space (eveinit).  It overwrites the
/ icode-relative offset in the argv[2] slot of the init argument vector
/ (default driv_no = /drv/notty) with driv_hr or driv_lr when the
/ corresponding framebuffer RAM is found.  Detection is a two-pattern
/ write/read-back; undecoded reads return garbage on this machine, and
/ there is no system RAM in the framebuffer windows, so a match is
/ conclusive.  Hi-res is preferred, then low-res, else serial.
/ Interrupts are still off here (ei VI happens later in start).
vidsel:
ld	r0, $0x3E00		/ hi-res framebuffer 0x3E0000 >> 8
call	vprobe
jr	eq, 1f			/ absent - try text card
ld	r0, $driv_hr-icode
ld	argv+10, r0		/ patch argv[2] offset -> /drv/hrtty
ret
1:	ld	r0, $0x3700		/ text framebuffer 0x370000 >> 8
call	vprobe
jr	eq, 2f			/ absent - keep default (/drv/notty)
ld	r0, $driv_lr-icode
ld	argv+10, r0		/ patch argv[2] offset -> /drv/lrtty
2:	ret

/ vprobe - r0 = MMU base field (physical base >> 8) of a framebuffer.
/ Map the transient segment ES onto it and write/read-back 0x55AA then
/ its complement 0xAA55, restoring the original word.  Returns with the
/ Z flag SET when the card is ABSENT (both patterns must survive to be
/ present).  Clobbers r0-r6; uses ES, the same scratch segment pucopy
/ and kkcopy map through pfix.
vprobe:
ld	r1, $ES
soutb	MMU+0x0100, rh1		/ select segment ES
soutb	MMU+0x0800, rh0		/ base field high
soutb	MMU+0x0800, rl0		/ base field low
sub	r3, r3			/ offset 0 within ES
ld	r4, ES|0(r3)		/ save original word
ld	r5, $0x55AA
ld	ES|0(r3), r5		/ write pattern 1
ld	r6, ES|0(r3)		/ read it back
cp	r6, r5
jr	ne, 8f			/ mismatch - absent
ld	r5, $0xAA55
ld	ES|0(r3), r5		/ write pattern 2 (complement)
ld	r6, ES|0(r3)
cp	r6, r5
jr	ne, 8f
ld	ES|0(r3), r4		/ restore original word
ld	r0, $1
test	r0			/ present: clear Z
ret
8:	ld	ES|0(r3), r4		/ restore original word
sub	r0, r0			/ absent: set Z
ret

/ turn a paddr_t (physical offset in bytes) into a vaddr_t (virtual addr)
/ and vice versa.
/	paddr_t				vaddr_t
/	vtop(v)				ptov(p)
/	vaddr_t v;			paddr_t p;
.globl	vtop_, ptov_

vtop_:
ldl	rr0, SS|2+2*[1](spo)
ldb	rl0, rh0
and	r0, $0x007F
ret

ptov_:
ldl	rr0, SS|2+2*[1](spo)
ldb	rh0, rl0
and	r0, $0x7F00
ret

/
/ Segment copy and clear routines.
/ A `saddr_t' is is units of 512 or 1024 bytes
/ but the hardware actually uses units of
/ 256 bytes, so it must be shifted left.

/ Clear `n' saddr_t units of memory
/
/ sclear(a, n)
/ saddr_t a;
/ unsigned n;
.globl	sclear_

sclear_:
call	omapget_		/ Get old overlay map
ld	r0, SS|2+2*[1](spo)		/ Software clicks
add	r0, r0			/ Hardware clicks
add	r0, r0			/ <<2 for 1K segment click
pushl	(sp), rr0		/ Save Hardware click and old map

ld	r2, $OS			/ Segment of rr2 pointer
ld	r4, r2			/ segment of rr4 pointer
0:
dec	SS|2+2*[4](spo)			/ Really arg 2 - click counter
jp	mi, 1f
call	omapset_		/ Uses map value (sp), destroys rr0
sub	r3, r3			/ Zero offset of rr2
ld	r5, $2			/ Load 1 word offset from rr2
ld	r0, $512-1		/ 512 words == 1024 bytes

clr	(rr2)			/ clear first word
ldir	(rr4), (rr2), r0	/ clear subsequent words

inc	(sp), $4		/ next click
jp	0b			/ loop until done

1:
inc	spo, $2			/ Pop saddr from stack.
call	omapset_		/ restore Overlay map
inc	spo, $2
ret

/ Copy segments of memory from one place to another.
/
/ slrcopy(f, t, n)
/ saddr_t f, t;
/ unsigned n;
.globl	slrcopy_

slrcopy_:
ldl	rr0, SS|2+2*[1](spo)		/ r0 = `f', r1 = `t'
addl	rr0, rr0		/ adjust to hardware clicks.
addl	rr0, rr0		/ <<2 for 1K saddr_t
ld	r2, SS|2+2*[3](spo)		/ `n'
test	r2			/ if zero, done
jp	eq, 9f			/
ldctl	r3, FCW			/ Save interrupt state
dec	spo, $10		/ Reserve space to ...
ldm	(sp), r6, $5		/   Save some registers

ld	r4, $ES			/ Extra segment to MMU and address
ld	r8, $OS			/ OS segment # for address
di	VI			/ disable to fiddle
soutb	MMU+0x0100, rh4		/ Save ES + OS
sinb	rh6, MMU+0x0C00		/ ES high
sinb	rl6, MMU+0x0C00		/ ES low
sinb	rh7, MMU+0x0C00		/ OS high
sinb	rl7, MMU+0x0C00		/ OS low
ldctl	FCW, r3			/ Restore interrupts

0:
di	VI
soutb	MMU+0x0100, rh4		/ Set ES + OS to do copy
soutb	MMU+0x0C00, rh0		/ ES high
soutb	MMU+0x0C00, rl0		/ ES low
soutb	MMU+0x0C00, rh1		/ OS high
soutb	MMU+0x0C00, rl1		/ OS low
ldctl	FCW, r3			/ Restore interrupts

ld	r10, $512		/ word count of 1 click
sub	r5, r5			/ Virtual offset 0 in rr4
sub	r9, r9			/ Same for rr8
ldir	(rr8), (rr4), r10

inc	r0, $4			/ Next click
inc	r1, $4			/
djnz	r2, 0b			/ Cycle around

di	VI			/ Disable
soutb	MMU+0x0100, rh4		/ Restore ES + OS
soutb	MMU+0x0C00, rh6
soutb	MMU+0x0C00, rl6
soutb	MMU+0x0C00, rh7
soutb	MMU+0x0C00, rl7
ldctl	FCW, r3

ldm	r6, (sp), $5		/ Restore registers
inc	spo, $10
9:
ret

/ Extra map and overlay map manipulation routines.
/ omapset -- set the overlay segment map to the specified hardware map value.
/ omapget -- get the overlay segment value value
/ emapset -- same for the extra map
/ emapget -- get extra map
/ [NB: these routines only change r0 and r1!]
/ cmap_t
/ omapget()			omapset(m)
/				cmap_t m;
.globl	omapget_, omapset_, emapget_, emapset_

omapset_:
ld	r1, $OS			/ segment number ...
ldctl	r0, FCW			/ Save interrupt state
di	VI
soutb	MMU+0x0100, rh1		/   into MMU
ld	r1, SS|2+2*[1](spo)		/ map value ...
soutb	MMU+0x0800, rh1		/ high half of base
soutb	MMU+0x0800, rl1		/ low half of base
ldctl	FCW, r0			/ restore state
ret

emapset_:
ld	r1, $ES			/ segment number ...
ldctl	r0, FCW			/ Save interrupt state
di	VI
soutb	MMU+0x0100, rh1		/   into MMU
ld	r1, SS|2+2*[1](spo)		/ map value ...
soutb	MMU+0x0800, rh1		/ high half of base
soutb	MMU+0x0800, rl1		/ low half of base
ldctl	FCW, r0			/ restore state
ret

omapget_:
ld	r1, $OS			/ segment number ...
ldctl	r0, FCW			/ Save interrupt state
di	VI
soutb	MMU+0x0100, rh1		/   into MMU
sinb	rh1, MMU+0x0800		/ high half of base
sinb	rl1, MMU+0x0800		/ low half
ldctl	FCW, r0			/ restore state
ret

emapget_:
ld	r1, $ES			/ segment number ...
ldctl	r0, FCW			/ Save interrupt state
di	VI
soutb	MMU+0x0100, rh1		/   into MMU
sinb	rh1, MMU+0x0800		/ high half of base
sinb	rl1, MMU+0x0800		/ low half
ldctl	FCW, r0			/ restore state
ret

/ Load `nseg' segments of the MMU, starting at User segment base (segment 0)
/ Clear the rest of `NHUSEG' segments from access.
/
/ loadmmu(nseg, p)
/ unsigned segno, nseg;
/ char p[4*32];
.globl	loadmmu_

loadmmu_:
ld	r0, SS|2+2*[1](spo)		/ r0 = number of segments
ld	r5, $NHUSEG		/ Total segments
sub	r5, r0			/ Number to clear at end
add	r0, r0			/
add	r0, r0			/ * sizeof each descriptor
ldl	rr2, SS|2+2*[2](spo)		/ Pointer to segment descriptors.

sub	r1, r1			/ Segment # 0
ldctl	r4, FCW			/ Save interrupt state
di	VI
soutb	MMU+0x0100, rh1		/ Start segment #
soutb	MMU+0x2000, rl1		/ Clear descriptor counter (in case)

ld	r1, $MMU+0x0F00		/ command write descriptors and
sotirb	(r1), (rr2), r0		/ increment

test	r5
jp	eq, 1f			/ branch if none to clear

ldar	rr2, cseg		/ Clear segments
ld	r1, $MMU+0x0E00		/ Write attribute and increment SAR
sotirb	(r1), (rr2), r5		/
1:
ldctl	FCW, r4			/ Restore interrrupts
ret

/ Clear segments descriptors with CPU inhibits
cseg:	.byte	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
.byte	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
.byte	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
.byte	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
.byte	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04
.byte	0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04

/ Get the values in the MMU.
/
/ getmmu(segno, nseg, p)
/ unsigned segno, nseg;
/ char *p;

.globl	getmmu_

getmmu_:
ldl	rr0, SS|2+2*[1](spo)		/ r0=segno, r1=nseg
ldl	rr2, SS|2+2*[3](spo)		/ pointer to returned data
add	r1, r1			/ r1<<2 for ...
add	r1, r1			/ sizeof table

soutb	MMU+0x0100, rl0		/ Set segment # in MMU
soutb	MMU+0x2000, rh0		/ Clear descriptor counter

ld	r4, $MMU+0x0F00		/ command to read descriptors...
sinirb	(rr2), (r4), r1		/ increment
ret


/
/ Miscellaneous routines used by Coherent
/

/ Return the high order half of a*b.  Used
/ in scaling the profile data.
/
/ pscale(a, b)
/ unsigned a, b;
.globl	pscale_

pscale_:				/[FIX]
ld	r1, SS|2+2*[1](spo)		/ a
mult	rr0, SS|2+2*[2](spo)		/ a*b
ld	r1, r0			/ return high half
ret

/ Wait for next interrupt.
/ This is where Coherent lives most
/ of the time.
.globl	_idle_

_idle_:
ei	VI
halt
ret

/ System data area

.shrd

/ Initial code that gets /etc/init
/ running in user mode.  This is
/ in data space because it gets
/ copied into the user space later.
.globl	icodep_, icodes_

icode:
subl	rr0, rr0		/ NULL pointer
pushl	(sp), rr0
ld	r2, $USEG		/ Segment #
ld	r3, $argv-icode
pushl	(sp), rr2
ld	r3, $file-icode
pushl	(sp), rr2
pushl	(sp), rr0		/ PC of routine
sys	11		/ exec
.word	0xE8FF		/ jr	. [BN!!]

argv:	.word	USEG, file-icode
.word	USEG, swap-icode
.word	USEG, driv_no-icode	/ default console; vidsel may repoint this
.long	0

file:	.ascii	"/etc/init"
.byte	0
swap:			/ no swapper exec by init
.byte	0
/ Console driver names.  The argv[2] offset above is patched by vidsel
/ (called from `start') to driv_lr/driv_hr when a video card is probed;
/ it is left at driv_no (serial console) when no display is present.
driv_lr: .ascii	"/drv/lrtty"		/ low-res text card
.byte	0
driv_hr: .ascii	"/drv/hrtty"		/ hi-res bitmap card
.byte	0
driv_no: .ascii	"/drv/notty"		/ no display: serial console
.byte	0
/ driv2:	.ascii	"/drv/al"
/	.byte	0
.even
icend:
icodep_:	.long	icode
icodes_:	.word	icend-icode

.globl	ddtregp_, depth_
ddtregp_: .blkl	1			/ SP for finding registers
depth_:	.word	0			/ Stack depth (whether user or sys)
