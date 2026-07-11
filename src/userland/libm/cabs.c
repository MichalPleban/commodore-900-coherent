/*
 * Complex absolute value.
 */
#include <math.h>

double
cabs(z)
CPX z;
{
	return (hypot(z.z_r, z.z_i));
}
