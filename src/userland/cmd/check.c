/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * check -- check all of the normally-used filesystems
 * or those specified by calling `icheck' and `dcheck'.
 * If `-s' is specified, also try to correct any
 * problems encountered in any of these filesystems.
 */

/*

return '-2' (254) if an internal error occurred; '-1' (255) if
there are unfixed errors; '0' if no errors were found; and '1'
if errors were found, but fixed (system should then be rebooted).

*/

#include <stdio.h>
#include <check.h>
#include <filsys.h>

char	icheck[] = "/bin/icheck";
char	dcheck[] = "/bin/dcheck";
char	fixopt[] = "-s";

int	sflag;

main(argc, argv)
char *argv[];
{
	register estat = 0;
	register char **fsp;

	if (argc>1 && *argv[1]=='-') {
		if (argv[1][1]=='s' && argv[1][2]=='\0')
			sflag = 1;
		else
			usage();
		argv++;
		argc--;
	}
	if (argc > 1)
		fsp = &argv[1];
	else
		usage();
	while (*fsp != NULL) {
		register int rv;

		rv = check(*fsp);
		estat |= rv;
		/*
		 * With -s, a clean (0) or fully-fixed (1) result means the
		 * file system is now consistent: mark it clean on disk so it
		 * mounts (and root auto-promotes) read/write again.
		 */
		if (sflag && rv >= 0)
			clrdirty(*fsp);
		fsp++;
	}
	exit(estat);
}

/*
 * Clear the dirty flag in the super block of file system `fs'.  Reads and
 * rewrites the raw super block block, touching only the (uncanonized) byte
 * s_dirty so the canonized fields are preserved untouched.
 */
clrdirty(fs)
char *fs;
{
	struct filsys sb;
	register int fd;

	if ((fd = open(fs, 2)) < 0)
		return;
	lseek(fd, (long)SUPERI*BSIZE, 0);
	if (read(fd, &sb, sizeof(sb)) == sizeof(sb) && sb.s_dirty != FSCLEAN) {
		sb.s_dirty = FSCLEAN;
		lseek(fd, (long)SUPERI*BSIZE, 0);
		write(fd, &sb, sizeof(sb));
	}
	close(fd);
}

/*
 * Do check for a single filesystem.
 */
check(fs)
char *fs;
{
	register int ierror, derror;
	register int bad = 0;

	if (ierror = run(icheck, fs, NULL))
		bad |= 1;
	if (derror = run(dcheck, fs, NULL))
		bad |= 1;
	if (sflag) {
		if ((ierror & ~IC_FIX) == 0) {
			if (derror & ~DC_FIX)
				return(-1);
			if (derror & DC_FIX) {
				/*
				 * A -s pass exits with the bits it just repaired
				 * (icheck/dcheck report what they fixed), so only
				 * a bit OUTSIDE the repairable mask means the fix
				 * actually failed.
				 */
				if (run(dcheck, fixopt, fs, NULL) & ~DC_FIX)
					return(-1);
				ierror = IC_MISS;	/*force fixup icheck*/
			}
			if (ierror & IC_FIX)
				if (run(icheck, fixopt, fs, NULL) & ~IC_FIX)
					return(-1);
		} else
			return(-1);
	}
	return (bad);
}

/*
 * Do a command -- either icheck or dcheck normally.
 */
/* VARARGS */
run(command, args)
char *command;
{
	register int pid;
	int status;

	if ((pid = fork()) < 0) {
		fprintf(stderr, "check: try again\n");
		exit(-2);
	}
	if (pid) {
		wait(&status);
		if ((status&0377) != 0)
			return(-2);
		return ((status>>8)&0377);
	} else {
		execv(command, &command);
		fprintf(stderr, "check: someone moved %s\n", command);
		exit(-2);
	}
	/* NOTREACHED */
}

usage()
{
	fprintf(stderr, "Usage: check [-s] filesystem [ ... ]\n");
	exit(-2);
}
