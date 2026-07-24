/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for getgid system call.
/ gid = getgid();

.globl	getgid_

getgid_:
	sys	057		/47
	ret
