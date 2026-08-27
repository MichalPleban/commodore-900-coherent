/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Print out the tty name of the
 * current user.
 */

#include <stdio.h>
char	*ttyname();

main(argc, argv)
char *argv[];
{
	char *tty;

	if (isatty(fileno(stderr)) == 0
	 || (tty = ttyname(fileno(stderr))) == NULL) {
		printf("Not a tty\n");
		return (1);
	}
	printf("%s\n", tty);
	return (0);
}
