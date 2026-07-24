/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Convert an array of longs into an array
 * of filesystem 3 byte numbers. This routine, unlike
 * the old one, is independent of the order of bytes in
 * a long. Bytes have 8 bits.
 */
ltol3(cp, lp, nl)
register char *cp;
register long *lp;
register unsigned nl;
{
	register long l;

	if (nl != 0) {
		do {
			l = *lp++;
			cp[0] = l >> 16;
			cp[1] = l;
			cp[2] = l >> 8;
			cp += 3;
		} while (--nl);
	}
}
