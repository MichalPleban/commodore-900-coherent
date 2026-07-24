/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Console driver for Commodore Z8000M.
 * Uses on-board RS-232 port B (SCC channel B) for all I/O.
 */
#include <coherent.h>

#define	RxAVAIL	0x01		/* character available */
#define	TxEMPTY	0x04		/* transmit buffer empty */
#define	WR0	0x0101		/* command register */
#define	RR0	0x0101		/* Tx & Rx status */
#define	WR8	0x0111		/* transmit buffer */
#define	RR8	0x0111		/* receive buffer */


/*
 * Print a character on the console.
 */
putchar(c)
register int c;
{
	register int s;

	if (c == '\n')
		putchar('\r');
	s = sphi();
	while ((inb(RR0)&TxEMPTY) == 0)
		;
	outb(WR8, c);
	spl(s);
}

/*
 * Get a character from the
 * console. Echo it. Map carriage
 * return into newline.
 */
getchar()
{
	register c;

	while ((inb(RR0)&RxAVAIL) == 0)
		;
	if ((c = inb(RR8)&0x7F) == '\r')
		c = '\n';
	putchar(c);
	return (c);
}
