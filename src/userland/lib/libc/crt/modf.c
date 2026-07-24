/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* modf( real, ip)
 double real, *ip

 returns  real - g  and stores g at ip
   where g = greatest integer <= real

 uses machine dependent subroutine modfs( real, ip, e)
   which does modf assuming that d >= 0  1 <= e = exponent of real  <= MBITS+1
*/

#include "fpformat.h"

extern	double	modfs(), frexp();

double
modf( d, dp)
double	d;
register double *dp;
{
	int	e;

 	frexp( d, &e);
	if( e >= MBITS+1) {
		*dp = d;
		return( 0.0);
	}
	if( e <= 0) {
		if( d < 0) {
			*dp = -1;
			return( 1 - d);
		}
		*dp = 0;
		return( d);
	}
	if( d >= 0)
		return( modfs( d, dp, e));
	d = modfs( -d, dp, e);
	if( d != 0) {
		*dp = -*dp - 1;
		return( 1 - d);
	} else {
		*dp = -*dp;
		return( d);
	}
}
