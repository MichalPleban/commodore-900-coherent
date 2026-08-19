/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * fortune - print a randomly selected saying.
 *
 * Reads the fortunes file (one fortune per line, '@' marking an
 * embedded newline) and keeps line i with probability 1/(i+1), so
 * every line is equally likely after one pass.  The saved buffer
 * starts out holding the EOF message, which is therefore what an
 * empty file prints.
 *
 * Reconstructed from the original Coherent binary (see
 * disasm/fortune.asm and disasm/fortune.c).
 */

#include <stdio.h>
#include <timeb.h>

char	savef[1000] = "EOF on fortune file\n";
char	*fortfile = "/usr/games/lib/fortunes";
char	fortline[1000];

main(argc, argv)
int	argc;
char	*argv[];
{
	register FILE *fp;
	register int i;
	struct timeb tb;

	ftime(&tb);
	srand((int)tb.time ^ tb.millitm);
	if (argc > 2)
		usage();
	if (argc > 1)
		fortfile = argv[1];
	if ((fp = fopen(fortfile, "r")) == NULL) {
		fprintf(stderr, "Cannot open fortunefile: %s\n", argv[1]);
		return (1);
	}
	for (i = 0; fgets(fortline, 1000, fp) != NULL; i++)
		if (grand(i) == i)
			save(fortline);
	prfortune(savef);
}

/*
 * Print the chosen fortune, turning '@' into a newline.
 */
prfortune(cp)
register char *cp;
{
	register int c;

	while ((c = *cp++) != 0) {
		if (c == '@')
			c = '\n';
		putchar(c);
	}
}

/*
 * Remember the currently chosen line.
 */
save(cp)
char	*cp;
{
	strcpy(savef, cp);
}

/*
 * Random integer in [0, n].
 */
grand(n)
int	n;
{
	return ((rand() >> 2) % (n + 1));
}

usage()
{
	fprintf(stderr, "usage: fortune [fortune_file]\n");
	exit(1);
}
