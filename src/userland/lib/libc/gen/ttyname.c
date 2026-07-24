/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent I/O Library
 * Return the name of the terminal device
 * associated with the given file descriptor.
 */
#include <stdio.h>
#include <stat.h>
#include <dir.h>
#include <canon.h>

static	char	tname[6+DIRSIZ] = "/dev/console";
#define	tstart	(tname+5)		/* Where to put terminal name */

char *
ttyname(fd)
int fd;
{
	struct stat   sb;
	struct direct db;
	register devfd;

	if (fstat(fd, &sb)<0 || (sb.st_mode&S_IFMT)!=S_IFCHR)
		return (NULL);
	if ((devfd = open("/dev", 0)) < 0)
		return (NULL);
	while (read(devfd, &db, sizeof(db)) == sizeof(db)) {
		canino(db.d_ino);
		if (db.d_ino == sb.st_ino) {
			strncpy(tstart, db.d_name, DIRSIZ);
			close(devfd);
			return (tname);
		}
	}
	close(devfd);
	return (NULL);
}
