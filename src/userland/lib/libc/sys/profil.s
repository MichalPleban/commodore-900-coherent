/ Copyright (c) 1977-1995 Robert Swartz.
/ SPDX-License-Identifier: BSD-3-Clause
/
/ C interface for profil system call.
/ profil(buff, bufsiz, offset, scale);

.globl	profil_

profil_:
	sys	054		/44
	ret
