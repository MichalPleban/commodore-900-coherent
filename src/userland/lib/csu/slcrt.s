/ Copyright (c) 2026 Michal Pleban.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Library
/ Shared library run-time.
/
/ The absolute symbols and raw-exit stub that crts0.s provides in a
/ normal executable, without its start code (a shared library has no
/ main and is never entered; the kernel pauses its holder before user
/ mode).  errno lives at the fixed stack-segment address the kernel
/ writes for every process, so library and clients agree on it by
/ construction.  _exit_ is the raw exit system call, referenced by the
/ libc exit() baked into the library.

	.globl	errno_
	.globl	_exit_
	.globl	SS

SS = 0x0000

errno_ = 0x0000FFFE		/ SS|0xFFFE

	.shri
_exit_:
	sys	1
