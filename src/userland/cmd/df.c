/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * df: print out information regarding the
 * remaining space available on a file system.
 * This command also considers a directory
 * to represent the filesystem.
 */

#include <stdio.h>
#include <filsys.h>
#include <stat.h>
#include <dir.h>
#include <canon.h>
#include <mnttab.h>
#if 1
#include <mtab.h>
#endif

#define	NMOUNT	64			/* Maximum mounted file systems */

int	aflag;				/* Print for each mounted fs */
int	iflag;				/* Print information on i-nodes */
int	tflag;				/* Print total device size */

char	buf[BSIZE];			/* Basic file system reading buffer */
struct	mnttab	mtab[NMOUNT];
struct	mnttab	*emtabp;

char	*devname();

main(argc, argv)
int argc;
char *argv[];
{
	register char *ap;
	register int i;
	register int estat = 0;

	while (argc>1 && *argv[1]=='-') {
		for (ap = &argv[1][1]; *ap != '\0'; ap++)
			switch (*ap) {
			case 'a':
				aflag++;
				break;

			case 'i':
				iflag++;
				break;

			case 't':
				tflag++;
				break;

			default:
				usage();
			}
		argc--;
		argv++;
	}
	minit();
	sync();
	if (argc < 2) {
		if (aflag)
			estat = dfmtab();
		else
			estat = df(".");
	} else {
		for (i=1; i<argc; i++)
			estat |= df(argv[i]);
		if (aflag)
			estat |= dfmtab();
	}
	exit(estat);
}

/*
 * Read the mount table, looking for file systems
 * to run df on.
 */
dfmtab()
{
	register struct mnttab *mp;
	int estat;
	int name[MNAMSIZ+10];

	estat = 0;
	for (mp = mtab; mp < emtabp; mp++) {
		if (mp->mt_dev[0]=='\0' || mp->mt_filsys[0]=='\0')
			continue;
		sprintf(name, "/dev/%s", mp->mt_filsys);
		estat |= df(name);
	}
	return (estat);
}

/*
 * Look at the file system
 * and find out free space.
 */
df(fs)
register char *fs;
{
	register struct filsys *sbp;
	struct stat sb;
	int fd;
	long	total;
	long	free;

	if (stat(fs, &sb) < 0) {
		cmsg("cannot stat '%s'", fs);
		return (1);
	}
	switch (sb.st_mode & S_IFMT) {
	case S_IFDIR:
	{
		char *nfs;

		if ((nfs = devname(sb.st_dev)) == NULL) {
			cmsg("no file system device found for directory '%s'",
			    fs);
			return (1);
		}
		fs = nfs;
		break;
	}

	case S_IFBLK:
	case S_IFCHR:
		break;

	default:
		cmsg("unknown file type '%s'", fs);
		return (1);
	}
	if ((fd = open(fs, 0)) < 0) {
		cmsg("cannot open '%s'", fs);
		return (1);
	}
	lseek(fd, (long)BSIZE * SUPERI, 0);
	if (read(fd, buf, sizeof(struct filsys)) != sizeof(struct filsys)) {
		cmsg("read error on '%s'", fs);
		close(fd);
		return (1);
	}
	close(fd);
	sbp = &buf[0];
	canf(sbp);
	if (tstf(sbp) == 0) {
		cmsg("badly formed super block on '%s'", fs);
		return (1);
	}
	printf("%-11s", fs);
	free = sbp->s_tfree;
	total = sbp->s_fsize - sbp->s_isize;
	report(free, total);
	if (iflag) {
		printf(", ");
		total = (sbp->s_isize-INODEI)*INOPB;
		free = sbp->s_tinode;
		report(free, total);
	}
	if (tflag)
		printf(", %U", sbp->s_fsize);
	printf("\n");
	return (0);
}

report(free, total)
long free;
long total;
{
	long percent;

	printf("%6U/%6U = ", free, total);
	percent = (free * 1000L) / total;
	printf("%2D.%1D%%", percent/10L, percent%10L);
}

/*
 * Initialise mount table
 * in memory.
 */
minit()
{
	register int fd;
	register int n;

	emtabp = &mtab[0];
	if ((fd = open("/etc/mnttab", 0)) >= 0) {
		if ((n = read(fd, (char *)&mtab[0], sizeof mtab)) > 0)
			emtabp = (char *)(&mtab[0]) + n;
		close(fd);
		return;
	}
#if 1
	if ((fd = open("/etc/mtab", 0)) >= 0) {
		register struct mtab *mp;

		while (read(fd, (char *)emtabp, sizeof(*mp)) == sizeof(*mp))
			emtabp++;
		close(fd);
	}
#endif
}

/*
 * Return the name of the block special file
 * (in directory '/dev') which corresponds to
 * the device given in argument.
 */
char *
devname(dev)
dev_t dev;
{
	register struct direct *dp;
	register int n;
	int fd;
	static char name[25];
	struct stat sb;

	if ((fd = open("/dev", 0)) < 0)
		return (NULL);
	while ((n = read(fd, buf, sizeof buf)) > 0) {
		for (dp = &buf[0]; dp < &buf[n]; dp++) {
			canino(dp->d_ino);
			if (dp->d_ino == 0)
				continue;
			strcpy(name, "/dev/");
			strncat(name, dp->d_name, DIRSIZ);
			if (stat(name, &sb) < 0)
				continue;
			if ((sb.st_mode & S_IFMT) != S_IFBLK)
				continue;
			if (sb.st_rdev != dev)
				continue;
			close(fd);
			return (name);
		}
	}
	close(fd);
	return (NULL);
}

/*
 * Canonicalize and check superblock for consistency.
 */
canf(fp)
register struct filsys *fp;
{
	register daddr_t *dp;
	register ino_t *ip;

	canshort(fp->s_isize);
	candaddr(fp->s_fsize);
	canshort(fp->s_nfree);
	for (dp = &fp->s_free[0]; dp < &fp->s_free[NICFREE]; dp += 1)
		candaddr(*dp);
	canshort(fp->s_ninode);
	for (ip = &fp->s_inode[0]; ip < &fp->s_inode[NICINOD]; ip += 1)
		canino(*ip);
	candaddr(fp->s_tfree);
	canino(fp->s_tinode);
}

tstf(fp)
register struct filsys *fp;
{
	register daddr_t *dp;
	register ino_t *ip;
	register ino_t maxinode;

	maxinode = (fp->s_isize - INODEI) * INOPB + 1;
	if (fp->s_isize >= fp->s_fsize)
		return (0);
	if (fp->s_tfree < fp->s_nfree
	 || fp->s_tfree >= fp->s_fsize - fp->s_isize + 1)
		return (0);
	if (fp->s_tinode < fp->s_ninode
	 || fp->s_tinode >= maxinode-1)
		return (0);
	for (dp = &fp->s_free[0]; dp < &fp->s_free[fp->s_nfree]; dp += 1)
		if (*dp < fp->s_isize || *dp >= fp->s_fsize)
			return (0);
	for (ip = &fp->s_inode[0]; ip < &fp->s_inode[fp->s_ninode]; ip += 1)
		if (*ip < 1 || *ip > maxinode)
			return (0);
	return (1);
}


/*
 * Errors and warnings.
 */
/* VARARGS */
cerr(arg)
char *arg;
{
	fprintf(stderr, "df: %r\n", &arg);
	exit(1);
}

/* VARARGS */
cmsg(arg)
char *arg;
{
	fprintf(stderr, "df: %r\n", &arg);
}

usage()
{
	fprintf(stderr, "Usage: df [-ait] [directory ...] [filesystem ...]\n");
	exit(1);
}
