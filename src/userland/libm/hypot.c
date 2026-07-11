/*
 * Hypotenuese function.
 */
#include <math.h>

double
hypot(x, y)
double x;
double y;
{
	double r;

	r = y/x;
	r = x * sqrt(1.0 + r*r);
	return (r);
}
