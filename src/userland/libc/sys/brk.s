/
/ C interface for brk system call
/ brk(newend);
/ char *newend;

.globl	brk_, __end_, end_

brk_:
	ldl	rr0, rr14(4)
	ldl	__end_, rr0
	sys	021			/17
	ret

	.prvd
__end_:
	.long	end_
