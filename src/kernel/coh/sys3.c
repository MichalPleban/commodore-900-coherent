/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * System calls (more filesystem related calls).
 */
#include <coherent.h>
#include <buf.h>
#include <errno.h>
#include <fd.h>
#include <filsys.h>
#include <ino.h>
#include <inode.h>
#include <io.h>
#include <mount.h>
#include <stat.h>
#include <uproc.h>

/*
 * Gate serialising `usync' and `uumount' against each other, so that a sync
 * never walks the mount table while an unmount is unlinking an entry from it
 * and freeing it.
 */
static GATE syngate;

/*
 * Open the file `np' with the mode `mode'.
 */
uopen(np, mode)
char *np;
{
	register int f;
	register INODE *ip;
	register int fd;

	switch (mode) {
	case 0:
		f = IPR;
		break;
	case 1:
		f = IPW;
		break;
	case 2:
		f = IPR|IPW;
		break;
	default:
		u.u_error = EINVAL;
		return;
	}
	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (iaccess(ip, f) == 0) {
		idetach(ip);
		return;
	}
	if ((fd=fdopen(ip, f)) < 0) {
		idetach(ip);
		return;
	}
	iunlock(ip);
	return (fd);
}

/*
 * Create a pipe.
 */
upipe(fdp)
int fdp[2];
{
	register INODE *ip;
	register int fd1;
	register int fd2;

	if ((ip=pmake(0)) == NULL)
		return;
	if ((fd1=fdopen(ip, IPR)) >= 0) {
		ip->i_refc++;
		if ((fd2=fdopen(ip, IPW)) >= 0) {
			putuwd(&fdp[0], fd1);
			putuwd(&fdp[1], fd2);
			iunlock(ip);
			return (0);
		}
		--ip->i_refc;
		iunlock(ip);
		fdclose(fd1);
		return (0);
	}
	idetach(ip);
	return (0);
}

/*
 * Read `n' bytes into the buffer `bp' from file number `fd'.
 */
uread(fd, bp, n)
char *bp;
unsigned n;
{
	return (sysio(fd, bp, n, 0));
}

/*
 * Read or write `n' bytes from the file number `fd' using the buffer
 * `bp'.  If `f' is 0, we read, else write.
 */
sysio(fd, bp, n, f)
char *bp;
unsigned n;
{
	register FD *fdp;
	register INODE *ip;
	register int type;

	if ((fdp=fdget(fd)) == NULL)
		return (0);
	if ((fdp->f_flag&(f?IPW:IPR)) == 0) {
		u.u_error = EBADF;
		return (0);
	}
	ip = fdp->f_ip;
	type = ip->i_mode&IFMT;
	u.u_io.io_seek = fdp->f_seek;
	u.u_io.io_base = bp;
	u.u_io.io_ioc = n;
	if (type != IFCHR)
		ilock(ip);
	if (f == 0) {
		iread(ip, &u.u_io);
		iacc(ip);		/* read - atime */
	} else {
		iwrite(ip, &u.u_io);
	}
	if (type != IFCHR)
		iunlock(ip);
	n -= u.u_io.io_ioc;
	fdp->f_seek += n;
	return (n);
}

/*
 * Return a status structure for the given file name.
 */
ustat(np, stp)
char *np;
struct stat *stp;
{
	register INODE *ip;
	struct stat stat;

	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	istat(ip, &stat);
	idetach(ip);
	kucopy(&stat, stp, sizeof(stat));
	return (0);
}

/*
 * Write out all modified buffers, inodes and super blocks to disk.
 */
usync()
{
	register MOUNT *mp;

	lock(syngate);
	/*
	 * Order matters.  Flush the inodes into the buffer cache first, then
	 * push every dirty buffer out, and only then write the super blocks
	 * marked clean.  `msync' used to write a FSCLEAN super block while
	 * the inodes and free list blocks it describes were still cache
	 * only, so a crash in that window left a file system that claimed
	 * to be consistent and was not -- exactly what the flag exists to
	 * prevent.
	 */
	for (mp=mountp; mp!=NULL; mp=mp->m_next)
		if ((mp->m_flag&MFRON) == 0)
			isync(mp->m_dev);
	bsync();
	for (mp=mountp; mp!=NULL; mp=mp->m_next)
		mssync(mp);
	unlock(syngate);
	return (0);
}

/*
 * Set the mask for file access.
 */
uumask(mask)
{
	register int omask;

	omask = u.u_umask;
	u.u_umask = mask & 0777;
	return (omask);
}

/*
 * Unmount the given device.
 */
uumount(sp)
char *sp;
{
	register INODE *ip;
	register MOUNT *mp;
	register MOUNT **mpp;
	register dev_t rdev;
	register int mode;

	if (super() == 0)
		return;
	if (ftoi(sp, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (iaccess(ip, IPR|IPW) == 0) {
		idetach(ip);
		return;
	}
	rdev = ip->i_a.i_rdev;
	mode = ip->i_mode;
	idetach(ip);
	if ((mode&IFMT) != IFBLK) {
		u.u_error = ENOTBLK;
		return;
	}
	/*
	 * `syngate' keeps two unmounts, and an unmount and a sync, off the
	 * mount table at the same time.  No inode is locked here, and nothing
	 * below waits on an inode or buffer gate held by a syncer, so this
	 * cannot deadlock.
	 */
	lock(syngate);
	for (mpp=&mountp; (mp=*mpp)!=NULL; mpp=&mp->m_next)
		if (mp->m_dev == rdev)
			break;
	if (mp == NULL) {
		unlock(syngate);
		u.u_error = EINVAL;
		return;
	}
	/*
	 * Only the inodes here:  the super block is written last, once
	 * `bflush' has put the blocks it describes on the disk.  If the
	 * unmount turns out to be busy the file system stays mounted, and
	 * leaving it marked dirty is then the honest answer.
	 */
	isync(mp->m_dev);
	for (ip=&inodep[NINODE-1]; ip>=inodep; --ip) {
		if (ip->i_refc>0 && ip->i_dev==rdev) {
			unlock(syngate);
			u.u_error = EBUSY;
			return;
		}
	}
	/*
	 * The file system is idle and the mount table is ours.  Unlink the
	 * mount entry and uncover the mount point now, before `bflush' and
	 * `dclose', which sleep:  once the entry is gone no path can lead on
	 * to `rdev' and no inode there can be attached, so the state cannot
	 * become busy again while we sleep.  Nothing below can fail, so the
	 * file system is never left half unmounted.
	 */
	*mpp = mp->m_next;
	mp->m_ip->i_flag &= ~IFMNT;
	for (ip=&inodep[NINODE-1]; ip>=inodep; --ip) {
		if (ip->i_dev == rdev)
			ip->i_ino = 0;
	}
	bflush(rdev);
	mssync(mp);			/* clean super block, data is down */
	bflush(rdev);			/* and drop it from the cache again */
	dclose(rdev);
	ldetach(mp->m_ip);
	kfree(mp);
	unlock(syngate);
	return (0);
}

/*
 * Return an unique number.
 */
long
uunique()
{
	register MOUNT *mp;
	register struct filsys *fsp;

	if ((mp=getment(rootdev, 1)) == NULL)
		return;
	fsp = &mp->m_super;
	smod(mp);
	return (++fsp->s_unique);
}

/*
 * Unlink the given file.
 */
uunlink(np)
char *np;
{
	register INODE *ip;
	register INODE *vip;
	register dev_t dev;

	if (ftoi(np, 'u') != 0)
		return;
	ip = u.u_pdiri;
	if (iaccess(ip, IPW) == 0) {
		u.u_error = EACCES;
		goto err;
	}
	dev = ip->i_dev;
	if (iucheck(dev, u.u_cdirn) == 0)
		goto err;
	/*
	 * Get the victim inode before the directory entry is zeroed.  If it
	 * cannot be had (inode table full, read error), the entry is still
	 * there and the file is not orphaned with a link count nobody can
	 * decrement.  `.' (and `..' of the root) names the parent directory,
	 * which we already hold locked, so attaching it again would gate
	 * against ourselves; use it directly instead.
	 */
	if (u.u_cdirn == ip->i_ino)
		vip = ip;
	else if ((vip=iattach(dev, u.u_cdirn)) == NULL)
		goto err;
	idirent(0);
	if (vip->i_nlink > 0)
		--vip->i_nlink;
	icrt(vip);	/* unlink - ctime */
	if ((vip->i_mode&IFMT)==IFPIPE && vip->i_nlink==0 && vip->i_refc==2)
		pevent(vip);
	if (vip != ip)
		idetach(vip);
err:
	idetach(ip);
	return (0);
}

/*
 * Set file times.
 */
uutime(np, utime)
char *np;
time_t utime[2];
{
	register INODE *ip;
	time_t stime[2];

	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (owner(ip->i_uid)) {
		iamc(ip);	/* utime - atime/mtime/ctime */
		if (utime != NULL) {
			ukcopy(utime, stime, sizeof(time_t[2]));
			ip->i_atime = stime[0];
			ip->i_mtime = stime[1];
		}
	}
	idetach(ip);
	return (0);
}

/*
 * Write `n' bytes from buffer `bp' on file number `fd'.
 */
uwrite(fd, bp, n)
char *bp;
unsigned n;
{
	return (sysio(fd, bp, n, 1));
}
