/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * libfs -- the inode -> pathname engine (the "inode-to-file" code).
 *
 * fsnames() scans the i-list and every directory and records, for each entry
 * (".", ".." excepted), a (child i-number -> parent i-number + leaf name)
 * mapping in a hash keyed on the child i-number.  It also builds a bitmap of
 * which i-numbers are directories (dbmap) and, optionally, of setuid/special
 * i-numbers (sbmap).  This is the machinery ncheck historically owned.
 *
 * fspath() then answers the question icheck and dcheck need: given an
 * i-number, walk it back up to the root through the hash and return its
 * pathname.  ncheck drives its own presentation layer directly off the hash
 * (fsp->entries) and dbmap.
 */
#include <stdio.h>
#include <fs.h>
#include <canon.h>

#define	IBLK	12		/* i-node read blocking factor */
#define	NBPC	8		/* bits per char (for the bitmaps) */

#define	mark(bm,i)	((bm)[(i)/NBPC] |= 1<<((i)%NBPC))

char	*malloc();

static	fspass1();
static	fspass2();
static	fsfinddirs();
static	fsdirenter();

/*
 * Build the directory-entry hash and the directory-inode bitmap.  If `sbflag'
 * is set, a setuid/special bitmap is built as well.  Returns 0 on success, -1
 * if memory could not be allocated.
 */
int
fsnames(fsp, sbflag)
register FS *fsp;
int sbflag;
{
	char *ibuf;
	unsigned nb;
	register int i;

	fsp->maxino = (fsp->isize - INODEI) * INOPB;
	nb = (fsp->maxino + NBPC) / NBPC;
	if ((fsp->dbmap = malloc(nb)) == NULL)
		return (-1);
	fsbclear(fsp->dbmap, nb);
	if (sbflag) {
		if ((fsp->sbmap = malloc(nb)) == NULL)
			return (-1);
		fsbclear(fsp->sbmap, nb);
	}
	if ((fsp->entries = (struct fsent **)
	    malloc(FS_NHASH * sizeof (struct fsent *))) == NULL)
		return (-1);
	for (i = 0; i < FS_NHASH; i++)
		fsp->entries[i] = NULL;
	if ((ibuf = malloc(IBLK * BSIZE)) == NULL)
		return (-1);
	fspass1(fsp, ibuf, sbflag);
	fspass2(fsp, ibuf);
	free(ibuf);
	return (0);
}

/*
 * Free everything fsnames() allocated.
 */
void
fsnfree(fsp)
register FS *fsp;
{
	register struct fsent *ep, **epp;

	if (fsp->entries != NULL) {
		for (epp = fsp->entries; epp < &fsp->entries[FS_NHASH]; epp++)
			for (ep = *epp; ep != NULL; ) {
				register struct fsent *next;

				next = ep->e_next;
				free((char *)ep);
				ep = next;
			}
		free((char *)fsp->entries);
		fsp->entries = NULL;
	}
	if (fsp->dbmap != NULL) {
		free(fsp->dbmap);
		fsp->dbmap = NULL;
	}
	if (fsp->sbmap != NULL) {
		free(fsp->sbmap);
		fsp->sbmap = NULL;
	}
}

/*
 * Pass one: mark every directory i-number in dbmap (and, if requested, every
 * setuid/special i-number in sbmap).  Pass two uses dbmap to know which inodes
 * to descend.
 */
static
fspass1(fsp, ibuf, sbflag)
register FS *fsp;
char *ibuf;
int sbflag;
{
	register struct dinode *ip;
	register ino_t inum;
	register int i;
	register unsigned imax;
	size_t seek;

	inum = 1;
	seek = INODEI * BSIZE;
	for (i = fsp->maxino; i > 0; i -= IBLK*INOPB) {
		lseek(fsp->fd, seek, 0);
		imax = i > IBLK*INOPB ? IBLK*INOPB : i;
		imax *= sizeof (struct dinode);
		seek += imax;
		if (read(fsp->fd, ibuf, imax) != imax) {
			if (fsp->pexstat != NULL)
				*fsp->pexstat |= fsp->hardbit;
			return;
		}
		for (ip = (struct dinode *)ibuf;
		    ip < (struct dinode *)&ibuf[imax]; ip++) {
			canshort(ip->di_mode);
			if ((ip->di_mode & IFMT) == IFDIR)
				mark(fsp->dbmap, inum);
			if (sbflag &&
			    (ip->di_mode & (ISUID|ISGID|IFBLK|IFCHR)))
				mark(fsp->sbmap, inum);
			inum++;
		}
	}
}

/*
 * Pass two: for every directory inode, record each of its entries in the hash.
 */
static
fspass2(fsp, ibuf)
register FS *fsp;
char *ibuf;
{
	register struct dinode *ip;
	register ino_t inum;
	register int i;
	register unsigned imax;
	size_t seek;

	inum = 1;
	seek = INODEI * BSIZE;
	for (i = fsp->maxino; i > 0; i -= IBLK*INOPB) {
		lseek(fsp->fd, seek, 0);
		imax = i > IBLK*INOPB ? IBLK*INOPB : i;
		imax *= sizeof (struct dinode);
		seek += imax;
		if (read(fsp->fd, ibuf, imax) != imax) {
			if (fsp->pexstat != NULL)
				*fsp->pexstat |= fsp->hardbit;
			return;
		}
		for (ip = (struct dinode *)ibuf;
		    ip < (struct dinode *)&ibuf[imax]; ip++) {
			canshort(ip->di_mode);
			cansize(ip->di_size);
			if ((ip->di_mode & IFMT) == IFDIR)
				fsfinddirs(fsp, ip, inum);
			inum++;
		}
	}
}

/*
 * Record every entry of one directory inode in the hash.  "." and ".." are
 * skipped; entries with an out-of-range i-number are ignored (the i-list
 * scanners report those separately).  Unlike ncheck's original finddirs --
 * which recorded only sub-directory entries -- every name is recorded, so a
 * plain file's i-number can be resolved to a path too.
 */
static
fsfinddirs(fsp, ip, inum)
register FS *fsp;
register struct dinode *ip;
register ino_t inum;
{
	size_t size;
	daddr_t pb, bn;

	size = ip->di_size;
	bn = 0;
	while (size >= sizeof (struct direct)) {
		register struct direct *dp;

		if ((pb = fsimap(fsp, ip, bn++)) == 0)
			break;
		fsbread(fsp, pb, fsp->dbuf);
		for (dp = (struct direct *)fsp->dbuf;
		    dp < (struct direct *)&fsp->dbuf[BSIZE]; dp++) {
			canino(dp->d_ino);
			if (dp->d_ino && dp->d_ino <= fsp->maxino)
				fsdirenter(fsp, dp, inum);
			size -= sizeof (struct direct);
			if (size == 0)
				break;
		}
	}
}

/*
 * Enter one directory entry (child i-number `dp->d_ino', parent `ino') into
 * the hash, keyed on the child i-number.
 */
static
fsdirenter(fsp, dp, ino)
register FS *fsp;
register struct direct *dp;
ino_t ino;
{
	register struct fsent *ep;
	register char *cp;
	register int n;

	cp = dp->d_name;
	if (*cp++ == '.')
		if ((*cp == '.' && cp[1] == '\0') || *cp == '\0')
			return;
	for (cp = dp->d_name; *cp != '\0'; cp++)
		if (cp >= &dp->d_name[DIRSIZ])
			break;
	n = cp - dp->d_name;
	/*
	 * Names are best effort: if we cannot record this entry, leave it out
	 * (its i-number will just resolve to "??") rather than flagging a hard
	 * file-system error, which running out of memory is not.
	 */
	if ((ep = (struct fsent *)
	    malloc(n + sizeof (char) + sizeof (struct fsent))) == NULL)
		return;
	ep->e_pino = ino;
	ep->e_cino = dp->d_ino;
	strncpy(ep->e_name, dp->d_name, n);
	ep->e_name[n] = '\0';
	n = ep->e_cino % FS_NHASH;
	ep->e_next = fsp->entries[n];
	fsp->entries[n] = ep;
}

/*
 * Build the pathname of i-number `ino' by walking it up to the root through
 * the hash, and return a pointer to it (in fsp->namebuf, valid until the next
 * fspath call).  A component whose name is not in the hash yields a "??"; a
 * (corrupt) parent cycle is broken by a depth limit.  Requires fsnames() to
 * have run first.
 */
char *
fspath(fsp, ino)
register FS *fsp;
ino_t ino;
{
	register char *np;
	register struct fsent *ep;
	register int depth;

	np = &fsp->namebuf[FS_NFNAME];
	*--np = '\0';
	if (fsp->entries == NULL) {
		*--np = '?';
		return (np);
	}
	if (ino == ROOTIN) {
		*--np = '/';
		return (np);
	}
	for (depth = 0; ino != ROOTIN && depth < FS_NHASH*64; depth++) {
		for (ep = fsp->entries[ino % FS_NHASH]; ep != NULL;
		    ep = ep->e_next)
			if (ep->e_cino == ino)
				break;
		if (ep == NULL) {
			if (np > &fsp->namebuf[2]) {
				*--np = '?';
				*--np = '?';
				*--np = '/';
			}
			return (np);
		}
		{
			register char *cp;

			cp = &ep->e_name[strlen(ep->e_name)];
			while (cp > ep->e_name && np > &fsp->namebuf[1])
				*--np = *--cp;
			if (np > &fsp->namebuf[0])
				*--np = '/';
		}
		ino = ep->e_pino;
	}
	return (np);
}
