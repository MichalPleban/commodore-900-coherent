/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for link system call.
/ link(old, new);

.globl	link_

link_:
	sys	011		/9
	ret
