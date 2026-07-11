/*
 * Given a directory tree root and a filesystem size,
 * write the fewest mkfs proto files necessary to
 * copy the directory tree onto floppies.
 * Preserve the ownerships, modes, dates, links, and order of links
 * within a directory.
 * Make each fragment root based so that a series of
 *	mount /dev/fd0 /f0; cpdir /f0 destination; umount /dev/fd0
 * can be used to reinstall the original directory.
 *
 * The algorithm for partitioning is empirical and may not work very
 * well for directories other than the pc coherent distribution.
 * Some degree of interaction is probably desirable for getting
 * reasonable partitioning of arbitrary directory trees.
 *
 * Overview:
 *	After minimal checks for necessary conditions,
 *	Read the source directory tree into a memory
 *		resident pseudo file system in which MINODE inumbers
 *		identify unique files and replace dp->d_ino in the
 *		directories.
 *	While the original root directory is not flagged I_DONE,
 *		copy those parts of the tree that are not flagged I_DONE.
 *		While the copy is too big for the floppy partition
 *			prune the copy.
 *	For each pruned copy produced, write the mkfs proto.
 *
 * -- rec 26.VI.84 -- invent cpfrag.
 * -- rec 12.IX.84 -- reconstruct cpfrag -> unmkfs.
 * -- norm 04.I.85 -- fix misc. bugs for z8000
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
	int	i_rdev;
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
struct stat tbuf;	/* Time buffer, leave zero for all times */
char *argv0;		/* For error recovery */

main(argc, argv)
char *argv[];
{
	int i;
	MINODE *rip, *tip, *sip;

	argv0 = argv[0];
	if (argc < 3)
		usage();
	dsize = atoi(argv[2]);
	if (argc == 4) {
		if (stat(argv[3], &tbuf) < 0) {
			fprintf(stderr, "unmkfs: can't stat %s\n", argv[3]);
			exit(1);
		}
	} else
		tbuf.st_mtime = 0;	/* make time == 0 to get all files */
	rip = makeroot(argv[1]);
	while ((rip->i_flag & I_DONE) == 0) {
		tip = cpyroot(rip);
		dused = 4;
		keepers(tip);
		excess = tip->i_size + 2 - dsize;
		while (excess > 0) {
			while ((sip = select(tip)) == NULL) {
				excess += 1;
			}
			flagroot(sip, I_PURGE);
			purge(tip);
			sizeroot(tip);
			excess = tip->i_size + 2 - dsize;
		}
		donedir(tip);
		markdir(rip);
		disks[ndisk] = tip;
		ndisk += 1;
	}
	for (i = 0; i < ndisk; i += 1) {
		makedisk(i, argv[1], argv[2]);
	}
}

/*
** Get the MINODE * corresponding to fname, and call
** makedir() to build the in-memory tree.  Call sizeroot()
** return the MINODE corresponding to the root.
*/
MINODE *
makeroot(cp)
char *cp;
{
	MINODE *rip;

	if (strlen(cp) >= MAXFNAME) {
		fprintf(stderr, "unmkfs: initial path name too long\n");
		exit(1);
	}
	strcpy(fname, cp);
	if (stat(fname, &sbuf) < 0)
		cantstat();
	if ((sbuf.st_mode&S_IFMT) != S_IFDIR) {
		fprintf(stderr, "unmkfs: initial path not directory\n");
		exit(1);
	}
	rip = fetchmi(entermi());
	if (cp[0] == '/' && cp[1] == '\0')
		fname[0] = 0;
	makedir(rip);
	sizeroot(rip);
	return (rip);
}

/*
** Recursively build a tree of MINODE pointers
** for the directory ip.
*/
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
		fprintf(stderr, "unmkfs: directory tree too deep\n");
		exit(1);
	}
	if ((fd = open(fname, 0)) < 0) {
		fprintf(stderr, "unmkfs: cannot open: %s\n", fname);
		exit(1);
	}
	i = ip->i_nent * sizeof(struct direct);
	if (read(fd, ip->i_elem, i) != i) {
		fprintf(stderr, "unmkfs: read error: %s\n", fname);
		exit(1);
	}
	close(fd);
	*cp = '/';
	dp1 = dp2 = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		if (dp2->d_name[0] == '.') {
			if (dp2->d_name[1] == 0
			 || (dp2->d_name[1] == '.' && dp2->d_name[2] == 0))
				dp2->d_ino = 0;
		}
		if (dp2->d_ino != 0) {
			strncpy(cp+1, dp2->d_name, DIRSIZ);
			if (stat(fname, &sbuf) < 0)
				cantstat();
			dp2->d_ino = entermi();
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
		fprintf(stderr, "unmkfs: realloc moved block\n");
		exit(1);
	}
	dp1 = ip->i_elem;
	for (i = 0; i < ip->i_nent; i += 1) {
		tip = fetchmi(dp1->d_ino);
		if (tip->i_isdir) {
			strncpy(cp+1, dp1->d_name, DIRSIZ);
			makedir(tip);
		}
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

FILE *ofp;

makedisk(n, cp, sp)
char *cp, *sp;
{
	MINODE *ip;
	static char dname[32];

	ip = disks[n];
	fprintf(stderr, "\ndisk %d, %d inodes, %d data blocks\n",
		n+1, ip->i_inos, ip->i_blks);
	ofp = stdout;
	mkfs(ip->i_inos);
	if (cp[0] == '/' && cp[1] == 0)
		fname[0] = 0;
	else
		strcpy(fname, cp);
	fprintf(ofp, "d--%03o %3d %3d\n", ip->i_mode&0777, ip->i_uid,
		ip->i_gid);
	indent(1);
	insdir(ip);
	indent(-1);
	fprintf(ofp, "$\n");
}

mkfs(nino)
{
	fprintf(ofp, "/dev/null xxxxx xxxxx\n");
	fprintf(ofp, "%d %d 1 1\n", dsize, nino);
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
		indent(0);
		fprintf(ofp, "%-14s", dp->d_name);
		if (tip->i_flag & I_ISMADE) {
			fprintf(ofp, " l-----   0   0 %s\n", tip->i_linkname);
			dp += 1;
			continue;
		}
		switch (tip->i_mode & S_IFMT) {
		case S_IFDIR:
			dtype = 'd';
			break;
		case S_IFCHR:
			dtype = 'c';
			break;
		case S_IFBLK:
			dtype = 'b';
			break;
		case S_IFREG:
			dtype = '-';
			break;
		default:
			fprintf(stderr, "unmkfs: bad file type %d of %s\n",
				tip->i_mode&S_IFMT, fname);
			exit(1);
		}
		fprintf(ofp, " %c%c%c%03o %3d %3d",
			dtype,
			(tip->i_mode&ISUID) ? 'u' : '-',
			(tip->i_mode&ISGID) ? 'g' : '-',
			tip->i_mode&0777,
			tip->i_uid, tip->i_gid);
		switch (tip->i_mode & S_IFMT) {
		case S_IFDIR:
			fputc('\n', ofp);
			indent(1);
			insdir(tip);
			indent(-1);
			indent(0);
			fprintf(ofp, "$\n");
			break;
		case S_IFCHR:
		case S_IFBLK:
			fprintf(ofp, "%3d %3d\n", major(tip->i_rdev),
				minor(tip->i_rdev));
			break;
		case S_IFREG:
			fprintf(ofp, " %s\n", fname);
			break;
		}
		if (tip->i_nlink > 1)
			tip->i_linkname = string(fname1);
		tip->i_flag |= I_ISMADE;
		dp += 1;
	}
	*cp = 0;
	*cp1 = 0;
}

indent(n)
int n;
{
	static int indent;

	if (n < 0)
		indent -= 1;
	else if (n > 0)
		indent += 1;
	else for (n = indent; --n >= 0; fprintf(ofp, " "));
}

uflagroot(rip, flag)
MINODE *rip;
{
	uflagdir(rip, flag);
	rip->i_flag &= ~flag;
}

/*
** Recursively turn off flag in ip and
** all directories below ip.
*/
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
	/* Add in bad block inode */
	rip->i_size = rip->i_blks + (++rip->i_inos+INOPB-1) / INOPB;
}

/*
** For subdirectories not flagged I_COUNT,
** add the isize and blksize to that of ip.
*/
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
			fprintf(stderr, "unmkfs: realloc moved block\n");
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
#if I8086
	assert((char *)&dp2 > edata + 16);
#endif
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

/*
** Return the mino in the hash table ihash1 corresponding to 
** the statbuf.  If not found, enter it and return the resulting
** entry.
*/
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
	else if (sbuf.st_mtime < tbuf.st_mtime)
		return 0;
	else if (blkuse(sbuf.st_size) > dsize-5) {
		fprintf(stderr, "unmkfs: file %s too large - omitted\n", fname);
		return 0;
	}
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

/*
** Find the entry i ihash2 corresponding to mino.
** Die with message if not there.
*/
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
	fprintf(stderr, "unmkfs: nonexistent internal inumber %d\n", mino);
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
	fprintf(stderr, "unmkfs: nonexistent internal inumber %d\n", mino);
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
		fprintf(stderr, "unmkfs: out of space\n");
		exit(1);
	}
	while (--nb >= 0)
		p[nb] = 0;
	return (p);
}

usage()
{
	fprintf(stderr, "Usage: unmkfs directory size_in_blocks [filename]\n");
	exit(1);
}

cantstat()
{
	fprintf(stderr, "unmkfs: cannot stat: %s\n", fname);
	exit(1);
}

