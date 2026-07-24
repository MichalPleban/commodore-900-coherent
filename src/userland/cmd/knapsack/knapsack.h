/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Header for knapsack encryption scheme.
 */
#include <mprec.h>

#define K	64	/* Length of knapsack vector. */
#define R	56	/* Modulus of random number gen. is 2^R. */
#define PKCL	16	/* Public Key Component Length when stored. */

/*
 * knapsack structure
 */
struct knapsack {
	mint m1;
	mint w1;
	mint w1inv;
	mint m2;
	mint w2;
	mint w2inv;
	mint d[K];
	int shufl[K];
};

#define	or	||
#define	and	&&
#define	not	!
#define	TRUE	(0==0)
#define	FALSE	(not TRUE)

#define	PUBKEYDIR	"/usr/spool/pubkey"
#define	PPSIZ		130	/* Passphrase buffer size. */

#define	MCFBAS		95
#define	MCFZERO		040
#define	MCFL		21

/*
 * Function returning non-int.
 */
char *pubkeyfile();

