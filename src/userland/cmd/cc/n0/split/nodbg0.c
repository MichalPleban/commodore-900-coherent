/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * n0/split/nodbg0.c
 * C compiler.
 * The debug-table generator's entry points, doing nothing.
 *
 * Linked in place of n0/dbgt0.c (src/cc/Makefile, DBG0) by a cc0 that is
 * never asked for -VDEBUG tables: nothing on this system's images reads
 * them.  The callers in cc0.c, stat.c, locals.c and cc0sym.c call these
 * unconditionally and dbgt0.c tests the variant inside, so with the
 * variant off the object is the same either way (the split fixed point
 * checks that).
 */
#include "cc0.h"

dbfname(f) char *f;
{
}

dbstat(s, line) int s, line;
{
}

dblabel(sp) SYM *sp;
{
}

dbdecl(sp) SYM *sp;
{
}

dbargs()
{
}

dblocal(sp) SYM *sp;
{
}

dbdown()
{
}

/* end of n0/split/nodbg0.c */
