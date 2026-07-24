/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for wait system call.
/ pid = wait(statusp);

.globl	wait_

wait_:
	sys	7
	ret
