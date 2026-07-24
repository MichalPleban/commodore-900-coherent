/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Natural logarithm.
 */
#include <math.h>

double
log(x)
double x;
{
	return (log10(x)*LOG10BE);
}
