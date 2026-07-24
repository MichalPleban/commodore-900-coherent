/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for dup (and dup2) system calls.
/ dup2(ofd, nfd);

.globl	dup_

dup_:
	sys	051		/41
	ret
