/ clrow.s - the ldir row copy under cl_blit's aligned fast path.
/
/ cl_ldrow(dst, src, nwords): one word-ldir.  Registers r0-r5 only (the
/ PCC ABI keeps live values in r6-r14 across calls - see the libc .s files).

	.globl	cl_ldrow_

cl_ldrow_:
	ldl	rr2, rr14(4)		/ dst
	ldl	rr4, rr14(8)		/ src
	ld	r0, rr14(12)		/ nwords
	ldir	(rr2), (rr4), r0
	ret
