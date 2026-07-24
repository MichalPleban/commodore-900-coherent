/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for lock system call
/ lock(flag);

.globl	lock_

lock_:
	sys	065		/53
	ret
