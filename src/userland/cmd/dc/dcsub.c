/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "bc.h"

/*
 *	dst -= src
 *	src freed
 *	modelled on bcadd
 */

dcsub(src, dst)
register rvalue	*src, *dst;
{
	if (dst->scale >= src->scale)
		rescale(src, dst->scale);
	else
		rescale(dst, src->scale);
	msub(&dst->mantissa, &src->mantissa, &dst->mantissa);
	mvfree(&src->mantissa);
}
