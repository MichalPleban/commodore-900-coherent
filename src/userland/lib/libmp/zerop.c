/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mprec.h"


/*
 *	Zerop returns a TRUE or FALSE depending on wether or not
 *	the mint pointed to by "a" is zero.
 */

zerop(a)
register mint	*a;
{
	return (a->len == 1 && *a->val == 0);
}
