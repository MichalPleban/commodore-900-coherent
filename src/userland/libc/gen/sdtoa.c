/*
 * Dummy routines for floating point output so programmes which
 * don't use floating point can stay small.
 */
#include <stdio.h>

_dtefg()
{
	fprintf(stderr, "No floating point!\n");
	exit(1);
}
