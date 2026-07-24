/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Segmented Z8001.
 * Structure for a setjmp environment.
 * The PC and R6-R15 are all saved.
 */

typedef	int	jmp_buf[12];
