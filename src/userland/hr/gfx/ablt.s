
//
// _GETBITS(n) - return the next 'n' bits of source
//	Entry:	SP+4 = number of bits (<=16)
//
//	Used:	r1 = return value
//		rr4= _sp
//		r2 = current source word (_sw).
//		r0 = n, i.e., #bits more to get
//
.globl getbits_
getbits_: 
	dec	r15, $4
	ld	(rr14), r13
	ld	r13, r15

	sub	r1,r1		// val = 0;
	ld	r0, L10001+4(r13)
	ld	r2,sw_
	cp	r0,$16
	ret	gt		// if (n > 16) then quit;
	jr	lt,gt50		// else if ( n != 16 || snm != 15)
	cp	snb_,$15	// 	do bottom of loop;
	jr	ne,gt50
	ld	r1,sw_		// else return whole word in sw
	inc	sp_+2,$2
	ldl	rr4,sp_
	ld	r4,@rr4
	ld	sw_,r4
	
	ld	r13, (rr14)
	inc	r15, $4
	ret
gtloop:
	dec	r0,$1		// n--;
	rl	r2,$1
	rlc	r1,$1		// val = 2*val + bit16(_sw);
	dec	snb_,$1		// if ( !_snb-- )
	jr	pl,gt50
	inc	sp_+2,$2	// {	_sp++;
	ldl	rr4,sp_
	ld	r2,@rr4		//	_sw = *_sp;
	ld	snb_,$15	//	_snb = 15;
gt50:				// }
	test	r0		//
	jr	nz,gtloop	// if ( n ) then gtloop;
	ld	sw_,r2		// update _sw

	ld	r13, (rr14)
	inc	r15, $4
	ret

L10001=SS|4
