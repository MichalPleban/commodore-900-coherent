/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for sync system call
/ sync();

.globl	sync_

sync_:
	sys	044		/36
	ret
