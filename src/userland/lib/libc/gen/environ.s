/ Copyright (c) 2026 Michal Pleban.
/ SPDX-License-Identifier: BSD-3-Clause
/ Segmented Z8001 Library
/
/ environ - the process environment pointer, stored by crts0 at start-up.
/ Defined here in libc rather than in crts0.s so the SHARED libc can
/ reference it: library code (getenv, the exec* wrappers) may only
/ reference addresses inside the library image, and crts0 stays on the
/ client side of every link.  crts0's store references environ_, which
/ pulls this member into every static link; in a shared link it binds to
/ the library's per-process data segment.  The leading null word keeps
/ crts0.s's historical layout (a NULL guard below the pointer).

	.globl	environ_

	.prvd
	.word	0			/ NULL
environ_:
	.long	0
