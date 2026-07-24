/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for open system call.
/ fd = open(name, mode);

.globl	open_

open_:
	sys	5
	ret
