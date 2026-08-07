
#define srcptr rr6
#define srclow r7
#define srchi rh6
#define dstptr rr4
#define dstlow r5
#define dsthi rh4
#define fpline r1
#define lines r11
#define lmask r12
#define rmask r13
#define pline r8
#define pat r9
#define wds r10

/ compile time only
#define code rr12
#define codelow r13
#define op_off r10

/ Written at runtime (blit loop assembled here, register save area):
/ private sections, or a shared-text (ld -n) binary faults on them.
.prvi
.globl code_space_
code_space_:
.blkw 500

.prvd
.globl sreg_
sreg_:
.blkw 16
.shri

code_op_:	// the basic operation, src op dst == r0 op r1
.long bop0	// false
.long bop1	// and
.long bop2	// andn
.long bop3	// src
.long bop4	// nand
.long bop5	// dst
.long bop6	// xor
.long bop7	// or
.long bop8	// nandn
.long bop9	// nxor
.long bop10	// ndst
.long bop11	// orn
.long bop12	// nsrc
.long bop13	// nor
.long bop14	// norn
.long bop15	// true
.long bop3	// for exchange, use src = src, which is a nop


code_inT_:	// inner loop, Top-down, no mask
.long inT0	// false
.long imT1	// and	....assumes mask
.long imT2	// andn	....assumes mask
.long inT3	// src
.long imT4	// nand	....assumes mask
.long imT5	// dst	....assumes mask
.long inT6	// xor
.long imT7	// or	....assumes mask
.long imT8	// nandn....assumes mask
.long imT9	// nxor	....assumes mask
.long inT10	// ndst
.long imT11	// orn	....assumes mask
.long imT12	// nsrc	....assumes mask
.long imT13	// nor	....assumes mask
.long imT14	// norn	....assumes mask
.long inT15	// true
.long inT16	// exchange

code_imT_:	// inner loop, Top-down, mask
.long inT0	// false...since mask doesn't matter, use inT0
.long imT1	// and
.long imT2	// andn
.long imT3	// src
.long imT4	// nand
.long imT5	// dst
.long imT6	// xor
.long imT7	// or
.long imT8	// nandn
.long imT9	// nxor
.long inT10	// ndst
.long imT11	// orn
.long imT12	// nsrc
.long imT13	// nor
.long imT14	// norn
.long imT15	// true
.long inT16	// exchange

code_inB_:	// inner loop, bottom up, no mask
.long inB0	// false
.long imB1	// and	...assumes a mask
.long imB2	// andn	...assumes a mask
.long inB3	// src
.long imB4	// nand	...assumes a mask
.long imB5	// dst	...assumes a mask
.long inB6	// xor
.long imB7	// or	...assumes a mask
.long imB8	// nandn...assumes a mask
.long imB9	// nxor	...assumes a mask
.long inB10	// ndst	....mask doesn't matter
.long imB11	// orn	...assumes a mask
.long imB12	// nsrc	...assumes a mask
.long imB13	// nor	...assumes a mask
.long imB14	// norn	...assumes a mask
.long inB15	// true
.long inB16	// exchange

code_imB_:	// inner loop, bottom up, mask
.long inB0	// false... since mask doesn't matter, use imB0
.long imB1	// and
.long imB2	// andn
.long imB3	// src
.long imB4	// nand
.long imB5	// dst
.long imB6	// xor
.long imB7	// or
.long imB8	// nandn
.long imB9	// nxor
.long inB10	// ndst .... mask doesn't matter
.long imB11	// orn
.long imB12	// nsrc
.long imB13	// nor
.long imB14	// norn
.long imB15	// true
.long inB16	// exchange

//
.globl BLT_block_

BLT_block_:
	lda	rr4, sreg_
	ldm	(rr4), r8, $6

	ld	op_off, BLT_op_
	sll	op_off, $2		// get offset to op instructions
	lda	code, code_space_	// ^ to code to execute
	ld	r11, spao_		// if true (1) pattern is all ones
	test	 BLT_direction_		// true (1) if Top-to-Bottom
	jp	z, block_bottom		// if bottom-up, go to other section


	// we now know that we are going from top-down
	// put the correct values for srcinc and dstinc into the code
	// to copy
	ld	r7, BLT_sinc_
	ld	r6, BLT_dinc_
	lda	rr4, end_inner_top
	ld	rr4(10), r7		// insert correct value for sinc
	ld	rr4(28), r6		// insert correct value for dinc
	// now we begin compiling code
	// first, if the pattern matters, load the pattern
	test	r11			// false(0) if pattern matters
	jr	nz, no_pat_T1
	lda	rr2, load_mask	// instructions to put pattern
	ldk	r0, $3			// off index(pline) into the
	ldir	@code, @rr2, r0		// "pat" register
no_pat_T1:
	test	BLT_left_		// true(1) if left partial
	jr	z, inner_T
	lda	rr2, fetch_data
	ldk	r0, $3
	ldir	@code, @rr2, r0		// fetch src, dst, and dst2
	test	r11
	jr	nz, no_pat_T2
	ld	@code, $0x8790		// and 	r0, pat
	inc	codelow, $2
no_pat_T2:
	cp	op_off, $32		// true is exchange
	jr	ne, normal_LT
	lda	rr2, ex_left_top
	ldk	r0, $9
	ldir	@code, @rr2, r0
	jr	inner_T

normal_LT:
	ldl	rr2, code_op_(op_off)	// code_op is an array of.longesses
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance to actual code
	ldir	@code, @rr2, r0		// and copy it
	test	r11			// test for pattern
	jr	nz, adv_TL
	cp	BLT_op_, $15		// special case ... op = TRUE
	jr	ne, adv_TL
	ld	@code, $0x8790		// and r0, pat (again)
	inc	codelow, $2
adv_TL:
	lda	rr2, left_top
	ldk	r0, $8			// number of words here
	ldir	@code, @rr2, r0		// load it in
inner_T:
	test	BLT_words_
	jr	z, right_T		// if no words, skip it
	lda	rr2, ld_words		
	ldk	r0, $3
	ldir	@code, @rr2, r0		// ld	wds, BLT_words_
	test	r11
	jr	nz, no_pat_T3		//
	ldl	rr2, code_imT_(op_off) 	// code_inner , mask counts
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance past word count
	ldir	@code, @rr2, r0		// load da code
	jr	right_T
no_pat_T3:
	ldl	rr2, code_inT_(op_off)	// code_inner, no mask
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance past word count
	ldir	@code, @rr2, r0		// load da code, mama
right_T:
	test	BLT_right_
	jr	z, load_inner_T
	lda	rr2, fetch_data
	ldk	r0, $3
	ldir	@code, @rr2, r0
	test	r11
	jr	nz, no_pat_T4
	ld	@code, $0x8790		// and 	r0, pat
	inc	codelow, $2
no_pat_T4:
	cp 	op_off, $32		// special case exchange
	jr	ne, normal_RT
	lda	rr2, ex_right_top
	ldk	r0, $7
	ldir	@code, @rr2, r0
	jr	load_inner_T

normal_RT:
	ldl	rr2, code_op_(op_off)	// code_op is an array of.longesses
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance to actual code
	ldir	@code, @rr2, r0		// and copy it
	test 	r11
	jr	nz, adv_TR
	cp	BLT_op_, $15		// special case ... op = TRUE
	jr	ne, adv_TR
	ld	@code, $0x8790		// and r0, pat (again)
	inc	codelow, $2
adv_TR:
	lda	rr2, right_top
	ldk	r0, $6			// number of words here
	ldir	@code, @rr2, r0		// load it in

load_inner_T:
	lda	rr2, end_inner_top
	ld	r0, $19
	ldir	@code, @rr2, r0

	test	r11
	jr	nz, load_end_T
	lda	rr2, inc_pline
	ldk	r0, $3
	ldir	@code, @rr2, r0		// inc pline, $2, and pline $31
load_end_T:
	lda	rr2, bottom_of_loop
	ldk	r0, $10
	ldir	@code, @rr2, r0
	jp	compile_done
//
block_bottom:
	// replace the source and destination increment amounts in the code
	ld	r7, BLT_sinc_
	ld	r6, BLT_dinc_
	lda	rr4, end_inner_bottom
	ld	rr4(10), r7
	ld	rr4(28), r6

	// now we begin compiling code, doing the right word first, then
	// the inner words, then the left word, then decrement the lines
	// count and repeat


	// now we begin compiling code
	// first, if the pattern matters, load the pattern
	test	r11			// false(0) if pattern matters
	jr	nz, no_pat_B1
	lda	rr2, load_mask	// instructions to put pattern
	ldk	r0, $3			// off index(pline) into the
	ldir	@code, @rr2, r0		// "pat" register
no_pat_B1:
	test	BLT_right_		// true(1) if left partial
	jr	z, inner_B
	lda	rr2, fetch_data
	ldk	r0, $3
	ldir	@code, @rr2, r0		// fetch src, dst, and dst2
	cp	BLT_op_, $16		// special case exchange
	jr	ne, normal_RB
	lda	rr2, ex_right_bottom
	ldk	r0, $9
	ldir	@code, @rr2, r0
	jr	inner_B

normal_RB:
	test	r11
	jr	nz, no_pat_B2
	ld	@code, $0x8790		// and 	r0, pat
	inc	codelow, $2
no_pat_B2:
	ldl	rr2, code_op_(op_off)	// code_op is an array of.longesses
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance to actual code
	ldir	@code, @rr2, r0		// and copy it
	test	r11
	jr	nz, adv_BR
	cp	BLT_op_, $15		// special case ... op = TRUE
	jr	ne, adv_BR
	ld	@code, $0x8790		// and r0, pat (again)
	inc	codelow, $2
adv_BR:
	lda	rr2, right_bottom
	ldk	r0, $8			// number of words here
	ldir	@code, @rr2, r0		// load it in
	// right word done


inner_B:
	test	BLT_words_
	jr	z, left_B		// if no words, skip it
	lda	rr2, ld_words
	ldk	r0, $3
	ldir	@code, @rr2, r0		// ld	wds, BLT_words_
	test	r11
	jr	nz, no_pat_B3		//
	ldl	rr2, code_imB_(op_off) 	// code_inner , mask counts
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance past word count
	ldir	@code, @rr2, r0		// load da code
	jr	left_B
no_pat_B3:
	ldl	rr2, code_inB_(op_off)	// code_inner, no mask
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance past word count
	ldir	@code, @rr2, r0		// load da code, mama
	// inner words done


left_B:
	test	BLT_left_
	jr	z, load_inner_B
	lda	rr2, fetch_data
	ldk	r0, $3
	ldir	@code, @rr2, r0		// fetch src, dst, and dst2
	cp	BLT_op_, $16		// exchange
	jr	ne, normal_LB
	lda	rr2, ex_left_bottom
	ldk	r0, $7
	ldir	@code, @rr2, r0
	jr	load_inner_B

normal_LB:
	test	r11
	jr	nz, no_pat_B4
	ld	@code, $0x8790		// and 	r0, pat
	inc	codelow, $2
no_pat_B4:
	ldl	rr2, code_op_(op_off)	// code_op is an array of.longesses
	ld	r0, @rr2		// get word count
	inc	r3, $2			// advance to actual code
	ldir	@code, @rr2, r0		// and copy it
	test	r11
	jr	nz, adv_BL
	cp	BLT_op_, $15		// special case ... op = TRUE
	jr	ne, adv_BL
	ld	@code, $0x8790		// and r0, pat (again)
	inc	codelow, $2
adv_BL:
	lda	rr2, left_bottom
	ldk	r0, $6			// number of words here
	ldir	@code, @rr2, r0		// load it in

load_inner_B:
	lda	rr2, end_inner_bottom
	ld	r0, $19
	ldir	@code, @rr2, r0

	test	r11
	jr	nz, load_end_B
	lda	rr2, dec_pline
	ldk	r0, $3
	ldir	@code, @rr2, r0		// dec pline, $2  and pline, $31
load_end_B:
	lda	rr2, bottom_of_loop
	ldk	r0, $10
	ldir	@code, @rr2, r0
	jp	compile_done

//
compile_done:
	test	spao_			// true if pattern all ones
	jr	nz, nopat99
	ldl	rr2, BLT_pat_ptr_
	lda	rr4, spat_
	ld	r0, $16
	ldir	@rr4, @rr2, r0
	ld	pline, BLT_pat_index_
	jr	pat99
nopat99:
	ld	pat, $0xffff
pat99:
	ldl	srcptr, BLT_sptrw_
	ldl	dstptr, BLT_dptrw_
	ld	lmask, BLT_lmask_
	ld	rmask, BLT_rmask_
	ld	lines, BLT_height_

	jp	code_space_
	 


//
//	Code Stubs
// .prvd not .shrd: BLT_block_ patches these in place (sinc, dinc)
// so with ld -n they must be in the private, writable segment.
.prvd
fetch_data:
	ld	r0, @srcptr	// load left src word
	ld	r1, @dstptr	// load left dst word
	ld	r2, r1		// make a copy of the dst word

left_top:
	and	r0, lmask	// src = src & lmask
	ld	r3, lmask	// make a copy of the lmask
	com	r3		// complement it
	and	r2, r3		// dst2 = dst2 & ~lmask
	add	r0, r2		// src = src + dst2
	ld	@dstptr, r0	// store the src -> dst
	inc	srclow, $2	// advance the src pointer
	inc	dstlow, $2	// advance the dst pointer

left_bottom:
	and	r0, lmask	// src = src & lmask
	ld	r3, lmask	// make a copy of the lmask
	com	r3		// complement it
	and	r2, r3		// dst2 = dst2 & ~lmask
	add	r0, r2		// src = src + dst2
	ld	@dstptr, r0	// store the src -> dst

right_top:
	and	r0, rmask	// src = src & rmask
	ld	r3, rmask	// load a temp of rmask
	com	r3		// complement it
	and	r2, r3		// dst2 = dst2 & ~rmask
	add	r0, r2		// src = src + dst2
	ld	@dstptr, r0	// store the final partial word of the line

right_bottom:
	and	r0, rmask	// src = src & rmask
	ld	r3, rmask	// load a temp of rmask
	com	r3		// complement it
	and	r2, r3		// dst2 = dst2 & ~rmask
	add	r0, r2		// src = src + dst2
	ld	@dstptr, r0	// store the final partial word of the line
	dec	srclow, $2	// back the src pointer up
	dec	dstlow, $2	// back the dst pointer up

end_inner_top:
	sub	r0, r0
	ldl	srcptr, BLT_sptrw_
	add	srclow, $99	// this value will be replaced at compile time
	adcb	srchi, rh0
	ldl	BLT_sptrw_, srcptr

	ldl	dstptr, BLT_dptrw_
	add	dstlow, $99	// this value replaces at compile time
	adcb	dsthi, rh0
	ldl	BLT_dptrw_, dstptr
//

end_inner_bottom:
	sub	r0, r0
	ldl	srcptr, BLT_sptrw_
	sub	srclow, $99	// this value will be replaced at compile time
	sbcb	srchi, rh0
	ldl	BLT_sptrw_, srcptr

	ldl	dstptr, BLT_dptrw_
	sub	dstlow, $99	// this value replaces at compile time
	sbcb	dsthi, rh0
	ldl	BLT_dptrw_, dstptr

load_mask:
	ld	pat, spat_(pline)

inc_pline:
	inc	pline, $2
	and	pline, $31

dec_pline:
	dec	pline, $2
	and	pline, $31
	
ld_words:
	ld	wds, BLT_words_
	
bottom_of_loop:
	dec	lines
	jp	nz, code_space_
	lda	rr4, sreg_
	ldm	r8, @rr4, $6
	ret
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//	special code stubs for special case EXCHANGE
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
ex_left_top:
	and	r0, lmask
	ld	r3, lmask
	com	r3
	and	r2, r3
	add	r0, r2
	ld	@dstptr, r0
	ld	@srcptr, r1
	inc	srclow, $2
	inc	dstlow, $2

ex_left_bottom:
	and	r0, lmask
	ld	r3, lmask
	com	r3
	and	r2, r3
	add	r0, r2
	ld	@dstptr, r0
	ld	@srcptr, r1

ex_right_top:
	and	r0, rmask
	ld	r3, rmask
	com	r3
	and	r2, r3
	add	r0, r2
	ld	@dstptr, r0
	ld	@srcptr, r1

ex_right_bottom:
	and	r0, rmask
	ld	r3, rmask
	com	r3
	and	r2, r3
	add	r0, r2
	ld	@dstptr, r0
	ld	@srcptr, r1
	dec	srclow, $2
	dec	dstlow, $2


	
null_op:
	.word 1
	halt
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
//		for all the Basic OPerations :
//              r0 = src wd, r1 = dst word
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
bop0:
	.word 1
	sub	r0, r0		// src = 0

bop1:
	.word 1
	and	r0, r1		// src = src & dst

bop2:
	.word 2
	com	r1		// ~dst
	and	r0, r1		// src = src & ~dst
	
bop3:
	.word 1
	nop			// src  = src
	
bop4:
	.word 2
	com	r0		// src = ~src
	and	r0, r1		// src = ~src & dst
	
bop5:
	.word 1
	ld	r0, r1		// src = dst
	
bop6:
	.word 1
	xor	r0, r1		// src = src ^ dst
	
bop7:
	.word 1
	or	r0, r1		// scr = src | dst
	
bop8:
	.word 3
	com	r0		
	com	r1
	and	r0, r1		// src = ~src & ~dst
	
bop9:
	.word 2
	com	r0
	xor	r0, r1		// src = ~src ^ dst
	
bop10:
	.word 2
	com	r1
	ld	r0, r1		// src = ~dst

//
	
bop11:
	.word 2
	com	r1
	or	r0, r1		// src = src | ~dst
	
bop12:
	.word 1
	com	r0		// src = ~src
	
bop13:
	.word 2
	com	r0
	or	r0, r1		// src = ~src | dst
	
bop14:
	.word 3
	com	r0
	com	r1
	or	r0, r1		// src = ~src | ~dst
	
bop15:
	.word 2
	ld	r0, $0xffff	// src = true...all one's

