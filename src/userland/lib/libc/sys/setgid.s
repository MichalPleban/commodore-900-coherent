/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for setgid system call
/ setgid(gid);

.globl	setgid_

setgid_:
	sys	056		/46
	ret
