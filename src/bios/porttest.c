/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * MMU (U2) segment map test.  Fill a reference window with a stepped
 * word pattern; then for each segment, save its map descriptor, map it
 * onto the reference window, verify the descriptor and the pattern read
 * back through the new mapping, write an advanced pattern through it,
 * and restore the saved descriptor.
 */

#include "cioports.h"
#include "ddtvars.h"

extern outstr();
extern putchar();
extern puthex();
extern mmuread();
extern mmuwrite();
extern long mkfp();
extern int rdport();
extern wrport();
extern char abflags[];		/* burn-in error / abort flags */
extern char ddttmp[];		/* debugger scratch */

char mmudesc[] = { 0x0c, 0x00, 0xff, 0xc0 };	/* reference window descriptor */
static char msgs[] = "MMU  (U2) test: \0OK\n\0\n    Bad segment detected = ";
char ldrbuf[542] = { 0 };	/* loader scratch */

port_test()
{
	register int seg, val;
	register unsigned *p, *lim;
	char *c1;
	char *c2;
	int pattern;

	outstr(msgs);
	pattern = 0x5555;
	lim = (unsigned *)mkfp(0x0c00, 0x0200);
	val = pattern;
	for (p = (unsigned *)mkfp(0x0c00, 0); p < lim; p++, val += 0x3b)
		*p = val;
	for (seg = 2; seg < 0x3f && abflags[0] == 0; seg++) {
		mmuread(seg, 1, ddtvars.d_mmusave);
		mmuwrite(seg, mmudesc);
		mmuread(seg, 1, &ddttmp[2]);
		for (val = 0, c1 = mmudesc, c2 = &ddttmp[2];
		    val < 4 && abflags[0] == 0; c1++, c2++, val++)
			abflags[0] |= (*c1 != *c2);
		if (abflags[0] == 0) {
			val = pattern;
			p = (unsigned *)mkfp(seg << 8, 0);
			lim = (unsigned *)mkfp(seg << 8, 0x0200);
			for (; p < lim; p++, val += 0x3b)
				abflags[0] |= (*p != val);
			pattern += 9;
			val = pattern;
			for (p = (unsigned *)mkfp(seg << 8, 0); p < lim;
			    p++, val += 0x3b)
				*p = val;
		}
		mmuwrite(seg, ddtvars.d_mmusave);
	}
	if (abflags[0] == 0) {
		wrport(rdport() + 1);
		outstr(&msgs[17]);	/* "OK\n" */
	} else {
		outstr(&msgs[21]);	/* "\n    Bad segment detected = " */
		puthex((long)seg, 4);
		putchar('\n');
	}
}

