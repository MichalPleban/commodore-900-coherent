/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for unlink system call.
/ unlink(name);

.globl	unlink_

unlink_:
	sys	012		/10
	ret
