/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ Common C interface for all forms of
/ exec system call.
/ execve(name, argp, envp);

.globl	execve_

execve_:
	sys	013		/11
	ret
