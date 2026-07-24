/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * System calls (filesystem related).
 */
#include <coherent.h>
#include <errno.h>
#include <fd.h>
#include <ino.h>
#include <inode.h>
#include <mount.h>
#include <stat.h>
#include <uproc.h>

/*
 * Determine accessibility of the given file.
 */
uaccess(np, mode)
char *np;
register int mode;
{
	register INODE *ip;
	register int r;

	schizo();
	r = ftoi(np, 'r');
	schizo();
	if (r != 0)
		return;
	ip = u.u_cdiri;
	if ((mode&imode(ip, u.u_ruid, u.u_rgid)) != mode)
		u.u_error = EACCES;
	idetach(ip);
	return (0);
}

/*
 * Schizo - swap real and effective id's.
 */
schizo()
{
	register int t;

	t = u.u_uid;
	u.u_uid = u.u_ruid;
	u.u_ruid = t;
	t = u.u_gid;
	u.u_gid = u.u_rgid;
	u.u_rgid = t;
}

/*
 * Turn accounting on or off.
 */
uacct(np)
register char *np;
{
	register INODE *ip;

	if (super() == 0)
		return;
	if (np == NULL) {
		if (acctip == NULL) {
			u.u_error = EINVAL;
			return;
		}
		ldetach(acctip);
		acctip = NULL;
	} else {
		if (acctip != NULL) {
			u.u_error = EINVAL;
			return;
		}
		if (ftoi(np, 'r') != 0)
			return;
		ip = u.u_cdiri;
		if ((ip->i_mode&IFMT) != IFREG) {
			u.u_error = EINVAL;
			idetach(ip);
			return;
		}
		iunlock(ip);
		acctip = ip;
	}
	return (0);
}

/*
 * Set current directory.
 */
uchdir(np)
char *np;
{
	setcdir(np, &u.u_cdir);
	return (0);
}

/*
 * Given a directory name and a pointer to a working directory pointer,
 * Save the inode associated with the directory name in the working
 * directory pointer and release the old one.  This is used to change
 * working and root directories.
 */
setcdir(np, ipp)
char *np;
register INODE **ipp;
{
	register INODE *ip;

	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if ((ip->i_mode&IFMT) != IFDIR) {
		u.u_error = ENOTDIR;
		idetach(ip);
		return;
	}
	if (iaccess(ip, IPE) == 0) {
		u.u_error = EACCES;
		idetach(ip);
		return;
	}
	iunlock(ip);
	ldetach(*ipp);
	*ipp = ip;
}

/*
 * Change the mode of a file.
 */
uchmod(np, mode)
char *np;
{
	register INODE *ip;

	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (owner(ip->i_uid)) {
		if (u.u_uid != 0)
			mode &= ~ISVTXT;
		ip->i_mode &= IFMT;
		ip->i_mode |= mode&~IFMT;
		icrt(ip);	/* chmod - ctime */
	}
	idetach(ip);
	return (0);
}

/*
 * Change owner and group of a file.
 */
uchown(np, uid, gid)
char *np;
{
	register INODE *ip;

	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (super()) {
		ip->i_mode &= ~(ISUID | ISGID);  /* clear any setuid/setgid */
		ip->i_uid = uid;
		ip->i_gid = gid;
		icrt(ip);	/* chown - ctime */
	}
	idetach(ip);
	return (0);
}

/*
 * Set root directory.
 */
uchroot(np)
register char *np;
{
	if (super())
		setcdir(np, &u.u_rdir);
	return (0);
}

/*
 * Close the given file descriptor.
 */
uclose(fd)
{
	fdclose(fd);
	return (0);
}

/*
 * Create a file with the given mode.
 */
ucreat(np, mode)
char *np;
register int mode;
{
	register INODE *ip;
	register int fd;
	register int cflag;

	cflag = 0;
	if (ftoi(np, 'c') != 0)
		return;
	if ((ip=u.u_cdiri) == NULL) {
		if ((ip=imake((mode&~IFMT)|IFREG, 0)) == NULL)
			return;
	} else {
		if (iaccess(ip, IPW)==0 || getment(ip->i_dev, 1)==NULL) {
			idetach(ip);
			return;
		}
		if ((ip->i_mode&IFMT) == IFDIR) {
			u.u_error = EISDIR;
			idetach(ip);
			return;
		}
		cflag = 1;
	}
	if ((fd=fdopen(ip, IPW)) < 0) {
		idetach(ip);
		return;
	}
	if (cflag)
		iclear(ip);
	iunlock(ip);
	return (fd);
}

/*
 * Duplicate a file descriptor.
 */
udup(ofd, nfd)
{
	return (fddup(ofd, nfd));
}

/*
 * Given a file descriptor, return a status structure.
 */
ufstat(fd, stp)
struct stat *stp;
{
	register INODE *ip;
	register FD *fdp;
	struct stat stat;

	if ((fdp=fdget(fd)) == NULL)
		return;
	ip = fdp->f_ip;
	istat(ip, &stat);
	kucopy(&stat, stp, sizeof(stat));
	return (0);
}

/*
 * Device control information.
 */
uioctl(fd, r, argp)
struct sgttyb *argp;
{
	register FD *fdp;
	register INODE *ip;
	register int mode;

	if ((fdp=fdget(fd)) == NULL)
		return;
	ip = fdp->f_ip;
	mode = ip->i_mode&IFMT;
	if (mode!=IFCHR && mode!=IFBLK) {
		u.u_error = ENOTTY;
		return;
	}
	dioctl(ip->i_a.i_rdev, r, argp);
	return (0);
}

/*
 * Create a link, `np2' to the already existing file `np1'.
 */
ulink(np1, np2)
char *np1;
char *np2;
{
	register INODE *ip1;

	if (ftoi(np1, 'r') != 0)
		return;
	ip1 = u.u_cdiri;
	if ((ip1->i_mode&IFMT)==IFDIR && super()==0) {
		idetach(ip1);
		return;
	}
	iunlock(ip1);
	if (ftoi(np2, 'c') != 0) {
		ldetach(ip1);
		return;
	}
	if (u.u_cdiri != NULL) {
		u.u_error = EEXIST;
		idetach(u.u_cdiri);
		ldetach(ip1);
		return;
	}
	if (ip1->i_dev != u.u_pdiri->i_dev) {
		u.u_error = EXDEV;
		idetach(u.u_pdiri);
		ldetach(ip1);
		return;
	}
	if (iaccess(u.u_pdiri, IPW) == 0) {
		idetach(u.u_pdiri);
		ldetach(ip1);
		return;
	}
	idirent(ip1->i_ino);
	idetach(u.u_pdiri);
	ilock(ip1);
	ip1->i_nlink++;
	icrt(ip1);	/* link - ctime */
	idetach(ip1);
	return (0);
}

/*
 * Seek on the given file descriptor.
 */
size_t
ulseek(fd, off, w)
register size_t off;
{
	register FD *fdp;
	register INODE *ip;

	if ((fdp=fdget(fd)) == NULL)
		return;
	ip = fdp->f_ip;
	if ((ip->i_mode&IFMT) == IFPIPE) {
		u.u_error = ESPIPE;
		return;
	}
	switch (w) {
	case 0:
		break;
	case 1:
		off += fdp->f_seek;
		break;
	case 2:
		off += ip->i_size;
		break;
	default:
		u.u_error = EINVAL;
		return;
	}
	if (off < 0) {
		u.u_error = EINVAL;
		return;
	}
	fdp->f_seek = off;
	return (off);
}

/*
 * Create a special file.
 */
umknod(np, mode, rdev)
char *np;
dev_t rdev;
{
	register INODE *ip;
	register int type;

	type = mode&IFMT;
	if (type!=IFPIPE && super()==0)
		return;
	if (type!=IFBLK && type!=IFCHR)
		rdev = 0;
	if (ftoi(np, 'c') != 0)
		return;
	if ((ip=u.u_cdiri) != NULL) {
		u.u_error = EEXIST;
		idetach(ip);
		return;
	}
	if ((ip=imake(mode, rdev)) != NULL)
		idetach(ip);
	return (0);
}

/*
 * Mount the device `sp' on the pathname `np'.  The flag, `f',
 * indicates that the device is to be mounted read only.
 */
umount(sp, np, f)
char *sp;
char *np;
{
	register INODE *ip;
	register MOUNT *mp;
	register dev_t rdev;
	register int mode;

	if (ftoi(sp, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (iaccess(ip, IPR|IPW) == 0)
		goto err;
	mode = ip->i_mode;
	rdev = ip->i_a.i_rdev;
	if ((mode&IFMT) != IFBLK) {
		u.u_error = ENOTBLK;
		goto err;
	}
	idetach(ip);
	/*
	 * Remount (mount -w): promote the already-mounted device to
	 * read/write in place.  Only the device matters, so the mount
	 * directory `np' is not consulted.
	 */
	if ((f&MFRMT) != 0) {
		for (mp=mountp; mp!=NULL; mp=mp->m_next)
			if (mp->m_dev == rdev)
				break;
		if (mp == NULL)
			u.u_error = EINVAL;
		else
			fsremount(mp, f);
		return (0);
	}
	if (ftoi(np, 'r') != 0)
		return;
	ip = u.u_cdiri;
	if (iaccess(ip, IPR) == 0)
		goto err;
	if ((ip->i_mode&IFMT) != IFDIR) {
		u.u_error = ENOTDIR;
		goto err;
	}
	/* Check for current directory, open, or mount directory */
	if (ip->i_refc > 1 || ip->i_ino == ROOTIN) {
		u.u_error = EBUSY;
		goto err;
	}
	for (mp=mountp; mp!=NULL; mp=mp->m_next) {
		if (mp->m_dev == rdev) {
			u.u_error = EBUSY;
			goto err;
		}
	}
	if ((mp=fsmount(rdev, f)) == NULL)
		goto err;
	mp->m_ip = ip;
	ip->i_flag |= IFMNT;
	ip->i_refc++;
err:
	idetach(ip);
	return (0);
}
