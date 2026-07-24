/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for umount system call.
/ umount(special);

.globl	umount_

umount_:
	sys	026		/22
	ret
