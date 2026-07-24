/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for getegid system call
/ gid = getegid();

.globl	getegid_

getegid_:
	sys	070		/56
	ret
