/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ Abort.
/ Blow up with HALT instruction.
/

	.globl	abort_

abort_:
	halt
	ret
