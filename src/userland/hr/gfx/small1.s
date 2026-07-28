
#define srcptr rr6
#define srclow r7
#define srchi r7
#define dstptr rr4
#define dstlow r5
#define dsthi rh4
#define fpline r1
#define lines r11
#define mask r10
#define dst2 rr12
#define dst2h r12
#define dst2l r13


.globl Small_ret
.prvi


// 
TMcode:
.long	smt0
.long	smt1
.long	smt2
.long	smt3
.long	smt4
.long	smb5
.long	smt6
.long	smt7
.long	smt8
.long	smt9
.long	smt10
.long	smt11
.long	smt12
.long	smt13
.long	smt14
.long	smt15
BMcode:
.long	smb0
.long	smb1
.long	smb2
.long	smb3
.long	smb4
.long	smb5
.long	smb6
.long	smb7
.long	smb8
.long	smb9
.long	smb10
.long	smb11
.long	smb12
.long	smb13
.long	smb14
.long	smb15

TNcode:
.long	smt0
.long	smt1
.long	smt2
.long	snt3	// different
.long	smt4
.long	smb5
.long	snt6	// different
.long	smt7
.long	smt8
.long	smt9
.long	smt10
.long	smt11
.long	smt12
.long	smt13
.long	smt14
.long	smt15
BNcode:
.long	smb0
.long	smb1
.long	smb2
.long	snb3
.long	smb4
.long	smb5
.long	snb6
.long	smb7
.long	smb8
.long	smb9
.long	smb10
.long	smb11
.long	snb12
.long	smb13
.long	smb14
.long	smb15



// 
.globl BLT_small_

BLT_small_:	// 
     
	lda	rr4, sreg_
	ldm	@rr4, r8, $6

	ldl	rr2, BLT_pat_ptr_// copy the stippling pattern into
	lda	rr4, spat_	// into a private area
	ld	r0, $16
	ldir	@rr4, @rr2, r0

				// mask = BLT_tbl1[width]
	ld	r5, BLT_width_	// get width
	ld	r1, r5		// copy width into r1
	sll	r5		// double for offset into table 1
	ld	mask, BLT_tbl1_(r5)
				// mask2 = (~BLT_tbl2[width] >> dptr.bit )
	sub	r3,r3
	ld	r2, BLT_tbl2_(r5)
	ld	r5, BLT_dptrb_	// r5 contains dptr.bit
	neg	r5
	sdll	rr2, r5
	com	r2
	com	r3
	ldl	mask2_, rr2
				// now compute the shift amount to src and dst.
				// ss = 32 - width - sptr.bit
				// ds = 32 - width - dptr.bit
	ld	r5, BLT_dptrb_
	ld	r6, $32		// r6 = 32
	sub	r6, r1		// r6 -= width
	ld	r4, r6		// for dptr
	sub	r4, r5		// old r6 -= dptr.bit
	ld	ds_, r4		// save into ds_
	ld	r4, r6		// again get 32 - width
	ld	r5, BLT_sptrb_	// load sptr.bit
	sub	r4, r5
	ld	ss_, r4
	ldl	srcptr, BLT_sptrw_
	ldl	dstptr, BLT_dptrw_
	ld	lines, BLT_height_
	ld	fpline, BLT_pat_index_
				// initial setups now complete
	ld	r8, BLT_op_
	sll	r8, $2		// double op for index into table
	test	spao_
	jr	z, pat_matters
	test	BLT_direction_
	jr	z, blt_bot2
	lda	rr2, TNcode
	add	r3, r8
	ldl	rr8, @rr2
	jp	@rr8
blt_bot2:
	lda	rr2, BNcode
	add	r3, r8
	ldl	rr8, @rr2
	jp	@rr8

pat_matters:
	test	BLT_direction_
	jr	z, blt_bottom
	lda	rr2, TMcode	// base of top-down code
	add	r3, r8		// offset of logical op code
	ldl	rr8, @rr2	// ld the.longess
	jp	@rr8		// and go there
blt_bottom:
	lda	rr2, BMcode
	add	r3, r8
	ldl	rr8, @rr2
	jp	@rr8		// jump to logical routine

Small_ret:
	// restore rr8-rr14 and return
	lda	rr4, sreg_
	ldm	r8, @rr4, $6
	ret
	 
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 			OP = false  
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt0:	// false... pattern doesn't matter
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	ldl	@dstptr, dst2
	sub	r0, r0
	add	dstlow, BLT_dinc_// inc dst ^
	adcb	dsthi, rh0	// check for segment overflow

	djnz	lines, smt0// while more raster lines, repeat
	jp	Small_ret		// else return
	 
smb0:	// false... pattern doesn't matter
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	ldl	@dstptr, dst2
	sub	r0, r0
	sub	dstlow, BLT_dinc_// inc dst ^
	sbcb	dsthi, rh0	// check for segment overflow

	djnz	lines, smb0// while more raster lines, repeat
	jp	Small_ret		// else return
	 
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src & dst
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt1:				// src & dst
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in low r3

	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r9, spat_(fpline)// src = src & pattern
	and	r8, spat_(fpline)
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for change in low r5

	// THE OPERATION
	and	r3, r9		// dst = dst & src
	and 	r3, mask	// dst = dst & mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and 	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt1// while more raster lines, repeat
	jp	Small_ret
	 
smb1:				// src & dst
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in low r3

	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r9, spat_(fpline)// src = src & pattern
	and	r8, spat_(fpline)
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for change in low r5

	// THE OPERATION
	and	r3, r9		// dst = dst & src
	and 	r3, mask	// dst = dst & mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and 	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb1// while more raster lines, repeat
	jp	Small_ret
	 
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src & (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt2:				// src & (~dst)
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	com	r3		// src & (~dst)
	and	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt2// while more raster lines, repeat
	jp	Small_ret
	 
smb2:				// src & (~dst)
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	com	r3		// src & (~dst)
	and	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb2// while more raster lines, repeat
	jp	Small_ret
	 
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
snt3:				// src
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	ldl	rr8, @srcptr	// load thirty two bits of source
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	and 	r9, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r8, r8
	ld	r0, ds_
	sdll	rr8, r0
	addl	rr8, dst2
	ldl	@dstptr, rr8	// store it back in 

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow

	djnz	lines, snt3	// while more raster lines, repeat
	jp	Small_ret
snb3:				// src
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	ldl	rr8, @srcptr	// load thirty two bits of source
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	and 	r9, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r8, r8
	ld	r0, ds_
	sdll	rr8, r0
	addl	rr8, dst2
	ldl	@dstptr, rr8	// store it back in 

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow

	djnz	lines, snb3	// while more raster lines, repeat
	jp	Small_ret
	 
smt3:				// src
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	and 	r9, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r8, r8
	ld	r0, ds_
	sdll	rr8, r0
	addl	rr8, dst2
	ldl	@dstptr, rr8	// store it back in 

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt3	// while more raster lines, repeat
	jp	Small_ret
	 
smb3:				// src
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	and 	r9, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r8, r8
	ld	r0, ds_
	sdll	rr8, r0
	addl	rr8, dst2
	ldl	@dstptr, rr8	// store it back in 

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb3	// while more raster lines, repeat
	jp	Small_ret
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) & dst
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt4:				// (~src) & dst
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	com	r9		// (~src)  &  dst
	and	r3, r9
	and	r3, mask

	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt4	// while more raster lines, repeat
	jp	Small_ret
	 
smb4:				// (~src) & dst
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	com	r9		// (~src)  &  dst
	and	r3, r9
	and	r3, mask

	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb4	// while more raster lines, repeat
	jp	Small_ret
	 
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = dst... among the stupidest ops
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smb5:				// dst ... kinda silly
	 
	jp	Small_ret
	 
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src ^ dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
snt6:				// src ^ dst
	 

	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "

	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	xor	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in 

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow

	djnz	lines, snt6	// while more raster lines, repeat
	jp	Small_ret
	 
// 
snb6:				// src ^ dst
	 

	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "

	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	xor	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in 

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow

	djnz	lines, snb6	// while more raster lines, repeat
	jp	Small_ret
	 
smt6:				// src ^ dst
	 

	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "

	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	xor	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in 

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment the src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt6	// while more raster lines, repeat
	jp	Small_ret
	 
smb6:				// src ^ dst
	 

	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "

	// now perform the operation ... the dst word is still safe in rr2
	ld	r0, ds_
	neg	r0		// must be negative for right movement
	sdll	rr2, r0		// the bits to change are in least sig bits
				// of r3
	ldl	rr8, @srcptr	// load thirty two bits of source
	and	r8, spat_(fpline)// src = src & pattern
	and	r9, spat_(fpline)// src = src & pattern
	ld	r0, ss_
	neg	r0		// must be negative for right movement
	sdll	rr8, r0		// the bits to use for the change are now in
				// the least sig bits of r5
	// THE OPERATION
	xor	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in 

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment the src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb6	// while more raster lines, repeat
	jp	Small_ret
	 
