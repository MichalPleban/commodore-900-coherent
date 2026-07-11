/*
 * Square root function.
 */
#include <math.h>

double
sqrt(x)
double x;
{
	double s;
	int i;
	register int n;

	if (x < 0.0) {
		errno = EDOM;
		return (0.0);
	}
	n = L2L2P;
	frexp(x, &i);
	s = ldexp(1.0, i/2);
	do {
		s = (s + x/s) / 2.0;
	} while (--n);
	return (s);
}
