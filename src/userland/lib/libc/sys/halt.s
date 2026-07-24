/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface routine for halt system call.
/ halt();

.globl	halt_

halt_:
	sys	130		/ 0x82
	ret
