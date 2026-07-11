/*
 * Fragment a directory tree into smaller pieces.
 * Write the pieces onto /dev/fd0 after fdformat'ing,
 * mkfs'ing, and mount'ing on /f0.
 * Preserve the ownerships, modes, dates, links, and order of links
 * within a directory.
 * Make each fragment root based so that a series of
 *	mount /dev/fd0 /f0; cpdir /f0 destination; umount /dev/fd0
 * can be used to reinstall the original directory.
 *
 * Ideally this should be a variation of cpdir, but cpdir
 * does not maintain the in core directory structure necessary
 * for the partitioning.
 *
 * As is the program is not distributable because it assumes it
 * runs as root and that access is unlimited.  Errors are ignored
 * or fatal.
 *
 * The algorithm for partitioning is empirical and may not work very
 * well for directories other than the pc coherent distribution.
 * Some degree of interaction is probably desirable for getting
 * reasonable partitioning of arbitrary directory trees.
 *
 * Overview:
 *	After minimal checks for ldcessary conditions,
 *	Read the source directory tree into a memory
 *		resident pseudo file system in which MINODE inumbers
 *		identify unique files and replace dp->d_ino in the
 *		directories.
 *	While the original root directory is not flagged I_DONE,
 *		copy those parts of the tree that are not flagged I_DONE.
 *		While the copy is too big for the floppy partition
 *			prune the copy.
 *	For each pruned copy produced, fdformat, mkfs, mount, write the
 *		copy, and umount.
 *		/bin/mkdir and /bin/cp are exec'ed to do some of the
 *		work.
 *
 * -- rec 26.VI.84 --
 */
#include <stdio.h>
#include <dir.h>
#include <assert.h>
#include <sys/const.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/filsys.h>
#include <sys/ino.h>

typedef struct MINODE {
	struct MINODE *i_link1;	/* dev x ino hash linkage */
	struct MINODE *i_link2;	/* my inumbering hash linkage */
	char	*i_linkname;	/* Name of first instance of file in copy */
	int	i_mino;		/* My inumber */
	int	i_flag;		/* Miscellaneous flags */
	int	i_blks;		/* Cumulative block size, includes indirects */
	int	i_inos;		/* Cumulative inodes used */
	int	i_size;		/* Total data, indir, and inode blocks */
	int	i_isdir;	/* Simplify many tests */
	dev_t	i_dev;		/* Some fields from stat() */
	ino_t	i_ino;
	int	i_mode;
	int	i_nlink;
	int	i_uid;
	int	i_gid;
	unsigned	i_rdev;
	time_t	i_mtime;
	int	i_nent;		/* Number of directory entries */
	struct direct i_elem[];	/* Directory entries */
} MINODE;

#define I_DONE	1	/* Inode is done */
#define I_COUNT	2	/* Inode is counted */
#define I_PUT	8	/* Inode size reported */
#define I_DONE1	16	/* Inode has been done once, for directories */
#define I_PURGE	32	/* Inode should be purged */
#define I_KEEP	64	/* Keep entire subdirectory */
#define I_CANFIT 128	/* Subdirectory could fit on disk */
#define I_ISMADE 256	/* Inode is made, do link */

#define NDISK 32
MINODE *disks[NDISK];
int	dsize;
int	dused;
int	vflag = 1;
int	excess;
int	ndisk;
int	myuid;
int	mygid;
int	nofflag = 0;		/* Suppress fdformat */
int	roundu = 1;

MINODE	*makeroot();
MINODE	*cpyroot();
MINODE	*cpydir();
MINODE	*select();
int	purge();
int	entermi();
MINODE *fetchmi();
long	blkuse();
char	*myalloc();
char	*string();

extern char edata[];

#define MAXFNAME	512
char fname[MAXFNAME];	/* Filename buffer */
char fname1[MAXFNAME];	/* Second file name buffer */
char cmdbuf[128];
struct stat sbuf;	/* Stat buffer */

main(argc, argv)
char *argv[];
{
	int i;
	MINODE *rip, *tip, *sip;
	/* static char dname[] = "disk #XX"; */

	if (argc < 3)
		usage();
	if (*argv[1] == '-')  {
		while (*++argv[1] != '\0')
			switch (*argv[1])  {
				case 'n':
					nofflag = 1;
					break;
				case 'r':
					roundu = 0;
					break;
				default:
					usage();
			}
		argv[1] = argv[2];
		argv[2] = argv[3];
	}
	dsize = atoi(argv[2]);
/* printf("dsize = %d blocks\n", dsize); */
	myuid = getuid();
	mygid = getgid();
	if (myuid != 0) {
		fprintf(stderr, "cpfrag: must be root\n");
		exit(1);
	}
	umask(0);
	rip = makeroot(argv[1]);
/* printf("root size = %d\n", rip->i_size); */
	while ((rip->i_flag & I_DONE) == 0) {
		/* sprintf(dname, "disk #%d", ndisk+1); */
		tip = cpyroot(rip);
		dused = 4;
		keepers(tip);
/* printf("copy #%d size = %d\n", ndisk+1, tip->i_size); */
		excess = tip->i_size + 2 - dsize;
		while (excess > 0) {
/* printf("\nexcess = %d\n", excess); */
			while ((sip = select(tip)) == NULL) {
/* printf("bump excess\n"); */
				excess += 1;
			}
/* printf("select size = %d\n", sip->i_size); */
			flagroot(sip, I_PURGE);
			purge(tip);
			sizeroot(tip);
/* printf("purge size = %d\n", tip->i_size); */
			excess = tip->i_size + 2 - dsize;
		}
		donedir(tip);
		markdir(rip);
/* printf("%s done\n", dname); */
		disks[ndisk] = tip;
		ndisk += 1;
	}
/* printf("Fragmented to %d disk%s:\n", ndisk, (ndisk>1 ? "s" : "")); */
	for (i = 0; i < ndisk; i += 1) {
		makedisk(i, argv[1]);
/* printf("%s:\n", dname); */
/* printroot(dname, disks[i]); */
	}
/* printf("Original tree:\n"); */
/* printroot(argv[1], rip); */
}

MINODE *
makeroot(cp)
char *cp;
{
	MINODE *rip;

	if (strlen(cp) >= MAXFNAME) {
		fprintf(stderr, "cpfrag: initial path name too long\n");
		exit(1);
	}
	strcpy(fname, cp);
	if (stat(fname, &sbuf) < 0)
		cantstat();
	if ((sbuf.st_mode&S_IFMT) != S_IFDIR) {
		fprintf(stderr, "cpfrag: initial path not directory\n");
		exit(1);
	}
	rip = fetchmi(entermi());
	if (cp[0] == '/' && cp[1] == '\0')
		fname[0] = 0;
	makedir(rip);
	sizeroot(rip);
	return (rip);
}

makedir(ip)
MINODE *ip;
{
	int	fd;
	int	i;
	struct direct *dp1, *dp2;
	MINODE *tip;
	char *cp;

	cp = fname + strlen(fname);
	if (cp + DIRSIZ + 2 >= fname + MAXFNAME) {
		fprintf(stderr, "cpfrag: directory tree too deep\n");
		exit(1);
	}
	if ((fd = open(fname, 0)) < 0) {
		fprintf(stderr, "cpfrag: cannot open: %s\n", fname);
		exit(1);
	}
	i = ip->i_nent * sizeof(struct direct);
	if (read(fd, ip->i_elem, i) != i) {
		fprintf(stderr, "cpfrag: read error: %s\n", fname);
		exit(1);
	}
	close(fd);
	dp1 = dp2 = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		if (dp2->d_name[0] == '.') {
			if (dp2->d_name[1] == 0
			 || (dp2->d_name[1] == '.' && dp2->d_name[2] == 0))
				dp2->d_ino = 0;
		}
		if (dp2->d_ino != 0) {
			if (dp1 != dp2)
				*dp1 = *dp2;
			dp1 += 1;
		}
		dp2 += 1;
	}
	ip->i_nent = dp1 - ip->i_elem;
	i = sizeof(MINODE) + ip->i_nent * sizeof(struct direct);
	if (realloc(ip, i) != ip) {
		fprintf(stderr, "cpfrag: realloc moved block\n");
		exit(1);
	}
	dp1 = ip->i_elem;
	*cp = '/';
	for (i = 0; i < ip->i_nent; i += 1) {
		strncpy(cp+1, dp1->d_name, DIRSIZ);
		if (stat(fname, &sbuf) < 0)
			cantstat();
		tip = fetchmi(dp1->d_ino = entermi());
		if (tip->i_isdir)
			makedir(tip);
		dp1 += 1;
	}
	*cp = 0;
}

printroot(cp, rip)
char *cp;
MINODE *rip;
{
	uflagroot(rip, I_PUT);
	strcpy(fname, cp);
	splat(rip);
	if (cp[0] == '/' && cp[1] == 0)
		fname[0] = 0;
	printdir(rip);
}

printdir(ip)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;
	char *cp;

	dp = ip->i_elem;
	cp = fname + strlen(fname);
	*cp = '/';
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		strncpy(cp+1, dp->d_name, DIRSIZ);
		splat(tip);
		if (tip->i_isdir)
			printdir(tip);
		dp += 1;
	}
	*cp = 0;
}

splat(ip)
MINODE *ip;
{
	printf("(%2d,%2d,%4d) ",
		major(ip->i_dev), minor(ip->i_dev), ip->i_ino);
	if ((ip->i_flag & I_PUT) != 0)
		printf("%6d %4d %6d ", 0, 0, 0);
	else
		printf("%6d %4d %6d ", ip->i_size, ip->i_inos, ip->i_blks);
	printf("%s\n", fname);
	ip->i_flag |= I_PUT;
}

makedisk(n, cp)
char *cp;
{
	MINODE *ip;

	ip = disks[n];
	printf("disk %d, %d inodes, %d data blocks\n",
		n+1, ip->i_inos, ip->i_blks);
again:
	printf("insert disk #%d into drive 0 and type return", n+1);
	gets(cmdbuf);
	if (nofflag == 0)
		if (fdformat() != 0) {
			printf("lets try another diskette\n");
			goto again;
		}
	mkfs(ip->i_inos);
	if (vflag)
		printf("mount /dev/fd0 /f0\n");
	mount("/dev/fd0", "/f0");
	if (cp[0] == '/' && cp[1] == 0)
		fname[0] = 0;
	else
		strcpy(fname, cp);
	strcpy(fname1, "/f0");
	insdir(ip);
	if (vflag)
		printf("umount /dev/fd0\n");
	umount("/dev/fd0");
}

fdformat()
{
#if Z8001
	return(1);
#endif
	sprintf(cmdbuf, "/etc/fdformat -v /dev/rfd0\n");
	if (vflag)
		printf("%s", cmdbuf);
	return (system("/etc/fdformat -v /dev/rfd0\n"));
}

mkfs(nino)
{
	sprintf(cmdbuf, "/etc/mkfs -i %d /dev/rfd0 %d\n", nino, dsize);
	if (vflag)
		printf("%s", cmdbuf);
	return (system(cmdbuf));
}

insdir(ip)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;
	char *cp, *cp1;
	time_t date[2];
	char dtype;

	cp = fname + strlen(fname);
	cp1 = fname1 + strlen(fname1);
	*cp = '/';
	*cp1 = '/';
	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		strncpy(cp+1, dp->d_name, DIRSIZ);
		strncpy(cp1+1, dp->d_name, DIRSIZ);
		if (tip->i_flag & I_ISMADE) {
			if (vflag)
				printf("ln %s %s\n", tip->i_linkname, fname1);
			link(tip->i_linkname, fname1);
			dp += 1;
			continue;
		}
		dtype = 'b';
		switch (tip->i_mode & S_IFMT) {
		case S_IFDIR:
			mkdir(fname1);
			break;
		case S_IFCHR:
			dtype += 1;
		case S_IFBLK:
			if (vflag)
				printf("mknod %s %c %d %d\n", fname1, dtype,
					major(tip->i_rdev), minor(tip->i_rdev));
			mknod(fname1, tip->i_mode, tip->i_rdev);
			break;
		case S_IFREG:
			copy(fname, fname1);
			break;
		default:
			fprintf(stderr, "cpfrag: bad file type %d of %s\n",
				tip->i_mode&S_IFMT, fname);
			exit(1);
		}
		if (tip->i_uid != myuid || tip->i_gid != mygid) {
			if (vflag) {
				printf("chown %d %s\n", tip->i_uid, fname1);
				printf("chgrp %d %s\n", tip->i_gid, fname1);
			}
			chown(fname1, tip->i_uid, tip->i_gid);
		}
		if (vflag)
			printf("chmod %o %s\n", tip->i_mode&~S_IFMT, fname1);
		chmod(fname1, tip->i_mode&~S_IFMT);
		if (tip->i_isdir)
			insdir(tip);
		else if (tip->i_nlink > 1)
			tip->i_linkname = string(fname1);
		tip->i_flag |= I_ISMADE;
		time(&date[0]);
		date[1] = tip->i_mtime;
		utime(fname1, date);
		dp += 1;
	}
	*cp = 0;
	*cp1 = 0;
}

mkdir(cp)
char *cp;
{
	int 	n;
	int	s;

	if (vflag)
		printf("mkdir %s\n", cp);
	if ((n = fork()) == 0) {
		close(2);
		execl("/bin/mkdir", "cpfrag", cp, NULL);
		exit(1);
	}
	while (wait(&s) != n)
		;
	return ((s>>8)&0377);
}

copy(cp1, cp2)
char *cp1, *cp2;
{
	int	n;
	int	s;

	if (vflag)
		printf("cp %s %s\n", cp1, cp2);
	if ((n = fork()) == 0) {
		close(2);
		execl("/bin/cp", "cpfrag", cp1, cp2, NULL);
		exit(1);
	}
	while (wait(&s) != n)
		;
	return ((s>>8)&0377);
}


uflagroot(rip, flag)
MINODE *rip;
{
	uflagdir(rip, flag);
	rip->i_flag &= ~flag;
}

uflagdir(ip, flag)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;

	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		tip->i_flag &= ~flag;
		if (tip->i_isdir)
			uflagdir(tip, flag);
		dp += 1;
	}
}

flagroot(ip, flag)
MINODE *ip;
{
	if (ip->i_isdir)
		flagdir(ip, flag);
	ip->i_flag |= flag;
}

flagdir(ip, flag)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;

	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_isdir)
			flagdir(tip, flag);
		tip->i_flag |= flag;
		dp += 1;
	}
}

sizeroot(rip)
MINODE *rip;
{
	uflagroot(rip, I_COUNT);
	sizedir(rip);
}

sizedir(ip)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;

	ip->i_blks = 0;
	ip->i_inos = 0;
	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_isdir)
			sizedir(tip);
		if ((tip->i_flag&I_COUNT) == 0) {
			ip->i_inos += tip->i_inos;
			ip->i_blks += tip->i_blks;
			tip->i_flag |= I_COUNT;
		}
		dp += 1;
	}
	ip->i_inos += 1;	/* For me */
	if (roundu)
		ip->i_inos += (ip->i_inos % 8 == 0 ? 0 : 8 - (ip->i_inos %8));
	ip->i_blks += blkuse((long)(ip->i_nent+2)*sizeof(struct direct));
	ip->i_size = ip->i_blks + (ip->i_inos+INOPB-1) / INOPB;
}

MINODE *
cpyroot(rip)
MINODE *rip;
{
	uflagdir(rip, I_PURGE|I_KEEP);
	rip = cpydir(rip);
	sizeroot(rip);
	return (rip);
}

MINODE *
cpydir(ip)
MINODE *ip;
{
	int i;
	MINODE *nip, *tip;
	struct direct *dp1, *dp2;

	nip = fetchmi(duplmi(ip));
	nip->i_nent = 0;
	dp1 = ip->i_elem;
	dp2 = nip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp1->d_ino);
		if ((tip->i_flag&I_DONE) != 0)
			tip = NULL;
		else if (tip->i_isdir)
			tip = cpydir(tip);
		if (tip != NULL) {
			dp2->d_ino = tip->i_mino;
			strncpy(dp2->d_name, dp1->d_name, DIRSIZ);
			nip->i_nent += 1;
			dp2 += 1;
		}
		dp1 += 1;
	}
	if (nip->i_nent < ip->i_nent) {
		i = sizeof(MINODE) + nip->i_nent * sizeof(struct direct);
		if (realloc(nip, i) != nip) {
			fprintf(stderr, "cpfrag: realloc moved block\n");
			exit(1);
		}
	}
	return (nip);
}

keepers(ip)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;

	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_isdir == 0) {
			dp += 1;
			continue;
		}
		if (tip->i_size < dsize - 4)
			tip->i_flag |= I_CANFIT;
		if (dused + tip->i_size < dsize) {
			tip->i_flag |= I_KEEP;
			dused += tip->i_size;
		}
		dp += 1;
	}
	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_isdir != 0
		 && (tip->i_flag & (I_CANFIT|I_KEEP)) == 0)
			keepers(tip);
		dp += 1;
	}
}

MINODE *
select(ip)
MINODE *ip;
{
	int i;
	MINODE *uip, *lip, *tip;
	struct direct *dp;

	uip = lip = NULL;
	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_flag & I_KEEP) {
			dp += 1;
			continue;
		}
		if (tip->i_flag & I_CANFIT)
			return (tip);
		if (tip->i_size == excess)
			return (tip);
		else if (tip->i_size > excess) {
			if (uip == NULL || uip->i_size > tip->i_size)
				uip = tip;
		} else {
			if (lip == NULL || lip->i_size < tip->i_size)
				lip = tip;
		}
		dp += 1;
	}
	if (lip != NULL)
		return (lip);
	if (uip->i_isdir)
		return (select(uip));
	return (uip);
}

purge(rip)
MINODE *rip;
{
	int i;
	MINODE *tip;
	struct direct *dp1, *dp2;

	assert(rip->i_isdir);
	dp1 = dp2 = rip->i_elem;
	for (i = 0; i < rip->i_nent; i += 1) {
		tip = fetchmi(dp1->d_ino);
		if (tip->i_isdir) {
			purge(tip);
			if ((tip->i_flag & I_PURGE) && tip->i_nent == 0) {
				dp1->d_ino = 0;
				freemi(tip->i_mino);
			}
		} else if (tip->i_flag & I_PURGE)
			dp1->d_ino = 0;
		if (dp1->d_ino != 0) {
			if (dp1 != dp2)
				*dp2 = *dp1;
			dp2 += 1;
		}
		dp1 += 1;
	}
	rip->i_nent = dp2 - rip->i_elem;
}

donedir(ip)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;

	ip->i_link1->i_flag |= I_DONE1;
	dp = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_isdir)
			donedir(tip);
		else
			tip->i_flag |= I_DONE|I_DONE1;
		dp += 1;
	}
}

markdir(ip)
MINODE *ip;
{
	int i;
	MINODE *tip;
	struct direct *dp;
	int flag;

	if (ip->i_flag & I_DONE)
		return (ip->i_flag);
	dp = ip->i_elem;
	flag = I_DONE;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp->d_ino);
		if (tip->i_isdir)
			markdir(tip);
		flag &= tip->i_flag;
		dp += 1;
	}
	if ((flag & I_DONE) != 0 && (ip->i_flag & I_DONE1) != 0)
		ip->i_flag |= I_DONE;
}

#define IHASH	128
MINODE *ihash1[IHASH];	/* dev x ino hash */
MINODE *ihash2[IHASH];	/* mino hash */
int	minumber = 1;

entermi()
{
	MINODE *ip, **ipp;
	int	nent;

	ipp = &ihash1[sbuf.st_ino % IHASH];
	while ((ip = *ipp) != NULL) {
		if (ip->i_ino == sbuf.st_ino
		 && ip->i_dev == sbuf.st_dev)
			return (ip->i_mino);
		ipp = &ip->i_link1;
	}
	nent = 0;
	if ((sbuf.st_mode&S_IFMT) == S_IFDIR)
		nent = sbuf.st_size / sizeof(struct direct);
	*ipp = ip = myalloc(sizeof(MINODE) + nent * sizeof(struct direct));
	ip->i_dev = sbuf.st_dev;
	ip->i_ino = sbuf.st_ino;
	ip->i_mode = sbuf.st_mode;
	ip->i_nlink = sbuf.st_nlink;
	ip->i_uid = sbuf.st_uid;
	ip->i_gid = sbuf.st_gid;
	ip->i_rdev = sbuf.st_rdev;
	ip->i_mtime = sbuf.st_mtime;
	ip->i_blks = blkuse(sbuf.st_size);
	ip->i_inos = 1;
	ip->i_size = ip->i_blks;
	ip->i_nent = nent;
	ip->i_mino = minumber++;
	ip->i_isdir = (nent != 0);
	ipp = &ihash2[ip->i_mino % IHASH];
	ip->i_link2 = *ipp;
	*ipp = ip;
	return (ip->i_mino);
}

duplmi(ip)
MINODE *ip;
{
	MINODE *nip, **ipp;

	nip = myalloc(sizeof(MINODE) + ip->i_nent * sizeof(struct direct));
	nip->i_dev = ip->i_dev;
	nip->i_ino = ip->i_ino;
	nip->i_mode = ip->i_mode;
	nip->i_nlink = ip->i_nlink;
	nip->i_uid = ip->i_uid;
	nip->i_gid = ip->i_gid;
	nip->i_rdev = ip->i_rdev;
	nip->i_blks = ip->i_blks;
	nip->i_inos = ip->i_inos;
	nip->i_size = ip->i_size;
	nip->i_nent = ip->i_nent;
	nip->i_mino = minumber++;
	nip->i_isdir = ip->i_isdir;
	ipp = &ihash2[nip->i_mino % IHASH];
	nip->i_link2 = *ipp;
	*ipp = nip;
	nip->i_link1 = ip;
	return (nip->i_mino);
}

MINODE *
fetchmi(mino)
{
	MINODE *ip, **ipp;

	ipp = &ihash2[mino % IHASH];
	while ((ip = *ipp) != NULL)
		if (ip->i_mino == mino)
			return (ip);
		else
			ipp = &ip->i_link2;
	fprintf(stderr, "cpfrag: nonexistent internal inumber %d\n", mino);
	exit(1);
}

freemi(mino)
{
	MINODE *ip, **ipp;

	ipp = &ihash2[mino % IHASH];
	while ((ip = *ipp) != NULL)
		if (ip->i_mino == mino) {
			*ipp = ip->i_link2;
			free(ip);
			return;
		} else
			ipp = &ip->i_link2;
	fprintf(stderr, "cpfrag: nonexistent internal inumber %d\n", mino);
	exit(1);
}
/*
 * A corrected disk usage computation
 * for retrofit into /usr/src/cmd/du.c, /usr/src/cmd/ls.c/prsize(),
 * and /usr/src/cmd/quot.c since they are all wrong.
 *
 * And this is not quite right either since it doesn't deal with sparse
 * blocks.
 */
long
blkuse(nb)
long nb;
{
#undef NBN
#define NBN	128L
#define nindir(x)	(((x)+NBN-1)/NBN)
#define nblock(x)	(((x)+BSIZE-1)/BSIZE)
#define min(x, y)	((x)<(y) ? (x) : (y))
	long bu, ndir, nidir, niidir;

	nb = nblock(nb);
	ndir = min(nb, ND);
	nb -= ndir;
	bu = ndir;
	if (nb) {
		nidir = min(nb, NBN);
		nb -= nidir;
		bu += nidir + 1;
		if (nb) {
			niidir = min(nb, NBN*NBN);
			nb -= niidir;
			bu += niidir + 1 + nindir(niidir);
			if (nb)
				bu += nb + 1 + nindir(nindir(nb)) + nindir(nb);
		}
	}
	return (bu);
}

char *
string(cp)
char *cp;
{
	char *sp;

	sp = myalloc(strlen(cp)+1);
	strcpy(sp, cp);
	return (sp);
}

char *
myalloc(nb)
int nb;
{
	char *p;

	if ((p = malloc(nb)) == NULL) {
		fprintf(stderr, "cpfrag: out of space\n");
		exit(1);
	}
	while (--nb >= 0)
		p[nb] = 0;
	return (p);
}

usage()
{
	fprintf(stderr, "Usage: cpfrag [-nf] directory size_of_floppy\n");
	exit(1);
}

cantstat()
{
	fprintf(stderr, "cpfrag: cannot stat: %s\n", fname);
	exit(1);
}

/*
pmino(ip)
MINODE *ip;
{
	printf("ip = %x\n", ip);
	printf("i_link1 == %x\n", ip->i_link1);
	printf("i_link2 == %x\n", ip->i_link2);
	printf("i_linkname == %s\n", ip->i_linkname);
	printf("i_mino == %d\n", ip->i_mino);
	printf("i_flag == %d\n", ip->i_flag);
	printf("i_blks == %d\n", ip->i_blks);
	printf("i_inos == %d\n", ip->i_inos);
	printf("i_size == %d\n", ip->i_size);
	printf("i_isdir == %d\n", ip->i_isdir);
	printf("i_dev == %x\n", ip->i_dev);
	printf("i_ino == %d\n", ip->i_ino);
	printf("i_mode == %d\n", ip->i_mode);
	printf("i_nlink == %d\n", ip->i_nlink);
	printf("i_uid == %d\n", ip->i_uid);
	printf("i_gid == %d\n", ip->i_gid);
	printf("i_rdev == %d\n", ip->i_rdev);
	printf("i_mtime == %D\n", ip->i_mtime);
}
*/
