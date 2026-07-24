/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Filesystem (disk inodes).
 */
#include <coherent.h>
#include <acct.h>
#include <buf.h>
#include <canon.h>
#include <drvcon.h>
#include <errno.h>
#include <filsys.h>
#include <ino.h>
#include <inode.h>
#include <io.h>
#include <mount.h>
#include <proc.h>
#include <uproc.h>

/*
 * Initialise filesystem.
 */
fsminit()
{
	register MOUNT *mp;

	if ((mp=fsmount(rootdev, ronflag)) == NULL)
		panic("fsminit: no rootdev");
	timer.t_time = mp->m_super.s_time;
	if ((u.u_rdir=iattach(rootdev, ROOTIN)) == NULL)
		panic("fsminit: no /");
	u.u_cdir = u.u_rdir;
	u.u_cdir->i_refc++;
	iunlock(u.u_rdir);
}

/*
 * Mount the given device.
 */
MOUNT *
fsmount(dev, f)
register dev_t dev;
{
	register MOUNT *mp;
	register BUF *bp;
	register int ro;

	if ((mp=kalloc(sizeof(MOUNT))) == NULL)
		return (NULL);
	ro = (f&MFRON) != 0;
	dopen(dev, (ro?IPR:IPR|IPW), DFBLK);
	if (u.u_error != 0) {
		kfree(mp);
		return (NULL);
	}
	if ((bp=bread(dev, (daddr_t)SUPERI, 1)) == NULL) {
		dclose(dev);
		kfree(mp);
		return (NULL);
	}
	kkcopy(bp->b_vaddr, &mp->m_super, sizeof(struct filsys));
	brelease(bp);
	cansuper(&mp->m_super);
	/*
	 * A file system that was not cleanly unmounted (s_dirty != FSCLEAN)
	 * is forced read only unless the caller forces it writable with
	 * MFFORCE.  This is how ronflag=0 yields a writable root only when
	 * the root file system is clean; a dirty root comes up read only for
	 * `check' to repair (ronflag=1 always forces read only via MFRON).
	 */
	if (ro==0 && mp->m_super.s_dirty!=FSCLEAN && (f&MFFORCE)==0)
		ro = 1;
	mp->m_ip = NULL;
	mp->m_dev = dev;
	mp->m_flag = ro ? MFRON : 0;
	mp->m_super.s_fmod = 0;
	mp->m_next = mountp;
	mountp = mp;
	/*
	 * Do not mark dirty here: mounting read/write does not by itself make
	 * the on-disk image inconsistent.  The first modification (smod()) is
	 * what persists FSDIRTY, and a sync writes FSCLEAN again.  So a freshly
	 * mounted, unmodified file system stays exactly as clean as it was.
	 */
	return (mp);
}

/*
 * Note that file system `mp' is about to be modified.  On the first
 * modification after a sync (s_fmod == 0) persist FSDIRTY to disk *before*
 * the change is written, so a crash mid-modification is detected as dirty.
 * Later modifications in the same sync interval are free.
 */
smod(mp)
register MOUNT *mp;
{
	register struct filsys *sbp;

	sbp = &mp->m_super;
	if (sbp->s_fmod == 0) {
		sbp->s_fmod = 1;
		sbp->s_dirty = FSDIRTY;
		superwrite(mp);
	}
}

/*
 * Write the in-core super block of `mp' out to disk synchronously.
 */
superwrite(mp)
register MOUNT *mp;
{
	register struct filsys *sbp;
	register BUF *bp;

	sbp = &mp->m_super;
	bp = bclaim(mp->m_dev, (daddr_t)SUPERI);
	kkcopy(sbp, bp->b_vaddr, sizeof(*sbp));
	cansuper(bp->b_vaddr);
	bwrite(bp, 1);
	brelease(bp);
}

/*
 * Remount the already-mounted file system `mp' read/write (mount -w).
 * Used after `check' has repaired a file system that came up read only.
 * `check' rewrote the raw device behind the buffer cache, so stale cached
 * blocks are flushed and in-core inodes re-read before the super block is
 * re-read and the file system made writable.  The file system must be clean
 * (FSCLEAN) unless the caller forces it with MFFORCE.
 */
fsremount(mp, f)
register MOUNT *mp;
{
	register BUF *bp;

	if ((mp->m_flag&MFRON) == 0)
		return;			/* already writable */
	/*
	 * Drop stale cached blocks first, so the super block and inodes are
	 * re-read from the repaired disk rather than from the cache.
	 */
	bflush(mp->m_dev);
	if ((bp=bread(mp->m_dev, (daddr_t)SUPERI, 1)) == NULL) {
		u.u_error = EIO;
		return;
	}
	kkcopy(bp->b_vaddr, &mp->m_super, sizeof(struct filsys));
	brelease(bp);
	cansuper(&mp->m_super);
	if (mp->m_super.s_dirty!=FSCLEAN && (f&MFFORCE)==0) {
		u.u_error = EROFS;	/* still dirty: run `check' first */
		return;
	}
	iremap(mp->m_dev);
	mp->m_flag &= ~MFRON;
	mp->m_super.s_fmod = 0;
	/*
	 * Leave s_dirty as read from disk (FSCLEAN, since `check' cleared it):
	 * the first modification after this remount marks it dirty via smod().
	 */
}

/*
 * Canonize a super block.
 */
cansuper(fsp)
register struct filsys *fsp;
{
	register int i;

	canint(fsp->s_isize);
	candaddr(fsp->s_fsize);
	canshort(fsp->s_nfree);
	for (i=0; i<NICFREE; i++)
		candaddr(fsp->s_free[i]);
	canshort(fsp->s_ninode);
	for (i=0; i<NICINOD; i++)
		canino(fsp->s_inode[i]);
	cantime(fsp->s_time);
	candaddr(fsp->s_tfree);
	canino(fsp->s_tinode);
	canshort(fsp->s_m);
	canshort(fsp->s_n);
	canlong(fsp->s_unique);
}

/*
 * Given a pointer to a mount entry, write out all inodes on that device.
 */
msync(mp)
register MOUNT *mp;
{
	register struct filsys *sbp;
	register BUF *bp;

	if ((mp->m_flag&MFRON) != 0)
		return;
	isync(mp->m_dev);
	sbp = &mp->m_super;
	if (sbp->s_fmod==0 && mp->m_dev!=rootdev)
		return;
	bp = bclaim(mp->m_dev, (daddr_t)SUPERI);
	sbp->s_time = timer.t_time;
	sbp->s_fmod = 0;
	sbp->s_dirty = FSCLEAN;		/* everything is now flushed: consistent */
	kkcopy(sbp, bp->b_vaddr, sizeof(*sbp));
	cansuper(bp->b_vaddr);
	bwrite(bp, 1);
	brelease(bp);
}

/*
 * Prepare for reboot: flush everything to disk -- which marks each modified
 * file system clean, since msync() now writes FSCLEAN -- and remount every
 * file system read only so nothing can dirty it again before the restart.
 * A file system that was genuinely idle+synced stays clean and comes back
 * read/write on the next boot; only a crash leaves one dirty.
 */
fsshutdown()
{
	register MOUNT *mp;

	usync();
	for (mp=mountp; mp!=NULL; mp=mp->m_next)
		mp->m_flag |= MFRON;
}

/*
 * Return the mount entry for the given device.  If `f' is not set
 * and the device is read only, don't set the error status.
 */
MOUNT *
getment(dev, f)
register dev_t dev;
{
	register MOUNT *mp;

	for (mp=mountp; mp!=NULL; mp=mp->m_next) {
		if (mp->m_dev != dev)
			continue;
		if ((mp->m_flag&MFRON) != 0) {
			if (f != 0)
				u.u_error = EROFS;
			return (NULL);
		}
		return (mp);
	}
	panic("getment: dev=0x%x", dev);
}

/*
 * Allocate a new inode with the given mode.  The returned inode is locked.
 */
INODE *
ialloc(dev, mode)
dev_t dev;
unsigned mode;
{
	register struct dinode *dip;
	register struct filsys *sbp;
	register ino_t *inop;
	register ino_t ino;
	register BUF *bp;
	register daddr_t b;
	register struct dinode *dipe;
	register ino_t *inope;
	register MOUNT *mp;
	register INODE *ip;

	if ((mp=getment(dev, 1)) == NULL)
		return (NULL);
	sbp = &mp->m_super;
	for (;;) {
		lock(mp->m_ilock);
		smod(mp);
		if (sbp->s_ninode == 0) {
			ino = 1;
			inop = sbp->s_inode;
			inope = &sbp->s_inode[NICINOD];
			for (b=INODEI; b<sbp->s_isize; b++) {
				if (bad(dev, b)) {
					ino += INOPB;
					continue;
				}
				if ((bp=bread(dev, b, 1)) == NULL) {
					ino += INOPB;
					continue;
				}
				dip = bp->b_vaddr;
				dipe = &dip[INOPB];
				for (; dip<dipe; dip++, ino++) {
					if (dip->di_mode != 0)
						continue;
					if (inop >= inope)
						break;
					*inop++ = ino;
				}
				brelease(bp);
				if (inop >= inope)
					break;
			}
			sbp->s_ninode = inop - sbp->s_inode;
			if (sbp->s_ninode == 0) {
				sbp->s_tinode = 0;
				unlock(mp->m_ilock);
				devmsg(dev, "Out of inodes");
				u.u_error = ENOSPC;
				return (NULL);
			}
		}
		ino = sbp->s_inode[--sbp->s_ninode];
		--sbp->s_tinode;
		unlock(mp->m_ilock);
		if ((ip=iattach(dev, ino)) != NULL) {
			if (ip->i_mode != 0) {
				devmsg(dev, "Inode %u busy", ino);
				idetach(ip);
				continue;
			}
			ip->i_flag = 0;
			ip->i_mode = mode;
			ip->i_nlink = 0;
			ip->i_uid = u.u_uid;
			ip->i_gid = u.u_gid;
		}
		return (ip);
	}
}

/*
 * Free the inode `ino' on device `dev'.
 */
ifree(dev, ino)
dev_t dev;
ino_t ino;
{
	register struct filsys *sbp;
	register MOUNT *mp;

	if ((mp=getment(dev, 1)) == NULL)
		return;
	lock(mp->m_ilock);
	sbp = &mp->m_super;
	smod(mp);
	if (sbp->s_ninode < NICINOD)
		sbp->s_inode[sbp->s_ninode++] = ino;
	sbp->s_tinode++;
	unlock(mp->m_ilock);
}

/*
 * Free all blocks in the indirect block `b' on the device `dev'.
 * `l' is the level of indirection.
 */
indfree(dev, b, l)
dev_t dev;
daddr_t b;
register unsigned l;
{
	register int i;
	register BUF *bp;
	daddr_t b1;

	if (b == 0)
		return;
	if (l-->0 && (bp=bread(dev, b, 1))!=NULL) {
		i = NBN;
		while (i-- > 0) {
			if ((b1=((daddr_t *)bp->b_vaddr)[i]) == 0)
				continue;
			candaddr(b1);
			if (l == 0)
				bfree(dev, b1);
			else
				indfree(dev, b1, l);
		}
		brelease(bp);
	}
	bfree(dev, b);
}

/*
 * Allocate a block from the filesystem mounted of device `dev'.
 */
daddr_t
balloc(dev)
dev_t dev;
{
	register struct filsys *sbp;
	register struct fblk *fbp;
	register daddr_t b;
	register BUF *bp;
	register MOUNT *mp;

	if ((mp=getment(dev, 1)) == NULL)
		return (0);
	lock(mp->m_flock);
	sbp = &mp->m_super;
	if (sbp->s_nfree == 0) {
enospc:
		sbp->s_nfree = 0;
		devmsg(dev, "Out of space");
		u.u_error = ENOSPC;
		b = 0;
	} else {
		smod(mp);
		if ((b=sbp->s_free[--sbp->s_nfree]) == 0)
			goto enospc;
		if (sbp->s_nfree == 0) {
			if (b >= sbp->s_fsize
			 || b < sbp->s_isize
			 || (bp = bread(dev, b, 1)) == NULL) {
ebadflist:
				devmsg(dev, "Bad free list");
				goto enospc;
			}
			fbp = bp->b_vaddr;
			sbp->s_nfree = fbp->df_nfree;
			canshort(sbp->s_nfree);
			if ((unsigned)sbp->s_nfree > NICFREE)
				goto ebadflist;
			kkcopy(fbp->df_free, sbp->s_free, sizeof(sbp->s_free));
			canndaddr(sbp->s_free, sbp->s_nfree);
			brelease(bp);
		}
		--sbp->s_tfree;
		if (b >= sbp->s_fsize || b < sbp->s_isize)
			goto ebadflist;
	}
	unlock(mp->m_flock);
	return (b);
}

/*
 * Free the block `b' on the device `dev'.
 */
bfree(dev, b)
dev_t dev;
daddr_t b;
{
	register struct filsys *sbp;
	register struct fblk *fbp;
	register BUF *bp;
	register MOUNT *mp;

	if ((mp=getment(dev, 1)) == NULL)
		return;
	sbp = &mp->m_super;
	if (b>=sbp->s_fsize || b<sbp->s_isize) {
		devmsg(dev, "Bad block %u (free)", (unsigned)b);
		return;
	}
	lock(mp->m_flock);
	if (sbp->s_nfree == 0 || sbp->s_nfree == NICFREE) {
		bp = bclaim(dev, b);
		fbp = bp->b_vaddr;
		kclear(fbp, BSIZE);
		fbp->df_nfree = sbp->s_nfree;
		canshort(fbp->df_nfree);
		kkcopy(sbp->s_free, fbp->df_free, sizeof(fbp->df_free));
		canndaddr(fbp->df_free, sbp->s_nfree);
		bp->b_flag |= BFMOD;
		brelease(bp);
		sbp->s_nfree = 0;
	}
	sbp->s_free[sbp->s_nfree++] = b;
	sbp->s_tfree++;
	smod(mp);
	unlock(mp->m_flock);
}

/*
 * Determine if the given block is bad.
 */
bad(dev, b)
dev_t dev;
daddr_t b;
{
	register INODE *ip;
	register BUF *bp;
	register int i;
	register int m;
	register int n;
	daddr_t l;

	if ((ip=iattach(dev, 1)) == NULL)
		panic("bad()");
	n = blockn(ip->i_size);
	if ((m=n) > ND)
		m = ND;
	for (i=0; i<m; i++) {
		--n;
		if (b == ip->i_a.i_addr[i]) {
			idetach(ip);
			return (1);
		}
	}
	l = ip->i_a.i_addr[ND];
	idetach(ip);
	if (n == 0)
		return (0);
	if ((bp=bread(dev, l, 1)) == NULL)
		return (0);
	if ((m=n) > NBN)
		m = NBN;
	for (i=0; i<m; i++) {
		l = ((daddr_t *)bp)[i];
		candaddr(l);
		if (b == l) {
			brelease(bp);
			return (1);
		}
	}
	brelease(bp);
	return (0);
}

/*
 * Canonize `n' disk addresses.
 */
canndaddr(dp, n)
register daddr_t *dp;
register int n;
{
	while (n--) {
		candaddr(*dp);
		dp++;
	}
}

/*
 * Write out an accounting record.
 */
setacct()
{
	register PROC *pp;
	struct acct acct;
	IO acctio;

	if (acctip == NULL)
		return;
	pp = SELF;
	kkcopy(u.u_comm, acct.ac_comm, 10);
	acct.ac_utime = ltoc(pp->p_utime);
	acct.ac_stime = ltoc(pp->p_stime);
	acct.ac_etime = ltoc(timer.t_time - u.u_btime);
	acct.ac_btime = u.u_btime;
	acct.ac_uid = u.u_uid;
	acct.ac_gid = u.u_gid;
	acct.ac_mem = 0;
	acct.ac_io = ltoc(u.u_block);
	acct.ac_tty = pp->p_ttdev;
	acct.ac_flag = u.u_flag;
	ilock(acctip);
	acctio.io_seek = acctip->i_size;
	acctio.io_ioc = sizeof (acct);
	acctio.io_base = &acct;
	acctio.io_seg = IOSYS;
	iwrite(acctip, &acctio);
	iunlock(acctip);
	u.u_error = 0;
}
