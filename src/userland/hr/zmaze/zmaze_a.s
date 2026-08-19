/ zmaze_a.s - hand-written inner loops for the zmaze raycaster.
/
/ ABI (see the libc .s files, and it is load-bearing: PCC keeps live values
/ in r6-r14 across calls): ONLY r0-r5 are scratch.  Every routine here stays
/ inside r0-r5 or explicitly saves what more it uses.
/ Args are at rr14(4) upward (segmented calr pushes a 4-byte return).
/
/ blitfb(src)          - bench-only fixed-position viewport blit (SEG0 rows
/                        260.., byte col 44); the GUI presents via cl_blit.
/ colfill(fp, ph, n)   - *fp = ph[i & 3], fp += 40, n rows (phase-aligned
/                        4-byte dither pattern), unrolled by 4.
/ castray(rx, ry, sp)  - integer DDA over flatmap[] (16 bytes per row, so
/                        x steps +-1 and y steps +-16 on one pointer);
/                        returns the 8.8 perpendicular distance, *sp = side.
/                        Setup uses recip[] and a 16x16 MULT (~70 cycles)
/                        where C promoted to a ~280-cycle multl.
/ mixrows(fp, y0, y1)  - the ragged-edge compositor: rows y0..y1-1 of one
/                        byte column, each pixel k chosen ceiling/wall/floor
/                        by rtop[k]/rbot[k], wall ink from colpb[y&3][k],
/                        floor from floorpat[y&3].  The row loop is unrolled
/                        by 4 so the y&3 phase (and with it every pattern
/                        address) is a CONSTANT in each unrolled slot; the
/                        8 pixels are unrolled with constant addresses too.
/                        MECHANICALLY GENERATED (tools note in zmcore.c) -
/                        edit the generator in the repo history, not the
/                        32 pixel blocks by hand.

	.globl	blitfb_
	.globl	colfill_
	.globl	castray_
	.globl	castcol_
	.globl	mixrows_
	.globl	px_
	.globl	py_
	.globl	recip_
	.globl	flatmap_
	.globl	rtop_
	.globl	rbot_
	.globl	rpat_
	.globl	colpb_
	.globl	floorpat_
	.globl	camx_
	.globl	hgt_
	.globl	cdrx_
	.globl	cdry_
	.globl	cplx_
	.globl	cply_
	.globl	cres_
	.globl	csame_
	.globl	emitspan_
	.globl	colmm_
	.globl	ctop_
	.globl	cbot_
	.globl	cpat_
	.globl	cfill_
	.globl	colsleft_
	.globl	dhw_

blitfb_:
	ldl	rr2, rr14(4)		/ src
	ldl	rr4, $0x3a00822c	/ SEG0 + 260*128 + 44
	ld	r1, $200		/ rows
1:
	ld	r0, $20			/ words per row
	ldir	(rr4), (rr2), r0	/ copy row; rr2,rr4 advance 40
	add	r5, $88			/ dst to next screen row (128-40)
	dec	r1
	jr	nz, 1b
	ret

colfill_:
	ldl	rr2, rr14(4)		/ fp
	ldl	rr4, rr14(8)		/ ph
	ld	r0, (rr4)		/ rh0 = ph[0], rl0 = ph[1]
	inc	r5, $2
	ld	r1, (rr4)		/ rh1 = ph[2], rl1 = ph[3]
	ld	r4, rr14(12)		/ n (rr4 is free now)
	test	r4
	ret	le			/ nothing to fill
/ whole groups of 4 rows: one dec+jr per group instead of per row
	ld	r5, r4
	sra	r5, $2			/ r5 = n >> 2 groups
	jr	z, 5f
	and	r4, $3			/ r4 = remainder rows
1:
	ldb	(rr2), rh0
	add	r3, $40
	ldb	(rr2), rl0
	add	r3, $40
	ldb	(rr2), rh1
	add	r3, $40
	ldb	(rr2), rl1
	add	r3, $40
	dec	r5
	jr	nz, 1b
5:
	test	r4			/ 0..3 leftover rows
	ret	z
	ldb	(rr2), rh0
	add	r3, $40
	dec	r4
	jr	z, 9f
	ldb	(rr2), rl0
	add	r3, $40
	dec	r4
	jr	z, 9f
	ldb	(rr2), rh1
	add	r3, $40
9:
	ret

/ castray(rx, ry, sidep): the C-callable wrapper over crcore below.
castray_:
	pushl	(rr14), rr6
	ld	r4, rr14(8)		/ rx
	ld	r5, rr14(10)		/ ry
	calr	crcore
	ldl	rr4, rr14(12)		/ *sidep = side
	ld	(rr4), r0
	popl	rr6, (rr14)
	ret

/ crcore: the DDA proper.  In: r4 = rx, r5 = ry (8.8 ray direction).
/ Out: r1 = perpendicular distance (8.8), r0 = side (0 x-wall, 1 y-wall).
/ Clobbers r0-r7 (callers save r6/r7 for their own callers).
/   r0 = sdx   r1 = sdy   rr2 = map pointer   r4 = ddx   r5 = ddy
/   r6 = x step (+-1)     r7 = y step (+-16)
crcore:
	ld	r0, py_			/ pm = flatmap + (my<<4) + mx
	srl	r0, $4
	and	r0, $0x00f0
	ld	r1, px_
	srl	r1, $8
	or	r0, r1
	ldl	rr2, $flatmap_
	add	r3, r0

/ Y axis first: its MULT uses rr0 while r0 is still free
	test	r5
	jr	eq, 5f
	jr	gt, 3f
	neg	r5
	ld	r7, $-16
	ld	r1, py_
	and	r1, $255		/ frac = py & 255
	jr	4f
3:
	ld	r7, $16
	ld	r1, py_
	and	r1, $255
	neg	r1
	add	r1, $256		/ frac = 256 - (py & 255)
4:
	sll	r5, $1
	ld	r5, recip_(r5)		/ ddy
	mult	rr0, r5			/ rr0 = frac * ddy
	srll	rr0, $8			/ sdy lands in r1
	jr	6f
5:
	ld	r7, $0
	ld	r1, $0x7fff		/ sdy = INF
	ld	r5, $0
6:
/ X axis (rx already in r4): r1 (sdy) is live, shelter it across the MULT
	test	r4
	jr	eq, 7f
	jr	gt, 8f
	neg	r4
	ld	r6, $-1
	ld	r0, px_
	and	r0, $255
	jr	9f
8:
	ld	r6, $1
	ld	r0, px_
	and	r0, $255
	neg	r0
	add	r0, $256
9:
	sll	r4, $1
	ld	r4, recip_(r4)		/ ddx
	push	(rr14), r1
	ld	r1, r0
	mult	rr0, r4
	srll	rr0, $8
	ld	r0, r1			/ sdx
	pop	r1, (rr14)
	jr	1f
7:
	ld	r6, $0
	ld	r0, $0x7fff
	ld	r4, $0

/ the DDA proper: step the shorter side distance, test the wall byte
1:
	cp	r0, r1
	jr	ge, 2f
	add	r3, r6			/ x step
	testb	(rr2)
	jr	nz, 8f			/ hit an x-side wall
	add	r0, r4
	jr	pl, 1b
	ld	r0, $0x7fff		/ clamp overflow to INF
	jr	1b
2:
	add	r3, r7			/ y step
	testb	(rr2)
	jr	nz, 9f			/ hit a y-side wall
	add	r1, r5
	jr	pl, 1b
	ld	r1, $0x7fff
	jr	1b
8:
	ld	r1, r0			/ x-side hit: perp = sdx, side = 0
	sub	r0, r0
	ret
9:
	ld	r0, $1			/ y-side hit: perp = sdy (already r1)
	ret

/ castcol(bx): cast all 8 rays of byte column bx and fold the per-column
/ reductions in.  Reads the per-frame globals cdrx/cdry (dir) and cplx/cply
/ (camera plane) plus camx/recip/hgt/flatmap/px/py; writes rtop[8]/rbot[8]/
/ rpat[8] and cres[4] = {t0,t1,b0,b1}, csame.  The point over the old C loop:
/ the two plane products per ray run on the 16x16 MULT (~70 cycles) instead
/ of the promoted multl the compiler emits (~280) - that alone is ~1.1M
/ cycles/frame on real hardware - and the reductions ride along in registers.
/   r8 = t0   r9 = t1   r10 = b0   r11 = b1
/   r12 = camx byte offset   r13 = 2k   (r6-r13 saved; crcore eats r0-r7)
castcol_:
	dec	r15, $16
	ldm	(rr14), r6, $8		/ save r6-r13
	ld	r12, rr14(20)		/ bx (16 saved bytes + 4 return)
	sll	r12, $4			/ camx byte offset = bx*8 rays *2
	sub	r13, r13		/ k = 0
	ld	r8, $200		/ t0 = min start
	ld	r10, $200		/ b0
	sub	r9, r9			/ t1 = max start
	sub	r11, r11		/ b1
	ld	r0, $1
	ld	csame_, r0
1:
	ld	r1, camx_(r12)		/ cx
	mult	rr0, cplx_		/ planex * cx, 16x16
	sral	rr0, $8
	add	r1, cdrx_
	ld	r4, r1			/ rx
	ld	r1, camx_(r12)
	mult	rr0, cply_
	sral	rr0, $8
	add	r1, cdry_
	ld	r5, r1			/ ry
	calr	crcore			/ -> r1 = perp, r0 = side
	cp	r1, $24
	jr	ge, 2f
	ld	r1, $24
2:
	cp	r1, $4092
	jr	le, 2f
	ld	r1, $4092
2:
	ld	r3, r1			/ h = hgt[perp >> 2]
	srl	r3, $1
	and	r3, $0xfffe
	ld	r3, hgt_(r3)
	ld	r2, $200		/ top = (H - h) >> 1
	sub	r2, r3
	srl	r2, $1
	ld	rtop_(r13), r2
	cp	r2, r8
	jr	ge, 2f
	ld	r8, r2
2:
	cp	r2, r9
	jr	le, 2f
	ld	r9, r2
2:
	ld	r2, $200		/ bottom = (H + h) >> 1
	add	r2, r3
	srl	r2, $1
	ld	rbot_(r13), r2
	cp	r2, r10
	jr	ge, 2f
	ld	r10, r2
2:
	cp	r2, r11
	jr	le, 2f
	ld	r11, r2
2:
	ld	r2, r1			/ half-cell distance band, max 3...
	srl	r2, $7
	cp	r2, $3
	jr	le, 2f
	ld	r2, $3
2:
	sll	r2, $1			/ ...doubled onto the 8-level scale
	test	r0			/ x-side walls a HALF-step darker
	jr	nz, 2f
	inc	r2, $1
2:
	ld	rpat_(r13), r2
	test	r13
	jr	z, 2f
	cp	r2, rpat_
	jr	eq, 2f
	sub	r0, r0
	ld	csame_, r0
2:
	inc	r12, $2
	inc	r13, $2
	cp	r13, $16
	jr	lt, 1b
	ldm	cres_, r8, $4		/ t0, t1, b0, b1
	ldm	r6, (rr14), $8		/ restore r6-r13
	inc	r15, $16
	ret

/ emitspan(x0, x1, hacc, dh, vert): the BSP span inner loop.
/ Columns x0..x1 (already clipped to 0..319): skip filled ones, else mark
/ filled, derive the shade band and clamped height from the interpolated
/ unclamped height hacc (8.8 in a long), and write ctop/cbot/cpat[x].
/ hacc advances by dh (long, stashed in dhw) every column, taken or not.
/ After the pushl: x0 @rr14(8), x1 @(10), hacc @(12), dh @(16), vert @(20).
/   rr6 = hacc   r1 = x (r0 is no index register)   r0 = x1   rr2/r4/r5 scratch
emitspan_:
	pushl	(rr14), rr6
	ldl	rr6, rr14(12)		/ hacc
	ldl	rr2, rr14(16)
	ldl	dhw_, rr2		/ dh parked in memory
	ld	r1, rr14(8)		/ x0
	ld	r0, rr14(10)		/ x1
1:
	cp	r1, r0
	jr	gt, 9f
	ldb	rl4, cfill_(r1)
	testb	rl4
	jr	nz, 8f
	ldb	rl4, $1
	ldb	cfill_(r1), rl4
	dec	colsleft_
	ldl	rr4, rr6
	srll	rr4, $8			/ r5 = unclamped height hh
	sub	r4, r4			/ half-cell distance band, 0..3
	cp	r5, $400		/ (hh thresholds = 51200/(128k))
	jr	gt, 2f
	inc	r4, $2			/ band steps are 2 on the 8-level scale
	cp	r5, $200
	jr	gt, 2f
	inc	r4, $2
	cp	r5, $133
	jr	gt, 2f
	inc	r4, $2
2:
	ld	r3, rr14(20)		/ + vert: x-side walls a HALF-step darker
	add	r4, r3
	cp	r5, $200		/ h = min(hh, 200)
	jr	le, 2f
	ld	r5, $200
2:
	ld	r2, r1
	add	r2, r2			/ int index -> byte offset
	ld	cpat_(r2), r4
	ld	r3, $200
	sub	r3, r5
	srl	r3, $1
	ld	ctop_(r2), r3
	ld	r3, $200
	add	r3, r5
	srl	r3, $1
	ld	cbot_(r2), r3
8:
	addl	rr6, dhw_		/ hacc += dh
	inc	r1, $1
	jr	1b
9:
	popl	rr6, (rr14)
	ret

/ colmm(bx): the BSP mode twin of the castcol reduction tail -- copy column
/ slice bx*8..+7 from ctop/cbot/cpat into rtop/rbot/rpat and fold t0/t1/
/ b0/b1/same, exactly as castcol publishes them (cres[4], csame).
colmm_:
	dec	r15, $16
	ldm	(rr14), r6, $8		/ save r6-r13
	ld	r12, rr14(20)		/ bx
	sll	r12, $4			/ byte offset of column bx*8
	sub	r13, r13		/ 2k
	ld	r8, $200		/ t0
	ld	r10, $200		/ b0
	sub	r9, r9			/ t1
	sub	r11, r11		/ b1
	ld	r1, $1
	ld	csame_, r1
1:
	ld	r2, ctop_(r12)
	ld	rtop_(r13), r2
	cp	r2, r8
	jr	ge, 2f
	ld	r8, r2
2:
	cp	r2, r9
	jr	le, 2f
	ld	r9, r2
2:
	ld	r2, cbot_(r12)
	ld	rbot_(r13), r2
	cp	r2, r10
	jr	ge, 2f
	ld	r10, r2
2:
	cp	r2, r11
	jr	le, 2f
	ld	r11, r2
2:
	ld	r2, cpat_(r12)
	ld	rpat_(r13), r2
	test	r13
	jr	z, 2f
	cp	r2, rpat_
	jr	eq, 2f
	sub	r1, r1
	ld	csame_, r1
2:
	inc	r12, $2
	inc	r13, $2
	cp	r13, $16
	jr	lt, 1b
	ldm	cres_, r8, $4
	ldm	r6, (rr14), $8
	inc	r15, $16
	ret

/ mixrows(fp, y0, y1): args after the pushl: rr14(8) fp, rr14(12) y0,
/ rr14(14) y1.  r0 = y, rl1 = row byte, rr2 = fp, r4/r5 scratch, r6 = y1.
mixrows_:
	pushl	(rr14), rr6
	ldl	rr2, rr14(8)
	ld	r0, rr14(12)
	ld	r6, rr14(14)
	cp	r0, r6
	jr	lt, 1f
	jp	mxdone
1:
	ld	r4, r0
	and	r4, $3
	test	r4
	jr	nz, 1f
	jp	mxp0
1:
	cp	r4, $1
	jr	nz, 1f
	jp	mxp1
1:
	cp	r4, $2
	jr	nz, 1f
	jp	mxp2
1:
	jp	mxp3
mxp0:
	clrb	rl1
	ld	r5, rtop_+0
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+0
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+0
	andb	rl5, $0x80
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x80
	orb	rl1, rl5
1:
	ld	r5, rtop_+2
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+2
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+1
	andb	rl5, $0x40
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x40
	orb	rl1, rl5
1:
	ld	r5, rtop_+4
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+4
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+2
	andb	rl5, $0x20
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x20
	orb	rl1, rl5
1:
	ld	r5, rtop_+6
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+6
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+3
	andb	rl5, $0x10
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x10
	orb	rl1, rl5
1:
	ld	r5, rtop_+8
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+8
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+4
	andb	rl5, $0x08
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x08
	orb	rl1, rl5
1:
	ld	r5, rtop_+10
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+10
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+5
	andb	rl5, $0x04
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x04
	orb	rl1, rl5
1:
	ld	r5, rtop_+12
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+12
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+6
	andb	rl5, $0x02
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x02
	orb	rl1, rl5
1:
	ld	r5, rtop_+14
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+14
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+7
	andb	rl5, $0x01
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+0
	andb	rl5, $0x01
	orb	rl1, rl5
1:
	ldb	(rr2), rl1
	add	r3, $40
	inc	r0, $1
	cp	r0, r6
	jr	lt, 1f
	jp	mxdone
1:
mxp1:
	clrb	rl1
	ld	r5, rtop_+0
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+0
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+8
	andb	rl5, $0x80
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x80
	orb	rl1, rl5
1:
	ld	r5, rtop_+2
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+2
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+9
	andb	rl5, $0x40
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x40
	orb	rl1, rl5
1:
	ld	r5, rtop_+4
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+4
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+10
	andb	rl5, $0x20
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x20
	orb	rl1, rl5
1:
	ld	r5, rtop_+6
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+6
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+11
	andb	rl5, $0x10
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x10
	orb	rl1, rl5
1:
	ld	r5, rtop_+8
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+8
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+12
	andb	rl5, $0x08
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x08
	orb	rl1, rl5
1:
	ld	r5, rtop_+10
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+10
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+13
	andb	rl5, $0x04
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x04
	orb	rl1, rl5
1:
	ld	r5, rtop_+12
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+12
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+14
	andb	rl5, $0x02
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x02
	orb	rl1, rl5
1:
	ld	r5, rtop_+14
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+14
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+15
	andb	rl5, $0x01
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+1
	andb	rl5, $0x01
	orb	rl1, rl5
1:
	ldb	(rr2), rl1
	add	r3, $40
	inc	r0, $1
	cp	r0, r6
	jr	lt, 1f
	jp	mxdone
1:
mxp2:
	clrb	rl1
	ld	r5, rtop_+0
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+0
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+16
	andb	rl5, $0x80
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x80
	orb	rl1, rl5
1:
	ld	r5, rtop_+2
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+2
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+17
	andb	rl5, $0x40
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x40
	orb	rl1, rl5
1:
	ld	r5, rtop_+4
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+4
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+18
	andb	rl5, $0x20
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x20
	orb	rl1, rl5
1:
	ld	r5, rtop_+6
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+6
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+19
	andb	rl5, $0x10
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x10
	orb	rl1, rl5
1:
	ld	r5, rtop_+8
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+8
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+20
	andb	rl5, $0x08
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x08
	orb	rl1, rl5
1:
	ld	r5, rtop_+10
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+10
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+21
	andb	rl5, $0x04
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x04
	orb	rl1, rl5
1:
	ld	r5, rtop_+12
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+12
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+22
	andb	rl5, $0x02
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x02
	orb	rl1, rl5
1:
	ld	r5, rtop_+14
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+14
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+23
	andb	rl5, $0x01
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+2
	andb	rl5, $0x01
	orb	rl1, rl5
1:
	ldb	(rr2), rl1
	add	r3, $40
	inc	r0, $1
	cp	r0, r6
	jr	lt, 1f
	jp	mxdone
1:
mxp3:
	clrb	rl1
	ld	r5, rtop_+0
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+0
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+24
	andb	rl5, $0x80
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x80
	orb	rl1, rl5
1:
	ld	r5, rtop_+2
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+2
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+25
	andb	rl5, $0x40
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x40
	orb	rl1, rl5
1:
	ld	r5, rtop_+4
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+4
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+26
	andb	rl5, $0x20
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x20
	orb	rl1, rl5
1:
	ld	r5, rtop_+6
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+6
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+27
	andb	rl5, $0x10
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x10
	orb	rl1, rl5
1:
	ld	r5, rtop_+8
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+8
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+28
	andb	rl5, $0x08
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x08
	orb	rl1, rl5
1:
	ld	r5, rtop_+10
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+10
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+29
	andb	rl5, $0x04
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x04
	orb	rl1, rl5
1:
	ld	r5, rtop_+12
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+12
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+30
	andb	rl5, $0x02
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x02
	orb	rl1, rl5
1:
	ld	r5, rtop_+14
	cp	r0, r5
	jr	lt, 1f
	ld	r5, rbot_+14
	cp	r0, r5
	jr	ge, 2f
	ldb	rl5, colpb_+31
	andb	rl5, $0x01
	orb	rl1, rl5
	jr	1f
2:
	ldb	rl5, floorpat_+3
	andb	rl5, $0x01
	orb	rl1, rl5
1:
	ldb	(rr2), rl1
	add	r3, $40
	inc	r0, $1
	cp	r0, r6
	jr	lt, 1f
	jp	mxdone
1:
	jp	mxp0
mxdone:
	popl	rr6, (rr14)
	ret
