/
/ C interface routine for halt system call.
/ halt();

.globl	halt_

halt_:
	sys	130		/ 0x82
	ret
