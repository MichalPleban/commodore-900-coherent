/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Return integer absolute value
 * (This doesn't work on the largest negative integer)
 */

abs(x)
{
	return (x<0 ? -x : x);
}
