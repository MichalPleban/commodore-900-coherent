/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Floor.
 */
#include <math.h>

double
floor(x)
double x;
{
	double r;

	modf(x, &r);
	return (r);
}
