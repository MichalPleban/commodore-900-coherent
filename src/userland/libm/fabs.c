/*
 * Floating absolute value.
 */
#include <math.h>

double
fabs(x)
double x;
{
	if (x < 0.0)
		x = -x;
	return (x);
}
