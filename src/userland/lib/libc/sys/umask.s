/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for umask system call
/ umask(complmode);

.globl	umask_

umask_:
	sys	074		/60
	ret
