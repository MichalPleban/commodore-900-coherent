/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Evaluate 2 to the power x.
 * (Hart 1067, 18.08)
 */
#include <math.h>

static double twontab[] ={
	0.1513906799054338915894328e+04,
	0.2020206565128692722788600e+02,
	0.2309334775375023362400000e-01
};
static double twomtab[] ={
	0.4368211662727558498496814e+04,
	0.2331842114274816237902950e+03,
	0.1000000000000000000000000e+01
};

double
_two(x)
double x;
{
	double p, q, r, e;
	register int s;

	if (x > L2I) {
		errno = ERANGE;
		return (I);
	}
	s = 0;
	x = modf(x, &e);
	if (x > 0.5) {
		s = 1;
		x -= 0.5;
	}
	r = x*x;
	p = x*_pol(r, twontab, 3);
	q = _pol(r, twomtab, 3);
	r = (q+p)/(q-p);
	if (s)
		r *= SQRT2;
	r = ldexp(r, (int) e);
	return (r);
}
