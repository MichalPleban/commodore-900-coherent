/*
 * Copyright (c) 1985 Rico Tudor.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "smgr.h"

/* The engine globals only the SERVER side touches: the layer manager's
 * display list and damage queue, the window table and the mouse position.
 * Split out of globals.c (Aug 2026) because globals.c is linked into
 * libhrgfx.sl, whose private half is copied into every GUI process at
 * exec -- so `update[MAX_UPBUF]' alone was 600 bytes of the layer
 * manager's damage queue carried by every client that never runs it
 * (layer.o is server-only and is filtered out of the .sl).
 *
 * The rule for this file: nothing here may be referenced by the client
 * stack.  It is enforced by the .sl link, which does not include this
 * object -- a client-side reference is a loud `undefined', not a silent
 * second copy. */

LAYER	*DM_rearmost;
LAYER	*DM_frontmost;
char	gkmsg[sizeof(GRAPH)+4];
UPDATE	update[MAX_UPBUF];
