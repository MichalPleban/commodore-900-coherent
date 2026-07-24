/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Make a special file
 */
#include <stdio.h>
#include <ino.h>
#include <stat.h>

main(argc, argv)
register char **argv;
{
	register int mode;
	register int addr;

	if (argc < 3)
		usage();
	switch (argv[2][0]) {
	case 'b':
		if (argc != 5)
			usage();
		mode = IFBLK;
		addr = makedev(atoi(argv[3]), atoi(argv[4]));
		break;
	case 'c':
		if (argc != 5)
			usage();
		mode = IFCHR;
		addr = makedev(atoi(argv[3]), atoi(argv[4]));
		break;
	case 'p':
		if (argc != 3)
			usage();
		mode = IFPIPE;
		addr = 0;
		break;
	default:
		usage();
	}
	mode |= 0666;
	if (mknod(argv[1], mode, addr) < 0) {
		fprintf(stderr, "mknod: cannot create node %s\n", argv[1]);
		return (1);
	}
	return (0);
}

/*
 * Print out a usage message.
 */
usage()
{
	fprintf(stderr, "usage: mknod <name> [bcp] <major> <minor>\n");
	exit(1);
}
