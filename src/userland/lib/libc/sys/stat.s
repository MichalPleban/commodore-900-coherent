/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for stat system call.
/ stat(file, sbp);

.globl	stat_

stat_:
	sys	022		/18
	ret
