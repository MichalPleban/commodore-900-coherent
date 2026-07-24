/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for kill system call.
/ kill(pid, sig);

.globl	kill_

kill_:
	sys	045		/37
	ret
