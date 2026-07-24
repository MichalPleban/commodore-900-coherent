/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for alarm system call.
/ alarm(nsec);

.globl	alarm_

alarm_:
	sys	033		/27
	ret
