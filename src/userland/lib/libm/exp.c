/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Exponential function.
 */
#include <math.h>

double
exp(x)
double x;
{
	return (_two(x*LOGEB2));
}
