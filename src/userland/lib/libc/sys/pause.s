/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for pause system call.
/ pause();

.globl	pause_

pause_:
	sys	035		/29
	ret
