/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Compute the inverse cosine function.
 */
#include <math.h>

double
acos(x)
double x;
{
	if (x<-1.0 || x>1.0) {
		errno = EDOM;
		return (0.0);
	}
	return (PI/2.0 - asin(x));
}
