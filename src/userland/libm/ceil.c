/*
 * Ceiling.
 */
#include <math.h>

double
ceil(x)
double x;
{
	double r;

	if (modf(x, &r) != 0.0)
		r += 1.0;
	return (r);
}
