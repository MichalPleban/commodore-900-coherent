/*
 * Floor.
 */
#include <math.h>

double
floor(x)
double x;
{
	double r;

	modf(x, &r);
	return (r);
}
