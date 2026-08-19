/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * moo - the number-guessing game ("bulls and cows").
 * Reconstructed from the disassembly of the original binary
 * (see disasm/moo.c); behavior matches the original exactly.
 *
 * moo [n]
 * The machine picks n (default 4, 1<=n<=10) pairwise-different
 * decimal digits.  Each line you type is scored: a digit in the
 * right place is a bull, a digit present but misplaced is a cow.
 * All bulls wins and starts a new game; end of file quits.
 */

#include <stdio.h>

char	entdiff[] = "Please enter a string of %d digits, all different\n";

long	time();

main(argc, argv)
int argc;
char *argv[];
{
	long tod;
	int cows, bulls;
	int i, j;
	char guess[256];
	int n;
	char secret[10];

	time(&tod);
	srand((int)(tod >> 16) + (int)tod);

	if (argc < 2)
		n = 4;
	else
		n = atoi(argv[1]);
	if (n < 1 || n > 10) {
		printf("Usage: moo [n]\n\twhere 1<=n<=10, n defaults to 4\n");
		exit(1);
	}

newgame:
	printf("New game.\n");

	/*
	 * Pick n different digits, rerolling any digit
	 * that repeats an earlier one.
	 */
	for (j = 0; j != n; j++) {
regen:
		secret[j] = rand()/100%10 + '0';
		for (i = 0; i != j; i++)
			if (secret[i] == secret[j])
				goto regen;
	}

	for (;;) {
		if (gets(guess) == NULL)
			exit(0);
		if (badline(guess, n)) {
			printf(entdiff, n);
			continue;
		}
		bulls = 0;
		cows = 0;
		for (j = 0; j != n; j++)
			for (i = 0; i != n; i++)
				if (secret[i] == guess[j]) {
					if (j == i)
						bulls++;
					else
						cows++;
				}
		if (bulls == n) {
			printf("Right!\n");
			goto newgame;
		}
		printf("%d bull%c, %d cow%c\n",
		    bulls, bulls == 1 ? '\0' : 's',
		    cows, cows == 1 ? '\0' : 's');
	}
}

/*
 * A line is bad unless it is exactly n characters long,
 * all decimal digits, all different.
 */
badline(s, n)
char *s;
int n;
{
	register int i, j;

	if (strlen(s) != n)
		return (1);
	for (i = 0; i != n; i++) {
		if (s[i] < '0' || s[i] > '9')
			return (1);
		for (j = 0; j != i; j++)
			if (s[j] == s[i])
				return (1);
	}
	return (0);
}
