/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
