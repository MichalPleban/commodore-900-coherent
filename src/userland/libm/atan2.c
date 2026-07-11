/*
 * Compute the inverse tangent given two sides of a right angled
 * triangle.
 */
#include <math.h>

double
atan2(y, x)
double x;
double y;
{
	double r;

	if (x == 0.0) {
		r = PI/2;
		if (y < 0.0)
			r = -r;
		return (r);
	}
	r = atan(y/x);
	if (x < 0.0) {
		if (y < 0.0)
			r -= PI;
		else
			r += PI;
	}
	return (r);
}
