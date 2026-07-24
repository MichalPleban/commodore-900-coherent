/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for times system call.
/ times(tbp);

.globl	times_

times_:
	sys	053		/43
	ret
