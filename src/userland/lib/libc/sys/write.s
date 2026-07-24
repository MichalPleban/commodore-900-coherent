/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for write system call.
/ count = write(fd, bp, nb);

.globl	write_

write_:
	sys	4
	ret
