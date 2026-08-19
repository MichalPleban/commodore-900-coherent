/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Format a floppy disk.
 *
 * The named device is opened read/write and the format request is
 * handed to the driver as a single argumentless ioctl; the Commodore
 * floppy controller formats the entire disk in one command (CFFMT,
 * see wdioctl()/wdformat() in the wd driver).  The command code comes
 * from <fdioctl.h>.
 */
#include <stdio.h>
#include <fdioctl.h>

/*
 * For the C compiler.
 */
int	usage();

/*
 * Variables.
 */
char	*progname;			/* argv[0], for messages */

main(argc, argv)
int argc;
char *argv[];
{
	register int fd;

	progname = argv[0];
	if (argc != 2)
		usage();
	fd = open(argv[1], 2);
	if (fd < 0) {
		fprintf(stderr, "%s: can't open %s\n", progname, argv[1]);
		exit(1);
	}
	if (ioctl(fd, FDFORMAT, (char *)0) < 0) {
		perror(progname);
		exit(1);
	}
	return;
}

usage()
{
	fprintf(stderr, "Usage: %s device\n", progname);
	exit(1);
}
