/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for chroot system call.
/ chroot(directory);

.globl	chroot_

chroot_:
	sys	075		/61
	ret
