/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for pipe system call.
/ pipe(pdes);
/ int pdes[2];

.globl	pipe_

pipe_:
	sys	052		/42
	ret
