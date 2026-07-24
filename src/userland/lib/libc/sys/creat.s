/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for creat system call.
/ fd = creat(name, mode);

.globl	creat_

creat_:
	sys	010		/8
	ret
