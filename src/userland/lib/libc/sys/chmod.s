/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for chmod system call.
/ chmod(file, mode);

.globl	chmod_

chmod_:
	sys	017		/15
	ret
