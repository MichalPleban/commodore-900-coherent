/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Floating absolute value.
 */
#include <math.h>

double
fabs(x)
double x;
{
	if (x < 0.0)
		x = -x;
	return (x);
}
