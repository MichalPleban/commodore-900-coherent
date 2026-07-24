/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for read system call.
/ count = read(fd, bp, nb);

.globl	read_

read_:
	sys	3
	ret
