/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for mknod system call.
/ mknod(file, mode, addr);

.globl	mknod_

mknod_:
	sys	016		/14
	ret
