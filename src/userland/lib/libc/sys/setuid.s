/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for setuid system call.
/ setuid(uid);

.globl	setuid_

setuid_:
	sys	027		/23
	ret
