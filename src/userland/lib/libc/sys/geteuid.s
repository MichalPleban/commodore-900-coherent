/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for geteuid system call.
/ uid = geteuid();

.globl	geteuid_

geteuid_:
	sys	071		/57
	ret
