/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * ncheck -- map an I-number into a pathname.
 * Also, look for special and setuid files.
 *
 * The directory-graph scan and the i-number -> pathname hash now live in
 * libfs (fsnames/fspath and the shared block I/O + imap); ncheck is the thin
 * presentation layer that drives them, applying the -a/-s/-i filters and
 * formatting the output.
 */

#include <stdio.h>
#include <filsys.h>
#include <fblk.h>
#include <dir.h>
#include <ino.h>
#include <canon.h>
#include <fs.h>

#define	NFNAME	400		/* Longest filename generated */
#define	IBLK	12		/* I-node read blocking factor */
#define	NINUM	20		/* Maximum number of i-numbers to look for */
#define	NHASH	FS_NHASH
#define	ESEEN	FS_ESEEN

/* Functions to test for directory or setuid/special i-numbers */
#define	test(bm,i)	(bm[(i)/8] & 1<<((i)%8))

/*
 * Default filesystem names
 * to check.
 */
char	*defnames[] = {
	"/dev/rrm00",
	NULL
};

char	tmi[] = "ncheck: too many i-numbers given\n";

int	ninumber;
ino_t	inums[NINUM];
char	superb[BSIZE];
char	ibuf[BSIZE*IBLK];	/* i-list read buffer for pass3 */
char	dbuf[BSIZE];		/* directory block buffer for printdir */
char	namebuf[NFNAME];

int	aflag;			/* All (print "." and ".." names) flag */
int	sflag;			/* Special and setuid files */
int	uflag;			/* Print unreferenced structure */
int	exstat;			/* Exit status */

FS	*fsp;			/* Open file system (libfs) */
struct	fsent **entries;	/* Directory-entry hash (from fsp) */
char	*dbmap;			/* Directory i-node bit-map (from fsp) */
char	*sbmap;			/* Special + setuid i-node bitmap (from fsp) */
ino_t	maxino;

main(argc, argv)
char *argv[];
{

	while (argc>1 && *argv[1]=='-') {
		switch (argv[1][1]) {
		case 'a':
			aflag = 1;
			break;

		case 'i':
			for (;;) {
				if (ninumber >= NINUM) {
					fprintf(stderr, tmi);
					exstat = 1;
					break;
				}
				if ((inums[ninumber] = atoi(argv[2])) == 0)
					break;
				argv++;
				argc--;
				ninumber++;
			}
			break;

		case 's':
			sflag = 1;
			break;

		case 'u':	/* Unimplemented search for orphan structure */
			uflag = 1;
			break;

		default:
			usage();
		}
		argc--;
		argv++;
	}
	if (argc > 1)
		allcheck(argv+1); else
		allcheck(defnames);
	exit(exstat);
}

/*
 * Check the given list of filesystems
 */
allcheck(fsl)
register char **fsl;
{
	while (*fsl != NULL)
		ncheck(*fsl++);
}

/*
 * Do `ncheck' for each filesystem.
 */
ncheck(fsname)
char *fsname;
{
	register struct filsys *sbp;

	if ((fsp = fsopen(fsname, 0, &exstat, 1)) == NULL) {
		fprintf(stderr, "%s: cannot open\n", fsname);
		exstat = 1;
		return;
	}
	printf( "%s:\n", fsname);
	sync();
	fsbread(fsp, (daddr_t)SUPERI, superb);
	sbp = (struct filsys *)superb;

	canshort(sbp->s_isize);
	candaddr(sbp->s_fsize);

	fsp->fsize = sbp->s_fsize;
	fsp->isize = sbp->s_isize;
	if (fsp->isize<INODEI+1 || fsp->isize>=fsp->fsize)
		cerr("Ridiculous fsize/isize");
	if (fsnames(fsp, sflag) < 0)
		cerr("Out of memory for directory structure");
	entries = fsp->entries;
	dbmap = fsp->dbmap;
	sbmap = fsp->sbmap;
	maxino = fsp->maxino;
	pass3();
	fsclose(fsp);
	fsp = NULL;
}

/*
 * Pass 3 uses the hashed table produced by fsnames()
 * to generate the output information that was
 * requested by the command line.
 */
pass3()
{
	register struct dinode *ip;
	register ino_t inum;
	register int i;
	register unsigned imax;
	size_t seek;

	inum = 1;
	seek = INODEI*BSIZE;
	if (!sflag && (ninumber==0 || iarg(ROOTIN)))
		printf( "%u\t/.\n", ROOTIN);
	for (i=maxino; i>0; i -= IBLK*INOPB) {
		lseek(fsp->fd, seek, 0);
		imax = i>IBLK*INOPB ? IBLK*INOPB : i;
		imax *= sizeof (struct dinode);
		seek += imax;
		if (read(fsp->fd, ibuf, imax) != imax) {
			fprintf(stderr, "ncheck: inode read error -- pass 3\n");
			exstat = 1;
			return;
		}
		for (ip = (struct dinode *)ibuf;
		    ip < (struct dinode *)&ibuf[imax]; ip++) {
			canshort(ip->di_mode);
			canshort(ip->di_nlink);
			cansize(ip->di_size);
			if ((ip->di_mode & IFMT) == IFDIR)
				printdir(ip, inum);
			inum++;
		}
	}
}

/*
 * Print all of the names found
 * in this directory.
 */
printdir(ip, ino)
register struct dinode *ip;
register ino_t ino;
{
	size_t size;
	daddr_t pb, bn;

	size = ip->di_size;
	bn = 0;
	while (size >= sizeof(struct direct)) {
		register struct direct *dp;

		if ((pb = fsimap(fsp, ip, bn++)) == 0)
			break;
		fsbread(fsp, pb, dbuf);
		for (dp=(struct direct *)dbuf;
		    dp < (struct direct *)&dbuf[BSIZE]; dp++) {
			canino( dp->d_ino);
			if (dp->d_ino) {
				if (dp->d_ino > maxino)
					continue;
				if (sflag && !test(sbmap, dp->d_ino))
					continue;
				if (ninumber!=0 && !iarg(dp->d_ino))
					continue;
				outname(dp, ino);
			}
			size -= sizeof( struct direct);
			if (size == 0)
				break;
		}
	}
}

/*
 * Print out the actual name by
 * traversing the structures
 * for a directory entry.
 */
outname(dp, ino)
register struct direct *dp;
ino_t ino;
{
	register char *np;

	np = dp->d_name;
	if (!aflag && *np++=='.')
		if ((*np=='.' && np[1]=='\0') || *np=='\0')
			return;
	np = &namebuf[NFNAME];
	*--np = '\0';
	if (!aflag && test(dbmap, dp->d_ino)) {
		*--np = '.';
		*--np = '/';
	}
	{
		register char *cp;

		for (cp = dp->d_name; cp < &dp->d_name[DIRSIZ]; cp++)
			if (*cp == '\0')
				break;
		while (cp > dp->d_name)
			*--np = *--cp;
		*--np = '/';
	}
	outpart(np, ino, dp->d_ino);
}

/*
 * Put out each name part.
 * Either get to the root
 * or find no parent.
 * `np' is the pointer running
 * backwards in the namebuf.
 */
outpart(np, ino, oino)
register char *np;
ino_t ino;
ino_t oino;
{
	register struct fsent *ep;
	register char *cp;
	register int found = 0;
	register char *snp;

	if (ino != ROOTIN) {
		for (ep = entries[ino%NHASH]; ep != NULL; ep = ep->e_next)
			if (ep->e_cino == ino) {
				if (ep->e_name[0] & ESEEN) {
					*--np = '.';
					*--np = '.';
					*--np = '.';
					return;
				}
				snp = np;
				cp = &ep->e_name[strlen(ep->e_name)];
				while (cp > ep->e_name)
					*--np = *--cp;
				*--np = '/';
				ep->e_name[0] |= ESEEN;
				found = 1;
				if (np > namebuf+DIRSIZ)
					outpart(np, ep->e_pino, oino);
				ep->e_name[0] &= ~ESEEN;
				np = snp;
			}
		if (!found) {
			*--np = '?';
			*--np = '?';
		}
	}
	if (!found || ino==ROOTIN)
		printf( "%u\t%s\n", oino, np);
}

/*
 * Return true if the argument
 * i-node is the one of the `-i'
 * arguments.
 */
iarg(ino)
register ino_t ino;
{
	register int i;

	for (i=0; i<ninumber; i++)
		if (inums[i] == ino)
			return (1);
	return (0);
}

/*
 * Unrecoverable errors
 */
cerr(x)
{
	fprintf(stderr, "ncheck: %r\n", &x);
	exit(1);
}

usage()
{
	fprintf(stderr, "Usage: ncheck [-a] [-s] [-i ino ...] [filesystem ...]");
}
