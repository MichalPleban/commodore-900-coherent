/*
 * Natural logarithm.
 */
#include <math.h>

double
log(x)
double x;
{
	return (log10(x)*LOG10BE);
}
