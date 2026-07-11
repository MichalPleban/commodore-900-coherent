/
/ C interface for bpt system call (causes SIGBPT to be sent)
/ bpt();

.globl	bpt_
bpt_:
	sys	129	/ 0x81
	ret
