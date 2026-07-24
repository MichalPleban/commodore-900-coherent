/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for chdir system call.
/ chdir(directory);

.globl	chdir_

chdir_:
	sys	014		/12
	ret
