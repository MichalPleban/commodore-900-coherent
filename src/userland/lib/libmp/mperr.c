/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "mprec.h"


/*
 *	Mperr issues the error message pointed at by "msg" and then
 *	exits returning a value of 1 (for unsuccessful execution).
 */

void
mperr(msg)
char *msg;
{
	fprintf(stderr, "mpx: %r", &msg);
	fprintf(stderr, "\n");
	exit(1);
}
