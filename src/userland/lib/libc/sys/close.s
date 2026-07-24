/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for close system call.
/ close(fd);

.globl	close_

close_:
	sys	6
	ret
