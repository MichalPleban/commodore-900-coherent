/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for access system call.
/ access(file, mode);

.globl	access_

access_:
	sys	041		/33
	ret
