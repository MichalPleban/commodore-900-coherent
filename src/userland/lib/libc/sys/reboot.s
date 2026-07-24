/ Copyright (c) 1977-1995 Robert Swartz.
/ Copyright (c) 2026 Michal Pleban.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface routine for reboot system call.
/ reboot();

.globl	reboot_

reboot_:
	sys	131		/ 0x83
	ret
