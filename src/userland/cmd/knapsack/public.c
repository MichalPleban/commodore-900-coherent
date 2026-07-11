/*
 * Public() creates a public key from the knapsack structure *k and stores it
 * in the mint array pointed to by key. The public key is scrambled here.
 * Given a user, pubkeyfile() returns the appropriate public key file name.
 * Pubkeyfile() is included in this file since it is called by enroll
 * and xencode, the same programs that call public().
 */
#include <stdio.h>
#include "knapsack.h"

static	char	pubkeydir[] = PUBKEYDIR;
static	mint	t;

public(key, k)
mint *key;
register struct knapsack *k;
{
	register mint *a;
	register int i;

	for (i = 0; i < K; ++i) {
		a = key + k->shufl[i];
		mult(&k->d[i], &k->w1, a);
		mdiv(a, &k->m1, &t, a);
		madd(a, &k->m1, a);
		mult(a, &k->w2, a);
		mdiv(a, &k->m2, &t, a);
	}
	return;
}

char *
pubkeyfile(cp)
char *cp;
{
	register char *ret;
	register int a;

	a = strlen(pubkeydir);
	if ((ret = malloc(a + 2 + strlen(cp))) == NULL) {
		fputs(stderr, "Out of memory\n");
		exit(1);
	}
	strcpy(ret, pubkeydir);
	ret[a] = '/';
	strcpy(ret + a + 1, cp);
	return (ret);
}

