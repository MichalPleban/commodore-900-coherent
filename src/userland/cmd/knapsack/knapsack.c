/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Knapsack() creates a knapsack structure from the passphrase s.
 * The knapsack is placed in the structure pointed to by kp.
 * Sxrand sets a multi-precision random number generator to return values
 * in the range 0 to (2^exp - 1). Xrand calls that r.n.g.
 */
#include <stdio.h>
#include "knapsack.h"

static	mint	t1, t2;		/* Temporaries for knapsack(). */
static	mint	g2R;		/* Used to hold 2^R for wpair(). */
static	mint	g;		/* Used by choose() and as a temporary. */

static	mint	m;		/* modulus for the r.n.gen. */
static	mint	a;		/* multiplier for r.n.gen. */
static	mint	c;		/* increment for r.n.gen.  */
static	mint	seed;		/* seed for the r.n. gen. */
static	mint	t;		/* temporary for the r.n.g. */

knapsack(kp, s)
register struct knapsack *kp;
char *s;
{
	register int i;

	sxrand(R, s);
	mitom(2, &g);
	spow(&g, R, &g);
	mcopy(&g, &g2R);

	/*
	 * Use choose() to pick the k.d's and k.m.
	 */
	for (i = 0; i < K; ++i)
		choose(&kp->d[i]);
	choose(&kp->m1);

	/*
	 * To choose m2 we want g = 2^(8 + K + R). Right now g = 2^(K + R + 1).
	 */
	mitom(128, &t1);
	mult(&g, &t1, &g);
	choose(&kp->m2);

	/*
	 * Choose w1, w1inv, w2 and w2inv.
	 * Note that it is now ok to use g for a temporary.
	 */
	wpair(&kp->m1, &kp->w1, &kp->w1inv);
	wpair(&kp->m2, &kp->w2, &kp->w2inv);

	/*
	 * Before we shuffle k.shufl, seed rand() from the knapsack info.
	 */
	mdiv(&kp->d[0], mmaxint, &t1, &g);
	srand(mtoi(&g));
	shuffle(kp->shufl);

	return;
}


/*
 * Repeated calls to choose(m) pick m so that   g - 2^R < m <= g.
 * g is multiplied by 2 each time. Calling choose() as is done above in
 * knapsack() ensures that k.d[] and k.m have the super-sequence property.
 */
static
choose(m)
register mint *m;
{
	xrand(m);
	msub(&g, m, m);
	smult(&g, 2, &g);
	return;
}

static
shuffle(buf)
register int buf[K];
{
	register int i;
	register int j;
	register int k;

	for (i = 0; i < K; ++i)
		buf[i] = i;
	for (i = 0; i < K; ++i) {
		j = rand() % (K - i) + i;
		k = buf[i];
		buf[i] = buf[j];
		buf[j] = k;
	}
	return;
}

/*
 * Select w and winv for m. Note that it is now ok to use g as a temporary.
 */
static
wpair(m, w, winv)
register mint *m, *w, *winv;
{
	do {
		xrand(w);
		msub(m, w, w);
		msub(w, &g2R, w);
		while (gcd(m, w, &g), mcmp(&g, mone) > 0)
			for (;;) {
				mdiv(w, &g, &t1, &t2);
				if (not zerop(&t2))
					break;
				mcopy(&t1, w);
			}
	}while (mcmp(w, &g2R) < 0);	/* Insures minimum size of w. */

	xgcd(m, w, &t1, winv, &t2);
	mdiv(winv, m, &t1, winv);
	if (not ispos(winv))
		madd(winv, m, winv);

	return;
}

/*
 * Multiprecision random number generator for knapsack encryption.
 * General ref. Knuth - Art of Computer Programming.
 * This is a linear congruential r.n.g. Such a generator has three parameters,
 *	the modulus m, the additive generator c, and the multiplicative
 *	generator a. It generates a sequence {r0, r1, r2, ...} of pseudo-random
 *	variables as follows: r0 is set to some initial value by sxrand(). Then
 *	r1 = (r0 * a + c) mod m, and this calculation is iterated to get
 *	r2, r3, etc.
 * We have m a power of two. We choose a to be the odd power of 5 closest to
 *	but less than m. Knuth suggests c should be close to
 *	(3 - sqrt(3)) * m / 6	and odd.
 */

/*
 * Put a random variable into r and return.
 */
static
xrand(r)
register mint *r;
{
	mult(&seed, &a, &seed);
	madd(&seed, &c, &seed);
	mdiv(&seed, &m, r, &seed);		/* r used as temporary here */
	mcopy(&seed, r);
	return;
}


/*
 * Initialize the random number generator. This means initializing the
 * mints m, a and c as well as seed.
 */
static
sxrand(exp, s)
register unsigned exp;
register char *s;
{
	short	tshort;			/* temporary for sdiv */

	/*
	 * Initialize m to be 2^exp.
	 */
	mitom(2, &m);			/* m = 2 */
	spow(&m, exp, &m);		/* m = 2^exp */

	/*
	 * Initialize a to be the odd power of 5 closest to but less than m.
	 * This power will be nearly exp * (log base 5 of 2), and
	 * (log base 5 of 2) is about .4306765. We first make this power a
	 * little larger than it need be.
	 */
	exp = (unsigned int) ((long)exp * 43068L / 100000L); 
	if (exp % 2 == 0)
		--exp;			/* make exp odd */
	mitom(5, &a);			/* a = 5 */
	spow(&a, exp, &a);		/* a = 5^exp */
	while (mcmp(&m, &a) <= 0)	/* a is not yet less than m */
		sdiv(&a, 25, &a, &tshort);

	/*
	 * Initialize c. Knuth suggests c be about   (3 - sqrt(3)) * m / 6
	 * and odd. We go out of the way to avoid serious roundoff with the
	 * sqrt(). Seed is used as a temporary here.
	 */
	smult(&m, 3, &c);		/* c = 3*m */
	mult(&c, &m, &t);		/* t = 3*m^2 */
	msqrt(&t, &t, &seed);		/* t = sqrt(3)*m */
	msub(&c, &t, &c);		/* c = (3 - sqrt(3)) * m */
	sdiv(&c, 6, &c, &tshort);	/* almost done, now make it odd */
	sdiv(&c, 2, &t, &tshort);
	if (tshort == 0)
		msub(&c, mone, &c);

	/*
	 * Initialize seed. Start with seed = c, and massage it with the
	 * characters of string s. Exp is used as a register temporary here.
	 */
	mcopy(&c, &seed);
	while ((exp = *s++) != '\0') {
		mitom(exp, &t);
		mult(&seed, &t, &seed);
		madd(&seed, &c, &seed);
		mdiv(&seed, &m, &t, &seed);
	}

	return;
}

