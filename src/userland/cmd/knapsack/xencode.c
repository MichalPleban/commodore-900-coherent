/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * xencode [user].
 * Encode stdin for user. If user is missing prompt for a passphrase,
 * make a public key, and use it to encode stdin.
 */
#include <stdio.h>
#include <canon.h>
#include "knapsack.h"

#define	testbit(cp, n)	(((cp)[((n)&~07)>>3]) & mask[n&07])
#define	MSGSIZ		2048

int	mask[8] = {01, 02, 04, 010, 020, 040, 0100, 0200};
char	msg[MSGSIZ];
struct	knapsack k;
mint	key[K];
mint	m;
mint	t;

main(ac, av)
int ac;
char *av[];
{
	register int n;
	register char *cp;

	init(ac, av);
	while ((n = getmsg()) != 0) {
		srand(n);
		putheader(n);
		while (n % (K/8) != 0)
			msg[n++] = rand() % 256;
		for (cp = msg; n > 0; cp += (K/8), n -= (K/8)) {
			xencrypt(cp, &m);
			mcfout(&m);
		}
	}
	return (0);
}

init(ac, av)
int ac;
char *av[];
{
	register int i;
	register char *cp;
	register FILE *fp;

	av[0][0] = '\0';
	switch (ac) {
	case 1:
		gpph("Passphrase: ", msg);
		knapsack(&k, msg);
		public(key, &k);
		break;
	case 2:
		cp = pubkeyfile(av[1]);
		if ((fp = fopen(cp, "r")) == NULL) {
			fputs(av[1], stderr);
			fputs(" is not enrolled.\n", stderr);
			exit(1);
		}
		while (*av[1])
			*av[1]++ = '\0';
		for (i = 0; i < K; ++i)
			pkin(&key[i], fp);
		free(cp);
		fclose(fp);
		break;
	default:
		fputs("Usage: xencode [user]\n", stderr);
		exit(1);
	}
	return;
}


getmsg()
{
	register char *cp = msg;

	while (cp < msg + MSGSIZ)
		if ((*cp++ = getchar()) == EOF) {
			ungetc(*--cp, stdin);
			break;
		}
	return (cp - msg);
}

putheader(n)
register int n;
{
	static char hbuf[K/8];

	hbuf[1] = n % 256;
	hbuf[0] = n / 256;
	for (n = 2; n < K/8; ++n)
		hbuf[n] = rand() % 256;
	xencrypt(hbuf, &m);
	mcfout(&m);
	return;
}

/*
 * Encrypt a block of characters.
 */
xencrypt(buf, m)
register char *buf;
register mint *m;
{
	register int i;

	mitom(0, m);
	for (i = 0; i < K; ++i)
		if (testbit(buf, i))
			madd(&key[i], m, m);
	return;
}

/*
 * Write a mint to stdin in "mail-compatible format".
 * This means a base of MCFBAS, always MCFL digits with a trailing newline,
 * least significant digit first, and digits offset by MCFZERO to bring them
 * out of control character range (and into "mail- compatible range).
 */
mcfout(m)
register mint *m;
{
	register int i;
	int a;

	for (i = MCFL; i > 0; --i) {
		sdiv(m, MCFBAS, m, &a);
		putchar(a + MCFZERO);
	}
	putchar('\n');
}

