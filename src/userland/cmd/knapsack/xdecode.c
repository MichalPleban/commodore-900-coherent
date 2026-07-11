/*
 * Xdecode - knapsack decryption filter.
 */
#include <stdio.h>
#include <canon.h>
#include "knapsack.h"

#define	setbit(cp, n)	(((cp)[((n)&~07)>>3]) |= mask[n&07])

int	mask[8] =	{01, 02, 04, 010, 020, 040, 0100, 0200};

char	buf[PPSIZ];
struct	knapsack k;
mint	tbl[MCFL];
mint	m;
mint	t;


main()
{
	register int n;

	gpph("Passphrase: ", buf);
	if (buf[0] == '\0')
		exit(0);
	knapsack(&k, buf);

	mcfinit();
	while ((n = getheader()) != EOF)
		while (n > 0) {
			if (mcfin(&m) == EOF)
				exit(0);
			xdecrypt(buf, &m);
			if (n < K/8)
				fwrite(buf, 1, n, stdout);
			else
				fwrite(buf, 1, K/8, stdout);
			n -= K/8;
		}
	return (0);
}

getheader()
{
	register unsigned char *uc = buf;

	if (mcfin(&m) == EOF)
		return (EOF);
	xdecrypt(buf, &m);
	return (uc[0] * 256 + uc[1]);
}

/*
 * Initialize tbl[] for use by mcfin().
 */
static
mcfinit()
{
	register mint *a;

	mcopy(&k.w2inv, tbl);
	for (a = tbl + 1; a < tbl + MCFL; ++a) {
		smult(a-1, MCFBAS, a);
		mdiv(a, &k.m2, &t, a);
	}
	return;
}

/*
 * Read in an encrypted block from stdin.
 * The block should be written in "mail-compatible format": base MCFBAS
 * (see knapsack.h), MCFL digits with a trailing newline, least significant
 * digit first, and digits offset by MCFZERO to bring them out of control
 * character range (and into "mail-compatible" range).
 */
mcfin(m)
register mint *m;
{
	register char *cp;
	register mint *tp;
	static char inbuf[MCFL + 1];

	if (fread(inbuf, 1, MCFL + 1, stdin) == 0)
		return (EOF);
	mitom(0, m);
	cp = inbuf + MCFL;
	tp = tbl + MCFL;
	while (tp > tbl) {
		smult(--tp, *--cp - MCFZERO, &t);
		madd(m, &t, m);
	}
	return ('\n');
}

/*
 * Decrypt the mint m, put the result in buf.
 * Note that because of our use of tbl to streamline calculations, m is
 * already effectively the m.c.format block time w2inv.
 */
xdecrypt(buf, m)
register char *buf;
register mint *m;
{
	register int i;

	mdiv(m, &k.m2, &t, m);
	mult(m, &k.w1inv, m);
	mdiv(m, &k.m1, &t, m);

	for (i = 0; i < K/8; ++i)
		buf[i] = '\0';
	for (i = K-1; i >= 0; --i)
		if (mcmp(m, &k.d[i]) >= 0) {
			msub(m, &k.d[i], m);
			setbit(buf, k.shufl[i]);
		}
	return;
}

