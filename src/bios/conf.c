/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Establish machine configuration parameters.
 */
#include "rom.h"

configure(rp, ramlow, ramhigh)
register struct romconf *rp;
unsigned ramlow, ramhigh;
{
	rp->rom_bram = ramlow;
	rp->rom_eram = ramhigh;
	rp->rom_ctype = CTYPE;
}
