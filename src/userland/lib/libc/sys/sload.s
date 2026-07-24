/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for sload system call.
/ int sload(major, name, configp) int major; char *name; CONFIG *configp;

.globl	sload_
sload_:
	sys	0x40		/ 64
	ret
