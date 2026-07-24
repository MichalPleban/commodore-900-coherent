/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for ftime system call
/ ftime(timebp);

.globl	ftime_

ftime_:
	sys	043		/35
	ret
