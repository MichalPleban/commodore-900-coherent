/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Common definitions.
 */
#ifndef ROM_H
#define ROM_H

#ifndef NULL
#define NULL    0
#endif

/*
 * A 32-bit segmented address: segment number in the high
 * half, offset in the low 16 bits.
 */
typedef unsigned long addr_t;

#define laddr(s, o)   (((unsigned long)(unsigned)(s) << 16) | (unsigned short)(o))

#include "romconf.h"

#define RAMBASE  0x08000000L    /* WD command block / load base */
#define LDSEG    0x30           /* logical segment the OS loads into */
#define RUNADDR  ((addr_t)0L)
#define CTYPE    1              /* clock type: 6MHz */
#define VERS     " V0.0\n"

#endif /* ROM_H */
