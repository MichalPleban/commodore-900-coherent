
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



.prvi
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//		INneR LOOPS  
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//		OP = false
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
.globl inT0, inT3, inT6, inT10, inT15, inT16
.globl inB0, inB3, inB6, inB10, inB15, inB16

.globl imT1, imT2, imT3, imT4, imT5, imT6, imT7
.globl imT8, imT9, imT11, imT12, imT13, imT14, imT15

.globl imB1, imB2, imB3, imB4, imB5, imB6, imB7
.globl imB8, imB9, imB11, imB12, imB13, imB14, imB15



inT0:
	.word 5
	lda	srcptr, BLTfalse_
	ldir	@dstptr, @srcptr, wds

inB0:
	.word 4
	sub	r0, r0
	ld	@dstptr, r0
	dec	dstlow, $2
	djnz	wds, inB0+2

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src AND dst
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT1:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	and	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT1+2

imB1:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	and	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB1+2
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src and (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT2:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	@dstptr
	and	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT2+2

imB2:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	@dstptr
	and	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB2+2
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 			OP = src
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
inT3:
	.word 2
	ldir	@dstptr, @srcptr, wds
inB3:
	.word 5
	ld	r0, @srcptr
	ld	@dstptr, r0
	dec	srclow, $2
	dec	dstlow, $2
	djnz	wds, inB3+2
imT3:
	.word 6
	ld	r0, @srcptr
	and	r0, pat
	ld	@dstptr, r0
	inc	srclow, $2
	inc	dstlow, $2
	djnz	wds, imT3+2
imB3:
	.word 6
	ld	r0, @srcptr
	and	r0, pat
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB3+2
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) AND dst
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT4:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	and	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT4+2

imB4:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	and	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB4+2

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT5:
	.word 	1
	nop
imB5:
	.word 	1
	nop

// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 		OP =  src XOR dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
inT6:
	.word 6
	ld	r0, @srcptr
	xor	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, inT6+2
inB6:
	.word 6
	ld	r0, @srcptr
	xor	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, inB6+2
imT6:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	xor	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT6+2
imB6:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	xor	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB6+2
//
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src OR dst
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT7:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	or	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT7+2
imB7:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	or	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB7+2

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) AND (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT8:
	.word	9
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	com	@dstptr
	and	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT8+2

imB8:
	.word	9
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	com	@dstptr
	and	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB8+2
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) XOR dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT9:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	xor	r0, @dstptr
	ld	@dstptr, r0
	inc	srclow, $2
	djnz	wds, imT9+2
imB9:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	xor	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB9+2

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = ~dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
inT10:
	.word 5
	ld	r1, @dstptr
	com	r1
	ld	@dstptr, r1
	inc	dstlow, $2
	djnz	wds, inT10+2
inB10:
	.word	5
	ld	r1, @dstptr
	com	r1
	ld	@dstptr, r1
	inc	dstlow, $2
	djnz	wds, inB10+2
//
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = src OR (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT11:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	@dstptr
	or	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT11+2
imB11:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	@dstptr
	or	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB11+2

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT12:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT12+2
imB12:
	.word	7
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB12+2
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) OR dst
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT13:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	or	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT13+2
imB13:
	.word	8
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	or	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB13+2

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = (~src) OR (~dst)
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
imT14:
	.word	9
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	com	@dstptr
	or	r0, @dstptr
	ld	@dstptr, r0
	inc	dstlow, $2
	inc	srclow, $2
	djnz	wds, imT14+2
imB14:
	.word	9
	ld	r0, @srcptr
	and	r0, pat
	com	r0
	com	@dstptr
	or	r0, @dstptr
	ld	@dstptr, r0
	dec	dstlow, $2
	dec	srclow, $2
	djnz	wds, imB14+2
// 
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = true
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
inT15:
	.word 5
	lda	srcptr, BLTtrue_
	ldir	@dstptr, @srcptr, wds

inB15:
	.word 4
	ld	@dstptr, $0xffff
	dec	dstlow, $2
	djnz	wds, inB15+2

imT15:
	.word  3
	ld	@dstptr, pat
	inc	dstlow, $2
	djnz	wds, imT15+2
imB15:
	.word	3
	ld	@dstptr, pat
	dec	dstlow, $2
	djnz	wds, imB15+2

// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//			OP = exchange
// ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
inT16:
	.word	6
	ld	r0, @srcptr
	ex 	r0, @dstptr
	ld	@srcptr, r0
	inc	srclow, $2
	inc	dstlow, $2
	djnz	wds, inT16+2

inB16:
	.word	6
	ld	r0, @srcptr
	ex	r0, @dstptr
	ld	@srcptr, r0
	dec	srclow, $2
	dec	dstlow, $2
	djnz	wds, inB16+2

