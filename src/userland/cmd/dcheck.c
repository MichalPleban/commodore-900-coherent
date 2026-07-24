/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Rec'd from Lauren Weinstein, 7-16-84.
 * Dcheck - check consistency of directory
 * graph structure for a filesystem.
 * and optionally repair faulty link counts.
 */
#include <stdio.h>
#include <filsys.h>
#include <fblk.h>
#include <dir.h>
#include <sys/ino.h>
#include <check.h>
#include <canon.h>
#include <fs.h>		/* shared FS access + inode->pathname engine (libfs) */

#define	IBLK	12		/* I-node read blocking factor */
#define	NINUM	20		/* Maximum number of i-numbers to look for */
#define	INOORG	2

/*
 * struct defect (the bad-block run list), the imap block map and the
 * ranges/offsets/coeff access-polynomial tables now live in libfs; see
 * fsimap()/fsfinddefective() in <fs.h>.
 */

char	tmi[] = "Too many i-numbers given\n";
char	irderr[] = "I-node read error -- pass %d\n";

int	ninumber;
ino_t	inums[NINUM];
char	superb[BSIZE];
char	ibuf[BSIZE*IBLK];
char	dbuf[BSIZE];

int	sflag;			/* Repair filesystem */
int	exstat;			/* Exit status */
FS	*fsp;			/* Open file system (libfs) */
daddr_t	fsize	= SUPERI+1;	/* Allow read of super-block */
ino_t	isize;
ino_t	maxino;
unsigned nhard;			/* Hard things requiring pass 3 to fix */
short	unsigned *entries;	/* Per-inode reference (link) count */

int	imark();
int	icompare();

main(argc, argv)
char *argv[];
{

	while (argc>1 && *argv[1]=='-') {
		switch (argv[1][1]) {
		case 'i':
			ninumber = 0;
			while(inums[ninumber] = atoi(argv[2])) {
				if (ninumber++ >= NINUM) {
					fprintf(stderr, tmi);
					exstat |= DC_MISC;
					break;
				}
				argv++;
				argc--;
			}
			break;

		case 's':
			sflag = 1;
			break;

		default:
			usage();
		}
		argc--;
		argv++;
	}
	if (argc > 1)
		allcheck(argv+1);
	else
		usage();
	exit(exstat);
}

/*
 * Check the given list of filesystems
 */
allcheck(fsl)
register char **fsl;
{
	while (*fsl != NULL)
		dcheck(*fsl++);
}

/*
 * Check one filesystem
 */
dcheck(fsname)
char *fsname;
{
	register	i;
	struct filsys *sbp;

	if ((fsp = fsopen(fsname, sflag, &exstat, DC_HARD)) == NULL) {
		fprintf(stderr, "%s: cannot open\n", fsname);
		exstat |= DC_MISC;
		return;
	}
	printf("%s:\n", fsname);
	if (!sflag)
		sync();
	bread((daddr_t)SUPERI, superb);
	sbp = superb;
	canshort( sbp->s_isize);
	candaddr( sbp->s_fsize);
	canshort( sbp->s_nfree);
	for (i=0; i<NICFREE; ++i)
		candaddr( sbp->s_free[i]);
	canshort( sbp->s_ninode);
	for (i=0; i<NICINOD; ++i)
		canino( sbp->s_inode[i]);
	cantime( sbp->s_time);
	candaddr( sbp->s_tfree);
	canino( sbp->s_tinode);
	canshort( sbp->s_m);
	canshort( sbp->s_n);
	fsize = sbp->s_fsize;
	isize = sbp->s_isize;
	if (isize<INODEI+1 || isize>=fsize)
		cerr("Ridiculous fsize/isize");
	fsp->fsize = fsize;
	fsp->isize = isize;
	/*
	 * Build the inode->pathname map so a bad link count or a bad/argument
	 * directory can be reported by name as well as by i-number.  Failure
	 * is non-fatal -- names then come back as "??".
	 */
	if (fsnames(fsp, 0) < 0)
		fprintf(stderr, "dcheck: no space for names -- i-numbers only\n");
	if ((entries=calloc(isize*INOPB, sizeof(short unsigned))) == NULL)
		cerr("Not enough space");
	fsfinddefective(fsp);
	maxino = (isize-INODEI) * INOPB;
	/*
	 * The first pass runs down the
	 * graph filling in the array
	 * `entries' which is the number
	 * of names found in directories for
	 * any i-node.
	 */
	entries[ROOTIN-1]++;
	pass(0, imark);
	/*
	 * In the next pass, link counts
	 * in the i-nodes are compared with
	 * those pre-computed for the graph.
	 */
	pass(1, icompare);
	/*
	 * This fixup pass is only
	 * required for some harder errors
	 * encountered in `-s' mode.
	 */
	if (nhard && sflag)
		pass(2, imark);
	free(entries);
	fsfreedefective(fsp);
	nhard = 0;
	fsclose(fsp);
	fsp = NULL;
}

/*
 * A generalised pass over all i-nodes, calls
 * the routine `func' for every i-node encountered.
 * `n' is the pass number, used only in the diagnostics.
 */
pass(n, func)
int n;
int (*func)();
{
	register struct dinode *ip;
	register ino_t inum;
	daddr_t seek, limit;
	int thischunk;
	struct defect *cdsp;

	inum = 1;
	seek = INOORG;
	cdsp = fsp->deflist;
	while (seek < isize) {
		if (cdsp!=NULL && cdsp->d_start==seek) {
			seek  += cdsp->d_length;
			inum  += cdsp->d_length*INOPB;
			cdsp   = cdsp->d_next;
			continue;
		}
		limit = seek+IBLK;
		if (cdsp!=NULL && limit>cdsp->d_start)
			limit = cdsp->d_start;
		if (limit > isize)
			limit = isize;
		thischunk = limit-seek;
		lseek(fsp->fd, seek*BSIZE, 0);
		seek += thischunk;
		thischunk *= BSIZE;
		if (read(fsp->fd, ibuf, thischunk) != thischunk) {
			fprintf(stderr, irderr, n);
			exstat |= DC_HARD;
			break;
		}
		ip = (struct dinode *) &ibuf[0];
		while (ip < (struct dinode *) &ibuf[thischunk]) {
			if (inum != BADFIN) {
				canshort(ip->di_mode);
				canshort(ip->di_nlink);
				canshort(ip->di_uid);
				canshort(ip->di_gid);
				cansize(ip->di_size);
				cantime(ip->di_atime);
				cantime(ip->di_mtime);
				cantime(ip->di_ctime);
				if ((*func)(ip, inum, n))
					return;
			}
			++inum;
			++ip;
		}
	}
}

/*
 * Check an i-node link count (in
 * pass 2) against the entries already
 * found.
 */
icompare(ip, inum, pn)
register struct dinode *ip;
register ino_t inum;
int pn;
{
	register unsigned nent;

	nent = entries[inum-1];
	entries[inum-1] = 0;
	if (nent != ip->di_nlink
	|| (ip->di_mode!=0 && ip->di_nlink==0))
		badnlink(ip, inum, nent);
	return (0);
}

/*
 * Report or fix up bad link count
 * in filesystem.
 * `entries' is the number found.
 */
badnlink(ip, ino, nent)
register struct dinode *ip;
ino_t ino;
int nent;
{
	static int needtitle = 1;

	if (sflag == 0) {
		if (needtitle != 0) {
			printf(" Ino  Entries   Link  Name\n");
			needtitle = 0;
		}
		printf("%4u  %7u %6u", ino, nent, ip->di_nlink);
		if (ip->di_mode!=0 && ip->di_nlink==0)
			printf(" (u)");
		printf("  %s", fspath(fsp, ino));
		putchar('\n');
	}
	if (nent == 0) {
		if (sflag) {
			bclear((char *) ip, sizeof (*ip));
			iwrite(ip, ino);
		} else
			exstat |= DC_CLRI;
	} else if (ip->di_mode != 0) {
		if (sflag) {
			ip->di_nlink = nent;
			iwrite(ip, ino);
		} else
			exstat |= DC_LCE;
	} else if (ip->di_mode==0 && ip->di_nlink==0) {
		nhard++;
		entries[ino-1] = nent;
	}
}

/*
 * Imark looks at all directory i-nodes
 * and marks all of the subordinate nodes
 * in the entries table.  It also checks for
 * argument i-numbers to list specially.
 * Returns non-zero if we should stop
 * i-list scanning in `pass'.
 */
imark(ip, inum, pn)
register struct dinode *ip;
register ino_t inum;
int pn;
{
	size_t size;
	daddr_t pb;
	register daddr_t bn;

	if (ip->di_mode == 0)
		return (0);
	if ((ip->di_mode&IFMT) != IFDIR)
		return (0);
	size = ip->di_size;
	bn = 0;
	if (pn==1 && (size % sizeof(struct direct))!=0) {
		printf("I#%u: Directory size not mod %d\n", inum,
		    sizeof(struct direct));
		size -= size % sizeof( struct direct);
	}
	while (size) {
		register struct direct *dp;

		if ((pb = fsimap(fsp, ip, bn++)) == 0)
			break;
		bread(pb, dbuf);
		for (dp=dbuf; dp < &dbuf[BSIZE]; dp++) {
			canino( dp->d_ino);
			if (dp->d_ino) {
				if (dp->d_ino > maxino)
					dirline(inum, dp, "bad");
				else if (pn == 0)
					entries[dp->d_ino-1]++;
				else {
					if (entries[dp->d_ino-1]) {
						if (--entries[dp->d_ino-1] == 0)
							nhard--;
						bclear(dp, sizeof(*dp));
						bwrite(pb, dbuf);
					}
				}
				if (ninumber)
					iarg(inum, dp);
			}
			size -= sizeof( struct direct);
			if (size == 0)
				break;
		}
	}
	if (pn==0)
		return (0);
	return (nhard == 0);
}

/*
 * Iarg checks if the directory i-number is in
 * the argument list of i-nodes, and if it is
 * prints this out.
 */
iarg(inum, dp)
register ino_t inum;
register struct direct *dp;
{
	register unsigned i;

	for (i=0; i<ninumber;)
		if (inums[i++] == dp->d_ino)
			dirline(inum, dp, "arg");
}

/*
 * Print out a line for a directory
 * that is found in the search (.e.g.
 * bad or argument directories).
 */
dirline(ino, dp, str)
ino_t ino;
register struct direct *dp;
char *str;
{
	printf("%u %s: %u/%-*.*s in %s\n", dp->d_ino, str, ino,
	    DIRSIZ, DIRSIZ, dp->d_name, fspath(fsp, ino));
}

/*
 * The defective-block list (finddefective/savedefective/freedefective), the
 * imap block map and the block I/O (bread/bwrite) now live in libfs; see
 * fsfinddefective()/fsimap()/fsbread() in <fs.h>.  bread/bwrite remain as thin
 * wrappers so the many call sites in this file are unchanged.
 */
bread(bn, buf)
daddr_t bn;
char *buf;
{
	fsbread(fsp, bn, buf);
}

bwrite(bn, buf)
daddr_t bn;
char *buf;
{
	fsbwrite(fsp, bn, buf);
}

/*
 * Put out an i-node to disc.
 * Used only for `-s' option.
 */
iwrite(ip, ino)
register struct dinode *ip;
register ino_t ino;
{
	register struct dinode	*ip2;
	daddr_t		bn;

	bn = iblockn(ino);
	bread(bn, dbuf);
	ip2 = &((struct dinode *)dbuf)[iblocko( ino)];
	*ip2 = *ip;
	canshort( ip2->di_mode);
	canshort( ip2->di_nlink);
	canshort( ip2->di_uid);
	canshort( ip2->di_gid);
	cansize( ip2->di_size);
	cantime( ip2->di_atime);
	cantime( ip2->di_mtime);
	cantime( ip2->di_ctime);
	bwrite(bn, dbuf);
}

/*
 * bclear/bcopy now live in libfs (fsbclear/fsbcopy); kept as thin wrappers so
 * this file's call sites are unchanged.
 */
bclear(bp, nb)
char *bp;
unsigned nb;
{
	fsbclear(bp, nb);
}

bcopy(in, out, nb)
char *in, *out;
unsigned nb;
{
	fsbcopy(in, out, nb);
}

/*
 * Unrecoverable errors
 */
cerr(x)
{
	printf("%r", &x);
	putchar('\n');
	exit(DC_MISC);
}

usage()
{
	cerr("Usage: dcheck [-s] [-i ino ...] filesystem ...");
}
