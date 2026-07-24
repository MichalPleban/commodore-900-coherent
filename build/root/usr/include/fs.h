/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * fs.h -- libfs: shared read/write access to a Coherent (V7-style) file
 * system image.  It centralises the pieces the file-system check utilities
 * (icheck, dcheck, ncheck) used to each carry a private copy of:
 *
 *	- block I/O over an open volume		fsbread / fsbwrite / fsbclear
 *	- the logical -> physical block map	fsimap
 *	- the bad-block / defect-run list	fsfinddefective / fssavedefective
 *	- the inode -> pathname engine		fsnames / fspath
 *
 * The last of these is the "inode-to-file" code ncheck historically owned;
 * putting it here lets icheck and dcheck report a file name next to each
 * i-number they complain about.
 *
 * NOTE: this is a new library, not part of the original Coherent source; the
 * three check tools are rebuilt together against it.
 */
#ifndef	_FS_H_
#define	_FS_H_

#include <filsys.h>
#include <ino.h>
#include <dir.h>

#define	FS_NHASH	101	/* directory-entry hash buckets (prime) */
#define	FS_NFNAME	400	/* longest pathname fspath() will build */
#define	FS_ESEEN	0200	/* cycle-guard bit borrowed in e_name[0] */

/*
 * One directory entry, hashed by child i-number.  fsnames() records every
 * name found in every directory ("." and ".." excepted); the chain lets an
 * i-number be walked back up to the root to build a pathname.
 */
struct	fsent {
	struct fsent	*e_next;	/* next in hash bucket */
	ino_t		e_pino;		/* parent i-number */
	ino_t		e_cino;		/* this entry's i-number */
	char		e_name[];	/* NUL-terminated leaf name */
};

/*
 * A run of consecutive defective blocks, from the bad-block file.  The list
 * is kept sorted so the i-list scanners can skip defective inode blocks.
 */
struct	defect {
	struct defect	*d_next;	/* link to next run */
	daddr_t		d_start;	/* first bad block in the run */
	int		d_length;	/* number of blocks in the run */
};

/*
 * An open file system.  A caller sets fsize/isize after it has read and
 * canonicalised the super block (fsopen leaves fsize = SUPERI+1 so the super
 * block itself can be read first); maxino is derived by fsnames().
 */
typedef	struct	FS {
	int		fd;		/* raw descriptor */
	int		writable;	/* opened for writing */
	daddr_t		fsize;		/* total blocks in the volume */
	unsigned short	isize;		/* first block past the i-list */
	ino_t		maxino;		/* highest valid i-number (fsnames) */
	int		*pexstat;	/* caller status word, OR'd on error (or 0) */
	int		hardbit;	/* bit to OR into *pexstat on I/O error */
	struct defect	*deflist;	/* bad-block runs */
	struct fsent	**entries;	/* [FS_NHASH] directory-entry hash */
	char		*dbmap;		/* directory-inode bitmap (fsnames) */
	char		*sbmap;		/* setuid/special bitmap (fsnames, opt.) */
	char		*dbuf;		/* private one-block scratch for imap */
	char		namebuf[FS_NFNAME];
}	FS;

FS	*fsopen();	/* (char *name, int writable, int *pexstat, int hardbit) */
void	fsclose();	/* (FS *) */
int	fsbread();	/* (FS *, daddr_t bn, char *buf)  -> 0 ok / -1 err */
int	fsbwrite();	/* (FS *, daddr_t bn, char *buf)  -> 0 ok / -1 err */
void	fsbclear();	/* (char *bp, unsigned nb) */
void	fsbcopy();	/* (char *in, char *out, unsigned nb) */
daddr_t	fsimap();	/* (FS *, struct dinode *ip, daddr_t lb) -> phys blk */
void	fssavedefective();	/* (FS *, daddr_t bn) */
void	fsfreedefective();	/* (FS *) */
int	fsfinddefective();	/* (FS *) reads BADFIN inode -> 0 ok / -1 err */
int	fsnames();	/* (FS *, int sbflag) build entries+dbmap -> 0 / -1 */
void	fsnfree();	/* (FS *) free the entries+dbmap+sbmap */
char	*fspath();	/* (FS *, ino_t) -> pathname (in fs->namebuf) */

#endif	/* _FS_H_ */
