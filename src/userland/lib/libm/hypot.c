/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Hypotenuese function.
 */
#include <math.h>

double
hypot(x, y)
double x;
double y;
{
	double r;

	r = y/x;
	r = x * sqrt(1.0 + r*r);
	return (r);
}
