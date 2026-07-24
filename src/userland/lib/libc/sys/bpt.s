/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for bpt system call (causes SIGBPT to be sent)
/ bpt();

.globl	bpt_
bpt_:
	sys	129	/ 0x81
	ret
