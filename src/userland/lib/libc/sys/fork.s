/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for fork system call.
/ pid = fork();

.globl	fork_

fork_:
	sys	2
	ret
