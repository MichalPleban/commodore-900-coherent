/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for stime system call
/ stime(time);

.globl	stime_

stime_:
	sys	031		/25
	ret
