/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Ceiling.
 */
#include <math.h>

double
ceil(x)
double x;
{
	double r;

	if (modf(x, &r) != 0.0)
		r += 1.0;
	return (r);
}
