/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Hyperbolic tangent.
 */
#include <math.h>

double
tanh(x)
double x;
{
	double r;
	register int s;

	s = 0;
	if (x < 0.0) {
		x = -x;
		s = 1;
	}
	r = exp(-2.0*x);
	r = (1.0-r) / (1.0+r);
	return (s?-r:r);
}
