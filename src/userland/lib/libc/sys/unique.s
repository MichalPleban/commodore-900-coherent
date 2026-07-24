/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for unique system call
/ l = unique();

.globl	unique_

unique_:
	sys	055		/45
	ret
