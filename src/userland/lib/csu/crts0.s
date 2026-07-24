/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Library
/ C run-time start-off.
/ Coherent native version.

	.globl	main_
	.globl	environ_
	.globl	errno_
	.globl	_exit_, exit_
	.globl	SS

SS = 0x0000

errno_ = 0x0000FFFE		/ SS|0xFFFE

start:
	ldl	rr0, rr14(6)		/ envp
	ldl	environ_, rr0
	sub	r13, r13		/ Clear frame pointer
	call	main_
	push	(rr14), r1
	call	exit_
_exit_:
	sys	1

	.prvd
	.word	0			/ NULL
environ_:
	.long	0
