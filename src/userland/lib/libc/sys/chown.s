/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for chown system call.
/ chown(file, owner, group);

.globl	chown_

chown_:
	sys	020		/16
	ret
