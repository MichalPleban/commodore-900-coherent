/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for getpid system call.
/ pid = getpid();

.globl	getpid_

getpid_:
	sys	024		/20
	ret
