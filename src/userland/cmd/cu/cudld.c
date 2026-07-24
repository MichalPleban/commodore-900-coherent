/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Downloading routines for use by
 * the CU terminal emulator.
 * A packet consists of an STX, packet type and sequence, information,
 * an ETX and then a two character CRC character.
 */

#include <stdio.h>
#include <signal.h>
#include "cu.h"

PKT	rpkt;			/* Received data (no windowing yet) */
PKT	xpkt;			/* Transmitted data */
static	unsigned accrc;		/* For incremental CC-16 computation */
int	nretry;			/* Number of retries */

int	timeout();

/*
 * Encode characters that need escaping on the line
 * Any entry that is zero need not be touched.
 */
static	char	encode[256] = {
	0100, 0101, 0102, 0103, 0104, 0105, 0106, 0107,		/*000-007*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*010-017*/
	0120, 0121, 0122, 0123, 0000, 0000, 0000, 0000,		/*010-017*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*030-037*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*040-047*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*050-057*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*060-067*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*070-077*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*100-107*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*110-117*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*120-127*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*130-137*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*140-147*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*150-157*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,		/*160-167*/
	0000, 0000, 0000, 0000, 0000, 0000, 0000, 0077,		/*170-177*/
};

/*
 * Callled to initialise everything
 * for the packet routines.
 */
pktinit()
{
	signal(SIGALRM, timeout);
}

PKT *
getpkt()
{
	register unsigned char c;
	register char *cp;
	register PKT *pp;

	alarm(TLEN);
	pp = &rpkt;
	cp = &pp->p_type;
	pp->p_flags = 0;
	accrc = 0;
	while ((c = sget()) != STX)
		if (pp->p_flags != 0)
			goto bad;
	while ((c = sget()) != ETX) {
		if (pp->p_flags != 0)
			goto bad;
		if (c == DLE) {
			if ((c = sget()) == EOF)
				break;
			c ^= 0100;
		}
		crcchar(c);
		*cp++ = c;
	}
	checkcrc(pp);
bad:
	pp->p_len = cp - (char *)(&pp->p_type);
	alarm(0);
	return (pp);
}

/*
 * Put out a packet of length `nb'
 * bytes.  This also sends the
 * proper CRC-16 check character.
 */
putpkt(pp)
PKT *pp;
{
	register unsigned char *cp;
	register unsigned nb;
	register unsigned c;

	sput(STX);
	accrc = 0;
	cp = &pp->p_type;
	nb = pp->p_len;
	do {
		mput(*cp++);
	} while (--nb);
	sput(ETX);
	c = accrc;
	mput(c&0xFF);
	mput(c>>8);
}

/*
 * Mapped putting of a character.
 */
mput(c)
register int c;
{
	register int c1;

	if ((c1 = encode[c]) != 0) {
		sput(DLE);
		sput(c1);
	} else
		sput(c);
	crcchar(c);
}

/*
 * Send the packet using the lower-level routines to do
 * retransmission on the line. 
 */
sendpkt(xpp)
register PKT *xpp;
{
	register PKT *rpp;

	for (;;) {
		putpkt(xpp);
		rpp = getpkt();
		if (rpp->p_type == 'E')
			doend(rpp);
		if (rpp->p_flags==0 && rpp->p_type=='A')
			break;
		/* Error */
		nretry++;
	}
}

/*
 * Receive a packet.
 * Manage the sequence numbers.
 */
PKT *
rcvpkt(seqno)
unsigned seqno;
{
	register PKT *rpp;
	register unsigned rseq;

	for (;;) {
		rpp = getpkt();
		if (rpp->p_flags != 0) {
			nretry++;
			nakpkt();
			continue;
		}
		if (rpp->p_type == 'E')
			doend(rpp);
		rseq = (rpp->p_seq[1] << 8) + rpp->p_seq[0];
		if (rpp->p_type=='I' && rseq!=seqno)
			nakpkt();
		else
			break;
	}
	return (rpp);
}

/*
 * Generate a Nak packet
 */
nakpkt()
{
	xpkt.p_type = 'N';
	xpkt.p_len = 1;
	putpkt(&xpkt);
}

/*
 * Generate an acknowledgement
 */
ackpkt()
{
	xpkt.p_type = 'A';
	xpkt.p_len = 1;
	putpkt(&xpkt);
}

/*
 * Generate an end packet.
 */
endpkt(s, f)
register char *s;
int f;
{
	register unsigned n;

	xpkt.p_type = 'E';
	n = strlen(s)+1;
	xpkt.p_len = n + 3;
	strncpy(&xpkt.p_data[0], s, n);
	putpkt(&xpkt);
	if (f)
		getpkt();
}

/*
 * Read in a CRC from the line
 * and compare it to the expected one.
 */
checkcrc(pp)
PKT *pp;
{
	register int c;
	register int i;
	register unsigned crc;
	static unsigned char x[2];

	for (i=0; i<2; i++) {
		if ((c = sget()) == EOF)
			break;
		if (c == DLE) {
			if ((c = sget()) == EOF)
				break;
			c ^= 0100;
		}
		x[i] = c;
	}
	crc = x[0] | (x[1]<<8);
	if (crc != accrc)
		pp->p_flags |= PCRC; else
		pp->p_flags &= ~PCRC;
}

/*
 * Called on line timeout.
 */
timeout()
{
	signal(SIGALRM, timeout);
	rpkt.p_flags |= PTOUT;
}

/*
 * CRC Calculation using the BNR fast Matrix Multiply
 * and Table Lookup Algorithm
 */

static int ytab[] = {
        0, 0146001, 0154001, 012000, 0170001, 036000,
	024000, 0162001, 0120001, 066000, 074000,
	0132001, 050000, 0116001, 0104001, 042000
};

static int ztab[] = {
        0, 0140301, 0140601, 0500, 0141401, 01700,
        01200, 0141101, 0143001, 03300, 03600, 0143501,
        02400, 0142701, 0142201, 02100
};

crcchar(c)
register c;
{
        accrc ^= c;
        accrc = (accrc>>8) ^ ztab[accrc&0xF] ^ ytab[(accrc>>4)&0xF];
}
