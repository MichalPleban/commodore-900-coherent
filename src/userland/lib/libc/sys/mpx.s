/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for mpx system call(s).
/ mpx(cmd, vec);

.globl	mpx_

mpx_:
	sys	067		/55
	ret
