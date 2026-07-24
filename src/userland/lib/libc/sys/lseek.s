/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for lseek system call.
/ lseek(fd, offset, ptrname);

.globl	lseek_

lseek_:
	sys	023		/19
	ret
