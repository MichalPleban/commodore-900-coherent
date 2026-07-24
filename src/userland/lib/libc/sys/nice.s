/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for nice system call.
/ nice(prio);

.globl	nice_

nice_:
	sys	042		/34
	ret
