/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for ioctl system call
/ ioctl(fd, req, vec);

.globl	ioctl_

ioctl_:
	sys	066		/54
	ret
