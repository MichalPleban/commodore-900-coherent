/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Disk load routines: per-device initialization before a boot read.
 */

extern        wdinit();
#include "ddtvars.h"
#define nerr  ddtvars.d_nerr     /* global error counter */
extern int    hr_y;              /* controller busy flag */

/*
 * Hard disk: clear error/busy state, initialize the controller.
 */
hdload()
{
        nerr = 0;
        hr_y = 0;
        wdinit();
}

/*
 * Floppy: zero the command block at the base of RAM, clear errors.
 */
fdload()
{
        register char *p, *q;
        register int i;

        p = (char *)0x08000000;
        q = p;
        for (i = 0; i < 0x20; i++)
                q[i] = 0;
        nerr = 0;
}
