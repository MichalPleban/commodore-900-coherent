/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for fstat system call.
/ fstat(fd, sbp);

.globl	fstat_

fstat_:
	sys	034		/28
	ret
