/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for utime system call
/ utime(file, timep);

.globl	utime_

utime_:
	sys	036		/30
	ret
