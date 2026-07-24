/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for acct system call
/ acct(file);
/ char *file;

.globl	acct_

acct_:
	sys	063		/51
	ret
