/
/ Abort.
/ Blow up with HALT instruction.
/

	.globl	abort_

abort_:
	halt
	ret
