/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Evaluate the sine function.
 */
#include <math.h>

double
sin(x)
double x;
{
	return (cos(x-PI/2.0));
}
