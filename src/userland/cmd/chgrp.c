/*
 * Change the group owner of specified files.
 */

#include <stdio.h>
#include <stat.h>
#include <grp.h>

main(argc, argv)
char *argv[];
{
	register struct group *grp;
	register int c;
	register short owner, group;
	struct stat sb;
	register short status = 0;

	if (argc < 3)
		usage();
	if ((c = *argv[1])>='0' && c<='9')
		group = atoi(argv[1]);
	else {
		if ((grp = getgrnam(argv[1])) == NULL)
			cherr("Bad username `%s'\n", argv[1]);
		group = grp->gr_gid;
	}
	for (c = 2; c < argc; c++) {
		owner = 0;
		if (stat(argv[c], &sb) >= 0)
			owner = sb.st_uid;
		if (chown(argv[c], owner, group) < 0) {
			perror(argv[c]);
			status = 2;
		}
	}
	exit (status);
}

usage()
{
	fprintf(stderr, "Usage: chgrp group file ...\n");
	exit(1);
}

/* VARARGS */
cherr(x)
{
	fprintf(stderr, "chgrp: %r", &x);
	exit(2);
}




