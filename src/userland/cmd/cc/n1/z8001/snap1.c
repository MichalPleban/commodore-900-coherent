/*
 * n1/z8001/snap1.c
 * Machine-specific parts of the cc1 -S debug dumps. Segmented Z8001.
 * Template: n1/i8086/snap1.c (most snap helpers are macros in cc1mch.h).
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif
#include <stdint.h>

/*
 * Print a pointer. Host-dependent (shows a host pointer), needed even if TINY
 * by other diagnostics. On the segmented model it is shown as seg:offset; the
 * i8086 original reinterpreted a char* as a struct, which is not portable, so
 * the halves are taken from the host pointer value here.
 */
psnap(p)
char *p;
{
#if RUNNING_LARGE
	unsigned long v = (unsigned long)(uintptr_t)p;
	printf("%02lx:%04lx", (v >> 16) & 0x7f, v & 0xffff);
#else
	printf("%04lx", (unsigned long)(uintptr_t)p & 0xffff);
#endif
}

#if !TINY
/*
 * Print a dval_t (the 8-byte target double image).
 */
dsnap(d)
dval_t d;
{
	register int i;

	for (i = 0; i < 8; ++i)
		printf(" %02x", d[i] & 0377);
}
/* isnap/lsnap/csnap/fsnap/mdlsnap/mdosnap are macros in cc1mch.h. */
#endif

/* end of n1/z8001/snap1.c */
