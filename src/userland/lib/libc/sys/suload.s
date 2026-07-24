/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for suload system call.
/ int suload(major) int major;

.globl	suload_
suload_:
	sys	0x41		/ 65
	ret
