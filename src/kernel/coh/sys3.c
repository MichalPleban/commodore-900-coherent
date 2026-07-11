/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
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
	static GATE syngate;

	lock(syngate);
	for (mp=mountp; mp!=NULL; mp=mp->m_next)
		msync(mp);
	bsync();
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
	for (mpp=&mountp; (mp=*mpp)!=NULL; mpp=&mp->m_next)
		if (mp->m_dev == rdev)
			break;
	if (mp == NULL) {
		u.u_error = EINVAL;
		return;
	}
	msync(mp);
	for (ip=&inodep[NINODE-1]; ip>=inodep; --ip) {
		if (ip->i_refc>0 && ip->i_dev==rdev) {
			u.u_error = EBUSY;
			return;
		}
	}
	for (ip=&inodep[NINODE-1]; ip>=inodep; --ip) {
		if (ip->i_dev == rdev)
			ip->i_ino = 0;
	}
	bflush(rdev);
	dclose(rdev);
	*mpp = mp->m_next;
	mp->m_ip->i_flag &= ~IFMNT;
	ldetach(mp->m_ip);
	kfree(mp);
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
	fsp->s_fmod = 1;
	return (++fsp->s_unique);
}

/*
 * Unlink the given file.
 */
uunlink(np)
char *np;
{
	register INODE *ip;
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
	idirent(0);
	idetach(ip);
	if ((ip=iattach(dev, u.u_cdirn)) == NULL)
		return;
	if (ip->i_nlink > 0)
		--ip->i_nlink;
	icrt(ip);	/* unlink - ctime */
	if ((ip->i_mode&IFMT)==IFPIPE && ip->i_nlink==0 && ip->i_refc==2)
		pevent(ip);
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
