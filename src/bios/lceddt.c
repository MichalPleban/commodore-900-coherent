/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * I/O chip register self-test.
 */

#include "romconf.h"
#include "ddtvars.h"

extern outstr();
extern putchar();
extern puthex();
extern outb();
extern int inb();
extern chipinit();
extern int rdport();
extern wrport();
extern char abflags[];		/* burn-in error / abort flags */
extern int put_init;		/* console initialized */
extern int curchip;		/* chip index under test */

/* chip-under-test names, indexed by curchip (set by the T_NAME op) */
char *chipnames[] = { "CIO1 (U78)", "CIO2 (U66)", "SCC  (U74)" };

/*
 * Test script walked by selftest():
 *   t_op 0 = walkbit(t_port, t_val)  walking-bit register test
 *   t_op 1 = announce chipnames[t_val]
 *   t_op 2 = section passed: bump the front panel, print "OK"
 *   t_op 3 = outb(t_port, t_val)     setup write
 */
struct test {
	char	t_op;
	char	t_x;
	int	t_port;
	char	t_val;
	char	t_y;
};
struct test testtab[] = {
	{ 1, 0, 0x0000, 0x00, 0 },
	{ 0, 0, 0x0041, 0xff, 0 },
	{ 0, 0, 0x0043, 0xff, 0 },
	{ 0, 0, 0x0045, 0xff, 0 },
	{ 0, 0, 0x000b, 0x0f, 0 },
	{ 0, 0, 0x0047, 0xff, 0 },
	{ 0, 0, 0x000d, 0x0f, 0 },
	{ 0, 0, 0x0049, 0xff, 0 },
	{ 0, 0, 0x000f, 0x0f, 0 },
	{ 0, 0, 0x001b, 0xff, 0 },
	{ 0, 0, 0x004b, 0xff, 0 },
	{ 0, 0, 0x004d, 0xff, 0 },
	{ 0, 0, 0x004d, 0xff, 0 },
	{ 0, 0, 0x004f, 0xff, 0 },
	{ 0, 0, 0x0015, 0x04, 0 },
	{ 0, 0, 0x0017, 0x04, 0 },
	{ 0, 0, 0x0019, 0x04, 0 },
	{ 0, 0, 0x0039, 0xff, 0 },
	{ 0, 0, 0x003b, 0xff, 0 },
	{ 0, 0, 0x003d, 0xff, 0 },
	{ 0, 0, 0x002d, 0xff, 0 },
	{ 0, 0, 0x002f, 0xff, 0 },
	{ 0, 0, 0x0031, 0xff, 0 },
	{ 0, 0, 0x0033, 0xff, 0 },
	{ 0, 0, 0x0035, 0xff, 0 },
	{ 0, 0, 0x0037, 0xff, 0 },
	{ 0, 0, 0x0005, 0xf0, 0 },
	{ 0, 0, 0x0007, 0xf0, 0 },
	{ 0, 0, 0x0009, 0xf0, 0 },
	{ 2, 0, 0x0000, 0x00, 0 },
	{ 1, 0, 0x0000, 0x01, 0 },
	{ 3, 0, 0x0081, 0x01, 0 },
	{ 3, 0, 0x0081, 0x00, 0 },
	{ 0, 0, 0x0083, 0xff, 0 },
	{ 0, 0, 0x00c1, 0xff, 0 },
	{ 0, 0, 0x00d1, 0xff, 0 },
	{ 0, 0, 0x00c3, 0xff, 0 },
	{ 0, 0, 0x00d3, 0xff, 0 },
	{ 0, 0, 0x00c5, 0xff, 0 },
	{ 0, 0, 0x00d5, 0xff, 0 },
	{ 0, 0, 0x008b, 0x0f, 0 },
	{ 0, 0, 0x00c7, 0xff, 0 },
	{ 0, 0, 0x00d7, 0xff, 0 },
	{ 0, 0, 0x008d, 0x0f, 0 },
	{ 0, 0, 0x00c9, 0xff, 0 },
	{ 0, 0, 0x00d9, 0xff, 0 },
	{ 0, 0, 0x008f, 0x0f, 0 },
	{ 0, 0, 0x009b, 0xff, 0 },
	{ 0, 0, 0x009d, 0xff, 0 },
	{ 0, 0, 0x00cb, 0xff, 0 },
	{ 0, 0, 0x00db, 0xff, 0 },
	{ 0, 0, 0x00cd, 0xff, 0 },
	{ 0, 0, 0x00dd, 0xff, 0 },
	{ 0, 0, 0x00cd, 0xff, 0 },
	{ 0, 0, 0x00cf, 0xff, 0 },
	{ 0, 0, 0x00df, 0xff, 0 },
	{ 0, 0, 0x0095, 0x04, 0 },
	{ 0, 0, 0x0097, 0x04, 0 },
	{ 0, 0, 0x0099, 0x04, 0 },
	{ 0, 0, 0x00b9, 0xff, 0 },
	{ 0, 0, 0x00bb, 0xff, 0 },
	{ 0, 0, 0x00bd, 0xff, 0 },
	{ 0, 0, 0x00ad, 0xff, 0 },
	{ 0, 0, 0x00af, 0xff, 0 },
	{ 0, 0, 0x00b1, 0xff, 0 },
	{ 0, 0, 0x00b3, 0xff, 0 },
	{ 0, 0, 0x00b5, 0xff, 0 },
	{ 0, 0, 0x00b7, 0xff, 0 },
	{ 0, 0, 0x0085, 0xf0, 0 },
	{ 0, 0, 0x0087, 0xf0, 0 },
	{ 0, 0, 0x0089, 0xf0, 0 },
	{ 2, 0, 0x0000, 0x00, 0 },
	{ 1, 0, 0x0000, 0x02, 0 },
	{ 3, 0, 0x0133, 0x82, 0 },
	{ 0, 0, 0x0125, 0xf0, 0 },
	{ 0, 0, 0x0139, 0xff, 0 },
	{ 0, 0, 0x013b, 0xff, 0 },
	{ 0, 0, 0x013f, 0xfa, 0 },
	{ 3, 0, 0x0113, 0x42, 0 },
	{ 0, 0, 0x0105, 0xf0, 0 },
	{ 0, 0, 0x0119, 0xff, 0 },
	{ 0, 0, 0x011b, 0xff, 0 },
	{ 0, 0, 0x011f, 0xfa, 0 },
	{ 2, 0, 0x0000, 0x00, 0 },
};

/*
 * Report a bad register: force output visible on the console,
 * print the particulars, latch the error flag.
 */
static
report(should, was, port)
int should, was, port;
{
	romconf.has_hires = 2;
	ddtvars.d_conhires = 2;
	romconf.rom_ctype = 1;
	chipinit();
	put_init = 0;
	outstr("\nBad register detected in ");
	outstr(chipnames[curchip]);
	putchar('\n');
	outstr("Port number = ");
	puthex((long)port, 12);
	outstr("  value input = ");
	puthex((long)was, 4);
	outstr("  should have been = ");
	puthex((long)should, 4);
	putchar('\n');
	abflags[0] = 0xff;
}

/*
 * Walk a one bit through the register at port, under mask;
 * report any bit that reads back wrong.  Leave the register clear.
 */
static
walkbit(port, mask)
int port, mask;
{
	char pat;
	int i;
	char got;

	for (i = 0, pat = 1; i < 8 && abflags[0] == 0; i++, pat <<= 1) {
		if ((mask & pat) == 0)
			continue;
		outb(port, pat);
		if ((got = inb(port) & mask) != pat)
			report(pat, got, port);
	}
	outb(port, 0);
}

/*
 * Walk the test script.
 */
selftest()
{
	register struct test *t;

	for (t = testtab; t < &testtab[84]; t++) {
		if (abflags[0])
			break;
		switch (t->t_op) {
		case 0:
			walkbit(t->t_port, t->t_val);
			break;
		case 1:
			curchip = t->t_val;
			outstr(chipnames[curchip]);
			outstr(" register test: ");
			break;
		case 2:
			romconf.rom_ctype = 1;
			put_init = 0;
			chipinit();
			wrport(rdport() + 1);
			outstr("OK\n");
			break;
		case 3:
			outb(t->t_port, t->t_val);
			break;
		}
	}
}
