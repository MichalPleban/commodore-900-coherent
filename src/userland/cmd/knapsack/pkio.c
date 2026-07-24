/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * These routines write and read public key elements. The stream f is assumed
 * to be open in an appropriate mode and positioned appropriately. One element
 * is written/read at a time.
 * Elements are stored in a base 256 format, least significant digit first.
 * Always, PKCL (see knapsack.h) digits are stored per element.
 * Its easiest to do the input via fread and output via successive putc's.
 */
#include <stdio.h>
#include "knapsack.h"

typedef	unsigned char	Uchar;

static	mint	t;			/* temporary */
static	mint	m256;			/* Mint to hold value 256. */
static	int	buf[PKCL];		/* Must be int for sdiv in pkout. */

pkin(m, f)
register mint *m;
FILE *f;
{
	register Uchar *cp;

	mitom(256, &m256);
	mitom(0, m);
	if (fread((Uchar *)buf, 1, PKCL, f) != PKCL) {
		fprintf(stderr, "Public key data corrupted.\n");
		exit(1);
	}
	for (cp = ((Uchar*)buf) + PKCL - 1; cp >= ((Uchar*)buf); --cp) {
		mult(m, &m256, m);
		mitom(*cp, &t);
		madd(m, &t, m);
	}
	return;
}

pkout(m, f)
mint *m;
register FILE *f;
{
	int a;
	int b;
	register int i;

	mcopy(m, &t);
	for (i = PKCL; i > 0; --i) {
		/* Doing the divide this way is faster than using mdiv */
		sdiv(&t, 16, &t, &b);
		sdiv(&t, 16, &t, &a);
		putc(b + 16 * a, f);
	}
	return;
}

