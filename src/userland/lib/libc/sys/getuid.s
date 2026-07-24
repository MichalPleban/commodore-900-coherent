/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for getuid system call
/ uid = getuid();

.globl	getuid_

getuid_:
	sys	030		/24
	ret
