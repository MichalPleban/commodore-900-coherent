/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mprec.h"


/*
 *	Ispos returns true iff the mint pointed to by "a" is positive
 *	(ie. greater than or equal to zero).
 */

ispos(a)
register mint *a;
{
	return (a->val[a->len - 1] != NEFL);
}
