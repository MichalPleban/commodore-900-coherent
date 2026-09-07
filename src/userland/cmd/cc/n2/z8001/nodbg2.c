/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * n2/z8001/nodbg2.c
 * C compiler.
 * The debug-table generator's and the assembly listing's entry points,
 * doing nothing.
 *
 * Linked in place of n2/dbgt2.c and n2/z8001/listing.c (src/cc/Makefile,
 * DBG2) by a cc2 that is never asked for -VDEBUG tables or a listing:
 * nothing on this system's images reads either.  cc2.c, getfun.c and
 * optim.c call the table generator unconditionally and it tests the
 * variant inside; emit1.c asks listwanted() first.  With the variant off
 * the object is the same either way (the split fixed point checks that).
 */
#include "cc2.h"

gendbgt(level) int level;
{
}

fixdbgt()
{
}

mrgdbgt(ip1, ip2) INS *ip1, *ip2;
{
}

dprint(ip) INS *ip;
{
}

listwanted()
{
	return 0;
}

listing()
{
}

/* end of n2/z8001/nodbg2.c */
