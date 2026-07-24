/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Raise x to the power y.
 */
#include <math.h>

double
pow(x, y)
double x, y;
{
	double r;
	register unsigned s, i, e;

	s = 0;
	i = 0;
	if (x == 0.0) {
		if (y <= 0.0)
			errno = EDOM;
		return (0.0);
	}
	r = modf(y, &r);
	if (x < 0.0) {
		if (r != 0.0) {
			errno = EDOM;
			return (0.0);
		}
		x = -x;
		if (((int) y) & 1)
			s = 1;
	}
	if (y < 0.0) {
		y = -y;
		i = 1;
	}
	if (r!=0.0 || y>16384.0)
		r = _two(y*log10(x)*LOG10B2);
	else {
		r = 1.0;
		for (e=y; e; e>>=1) {
			if (e&01)
				r *= x;
			x *= x;
		}
	}
	if (i)
		r = 1/r;
	if (s)
		r = -r;
	return (r);
}
