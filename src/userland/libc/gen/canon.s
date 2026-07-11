/ Segmented Z8001 library
/ int _canw(i) int i;
/ long _canl(l) long l;
/ These are called by the routines that
/ transform words and longs to and from the
/ canonical formats.

	.globl	_canw_, _canl_
	.globl	SS

_canw_:
	ld	r1, SS|4(r15)
	exb	rh1, rl1
	ret

_canl_:
	ldl	rr0, SS|4(r15)
	exb	rh0, rl0
	exb	rh1, rl1
	ret
