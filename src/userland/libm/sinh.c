/*
 * Hyperbolic sine.
 */
#include <math.h>

double
sinh(x)
double x;
{
	double r;
	register int e;

	e = errno;
	r = exp(x);
	errno = e;
	r = (r-1.0/r) / 2.0;
	return (r);
}
