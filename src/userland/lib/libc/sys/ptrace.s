/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for ptrace system call
/ ptrace(req, pid, addr, data);

.globl	ptrace_

ptrace_:
	sys	032		/26
	ret
