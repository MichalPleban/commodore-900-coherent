/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mprec.h"




/*
 *	Mintfr releases all space occupied by the mint pointed to by "a".
 *	This includes the space for the mint and for the value to which
 *	it points.
 */

void
mintfr(a)
register mint *a;
{
	mpfree(a->val);
	mpfree(a);
}
