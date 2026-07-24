/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include "tsort.h"


/*
 *	Actual definition of external variables.
 */

struct wordlist *words;		/* linked list of all words */


/*
 *	Die is used to exit from tsort with some error
 *	message.
 */

void
die(str)
char *str;
{
	fprintf(stderr, "tsort: %r", &str);
	putc('\n', stderr);
	exit(1);
}
