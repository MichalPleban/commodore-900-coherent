
#define srcptr rr6
#define srclow r7
#define srchi rh6
#define dstptr rr4
#define dstlow r5
#define dsthi rh4
#define fpline r1
#define lines r11
#define mask r10
#define dst2 rr12
#define dst2h r12
#define dst2l r13


.prvi

.globl smt7, smt8, smt9, smt10, smt11, smt12, smt13, smt14, smt15
.globl smb7, smb8, smb9, smb10, smb11, smb12, smb13, smb14, smb15
.globl snb12

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src | dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt7:				// src  | dst
	 

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
	add	srclow, BLT_sinc_	// increment src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt7	// while more lines, repeat
	jp	Small_ret
	 
smb7:				// src  | dst
	 

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
	sub	srclow, BLT_sinc_	// increment src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb7	// while more lines, repeat
	jp	Small_ret
	 
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) | (~dst)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt8:				// (~src) & (~dst)
	 
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
	com	r3
	com	r9		// (~src)  &  ( ~dst)
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

	djnz	lines, smt8	// while more lines, repeat
	jp	Small_ret
	 
smb8:				// (~src) & (~dst)
	 
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
	com	r3
	com	r9		// (~src)  &  ( ~dst)
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

	djnz	lines, smb8	// while more lines, repeat
	jp	Small_ret
	 
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) ^ dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt9:				// (~src) ^ dst
	 
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
	// THE OPERATION	(~src) XOR dst
	com	r3
	xor	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	add	srclow, BLT_sinc_	// increment src ^
	adcb	srchi, rh0	// check for segment overflow
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt9	// while more lines, repeat
	jp	Small_ret
	 
smb9:				// (~src) ^ dst
	 
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
	// THE OPERATION	(~src) XOR dst
	com	r3
	xor	r3, r9
	and	r3, mask
	// Shift the answer back over to line up with the hole and add it in
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2	// store it back in

	sub	r0, r0
	sub	srclow, BLT_sinc_	// increment src ^
	sbcb	srchi, rh0	// check for segment overflow
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb9	// while more lines, repeat
	jp	Small_ret
	 
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = ~dst... pattern doesn't matter
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt10:			// ~dst... pattern doesn't matter
	 
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
	//  The operation   (~dst)
	com	r3
	and 	r3, mask
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2


	sub	r0, r0
	add	dstlow, BLT_dinc_	// increment dst ^
	adcb	dsthi, rh0	// check for segment overflow

	djnz	lines, smt10	// while more lines, repeat
	jp	Small_ret
	 
smb10:			// ~dst... pattern doesn't matter
	 
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
	//  The operation   (~dst)
	com	r3
	and 	r3, mask
	sub	r2, r2
	ld	r0, ds_
	sdll	rr2, r0
	addl	rr2, dst2
	ldl	@dstptr, rr2


	sub	r0, r0
	sub	dstlow, BLT_dinc_	// increment dst ^
	sbcb	dsthi, rh0	// check for segment overflow

	djnz	lines, smb10	// while more lines, repeat
	jp	Small_ret
	 
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 			OP = src | (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt11:			// src | (~dst)
	 
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
	// THE OPERATION	src | (~dst)
	com	r3
	or	r3, r9
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

	djnz	lines, smt11	// while more lines, repeat
	jp	Small_ret
	 
smb11:			// src | (~dst)
	 
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
	// THE OPERATION	src | (~dst)
	com	r3
	or	r3, r9
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

	djnz	lines, smb11	// while more lines, repeat
	jp	Small_ret
	 
// 	
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = ~src
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
snb12:			// ~src
	 
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
	com	r9
	and	r9, mask
	ld	r3, r9
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

	djnz	lines, snb12	// while more lines, repeat
	jp	Small_ret

	 
smt12:			// ~src
	 
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
	com	r9
	and	r9, mask
	ld	r3, r9
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

	djnz	lines, smt12	// while more lines, repeat
	jp	Small_ret
	 
smb12:			// ~src
	 
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
	com	r9
	and	r9, mask
	ld	r3, r9
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

	djnz	lines, smb12	// while more lines, repeat
	jp	Small_ret

	 
// 	
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) | dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt13:			// (~src) | dst
	 
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
	// THE OPERATION	(~src) | dst
	com	r9
	or	r3, r9
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

	djnz	lines, smt13	// while more lines, repeat
	jp	Small_ret
	 
smb13:			// (~src) | dst
	 
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
	// THE OPERATION	(~src) | dst
	com	r9
	or	r3, r9
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

	djnz	lines, smb13	// while more lines, repeat
	jp	Small_ret
	 
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 			OP = (~src) | (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt14:			// (~src) | (~dst)
	 
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
	// THE OPERATION	(~src) | (~dst)
	com	r3
	com	r9
	or	r3, r9
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

	djnz	lines, smt14	// while more lines, repeat
	jp	Small_ret
	 
smb14:			// (~src) | (~dst)
	 
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
	// THE OPERATION	(~src) | (~dst)
	com	r3
	com	r9
	or	r3, r9
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

	djnz	lines, smb14	// while more lines, repeat
	jp	Small_ret
	 
// 
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = true... all ones
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
smt15:			// true...all 1's
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
				// now add the full FFFF mask in
	sub	r2, r2
	ld	r3, mask
	ld	r0, ds_
	sdll	rr2, r0
	and	r2, spat_(fpline)// and in the pattern
	and	r3, spat_(fpline)// again
	addl	dst2, rr2
	ldl	@dstptr, dst2

	sub	r0, r0
	add	dstlow, BLT_dinc_	// increment the dst ^
	adcb	dsthi, rh0	// check for segment overflow
	inc	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smt15	// while more lines, repeat
	jp	Small_ret
	 
smb15:			// true...all 1's
	 
	// first create the dst word with a hole in it for the new stuff
	ldl	dst2, mask2_	// mask with hole in it
	ldl	rr2, @dstptr	// dst word
	and	dst2h, r2	// 'and' them together
	and	dst2l, r3	//   "    "     "
				// now add the full FFFF mask in
	sub	r2, r2
	ld	r3, mask
	ld	r0, ds_
	sdll	rr2, r0
	and	r2, spat_(fpline)
	and	r3, spat_(fpline)// again
	addl	dst2, rr2
	ldl	@dstptr, dst2

	sub	r0, r0
	sub	dstlow, BLT_dinc_	// increment the dst ^
	sbcb	dsthi, rh0	// check for segment overflow
	dec	fpline, $2	// increment pattern index
	and	fpline, $31	// with wrap around at 16th line

	djnz	lines, smb15	// while more lines, repeat
	jp	Small_ret
	 
