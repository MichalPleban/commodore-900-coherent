/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Make a link
 */

#include <stdio.h>
#include <stat.h>
#include <errno.h>

struct	stat	sb;
char	namebuf[200];
char	usemsg[] = "\
Usage: ln [-f] oldfile [newlink]\n\
       ln [-f] name ... dir\n\
";

int	fflag;
extern	int	errno;

main(argc, argv)
char *argv[];
{
	register int i;
	register int estat = 0;
	char *lname;

	if (argc>2 && *argv[1]=='-') {
		if (argv[1][1]=='f' && argv[1][2]=='\0')
			fflag = 1;
		else
			usage();
		argc--;
		argv++;
	}
	if (argc < 2)
		usage();
	lname = argv[argc-1];
	if (stat(lname, &sb)>=0 && (sb.st_mode&S_IFMT)==S_IFDIR)
		for (i=1; i<argc-1; i++)
			estat |= lndir(argv[i], lname);
	else {
		if (argc > 3)
			usage();
		if (argc == 2)
			estat = lndir(argv[1], ".");
		else
			estat = ln(argv[1], argv[2]);
	}
	exit(estat);
}

/*
 * Do the link and
 * check for errors.
 * Remove the file and try again
 * if the `-f' is specified.
 */
ln(n1, n2)
char *n1, *n2;
{
	if (link(n1, n2) < 0) {
		if (fflag && errno==EEXIST) {
			if (stat(n2, &sb) >= 0
			 && (sb.st_mode&S_IFMT) != S_IFDIR
			 && unlink(n2) >= 0
			 && link(n1, n2) >= 0)
				return (0);
		}
		perror("link");
		return (1);
	}
	return (0);
}

/*
 * Do a link into a directory.
 * Generate a name.
 */
lndir(n1, n2)
char *n1, *n2;
{
	register char *rp, *sp, *cp;

	for (sp = rp = n1; *rp != '\0'; )
		if (*rp++ == '/')
			sp = rp;
	for (cp=namebuf, rp=n2; *rp != '\0'; )
		*cp++ = *rp++;
	*cp++ = '/';
	while (*cp++ = *sp++)
		;
	return (ln(n1, namebuf));
}

usage()
{
	fprintf(stderr, usemsg);
	exit(1);
}
