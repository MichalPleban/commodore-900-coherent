/*
 * List structure
 */

#include <stdio.h>
#include <canon.h>
#include <time.h>
#include <stat.h>
#include <dir.h>
#include <pwd.h>
#include <grp.h>

#define	BSIZE	BUFSIZ		/* Disc blocking factor for `-s' */
#define	NBN	128		/* Number of blocks in an indirect block */
#define	ND	10		/* Number of direct blocks */
#define	NI	1		/* Number of indirect blocks */
#define	NII	1		/* Number of double indirect blocks */
#define	NIII	1		/* Number of triple indirect blocks */

#define	MTIME	0		/* Use modify time */
#define	ATIME	1		/* Use access time */
#define	CTIME	2		/* Use create time */

#define	OLD	(60L*60*24*365)	/* old form of dates (seconds) */

int	aflag;			/* List all entries (including "." & "..") */
int	dflag;			/* Treat directories like files */
int	fflag;			/* Force something to look like a directory */
int	gflag;			/* Print gid vs. uid */
int	iflag;			/* Give i-number */
int	lflag;			/* Longer format */
int	rflag;			/* Reverse order of sort */
int	sflag;			/* Print size in bytes */
int	tflag = MTIME;		/* Which time to display and sort on */
int	sortflg;		/* On for sort by time, 0 for by name */
int	myuid;			/* User id for selecting .* suppression */

time_t	curtime;

char	obuf[BUFSIZ];
char	namebuf[200];		/* Buffer for constructing names */
char	dirbuf[BSIZE];		/* Buffer for reading directories */
char	*deflist[] = {
	".",
	NULL
};

struct	ls {
	char	ls_dname[DIRSIZ+1];
	char	*ls_name;
	ino_t	ls_ino;
	short	ls_mode;
	short	ls_uid;			/* Uid or gid */
	short	ls_nlink;
	size_t	ls_size;
	dev_t	ls_rdev;		/* Real device */
	time_t	ls_time;		/* One of atime, mtime, ctime */
};

struct	ls	*saved;		/* Array of saved info */
struct	ls	*savep;
struct	ls	*asavep;	/* Pointer for argument storage */
size_t	dirsize;			/* Size of directory */

int	(*qcomp)();
int	qtcomp();
int	qncomp();
int	qdncomp();
char	*getuname();

main(argc, argv)
char *argv[];
{
	register char *ap;
	register int es;

	setbuf(stdout, obuf);
	while (argc>1 && *argv[1]=='-') {
		for (ap=&argv[1][1]; *ap; ap++)
			switch (*ap) {
			case 'a':
				aflag = 1;
				break;

			case 'c':
				tflag = CTIME;
				break;

			case 'd':
				dflag = 1;
				break;

			case 'f':
				aflag = fflag = 1;
				lflag = sflag = 0;
				break;

			case 'g':
				gflag = 1;
				break;

			case 'i':
				iflag = 1;
				break;

			case 'l':
				lflag = 1;
				break;

			case 'r':
				rflag = 1;
				break;

			case 's':
				sflag = 1;
				break;

			case 't':
				sortflg = 1;
				break;

			case 'u':
				tflag = ATIME;
				break;

			default:
				usage();
			}
		argc--;
		argv++;
	}
	time(&curtime);
	myuid = getuid();
	if (sortflg)
		qcomp = qtcomp; else
		qcomp = qncomp;
	if (argc > 1)
		es = ls(argv+1, argc-1); else
		es = ls(deflist, 1);
	exit(es);
}

/*
 * Do `ls' on one file or
 * directory.
 * `narg' is the number of names
 * in `flist' to determine special
 * output format.
 */
ls(flist, narg)
register char **flist;
int narg;
{
	register int estat = 0;
	register struct ls *lsp;
	register struct ls *arena;
	struct stat sb;

	if ((arena = (struct ls *)malloc(narg*sizeof(struct ls))) == NULL) {
		fprintf(stderr, "ls: out of memory for arguments\n");
		exit(1);
	}
	asavep = arena;
	for ( ; *flist!=NULL; flist++) {
		if (stat(*flist, &sb) < 0) {
			perror(*flist);
			estat = 1;
			continue;
		}
		astore(*flist, &sb);
	}
	qsort(arena, asavep-arena, sizeof(struct ls), qcomp);
	if (qcomp == qncomp)
		qcomp = qdncomp;
	for (lsp = arena; lsp < asavep; lsp++) {
		if (fflag)
			continue;
		if ((lsp->ls_mode&S_IFMT)==S_IFDIR && !dflag)
			continue;
		prstuff(lsp->ls_name, lsp);
	}
	for (lsp = arena; lsp < asavep; lsp++) {
		if (dflag || (lsp->ls_mode&S_IFMT)!=S_IFDIR)
			continue;
		dirsize = lsp->ls_size;
		if (narg > 1)
			printf("\n%s:\n", lsp->ls_name);
		lsdir(lsp->ls_name);
	}
	return (estat);
}

prstuff(file, lsp)
char *file;
register struct ls *lsp;
{
	register char *cp;
	register spcl = 0;

	if (iflag)
		printf("%5u ", lsp->ls_ino);
	if (sflag) {
		prsize(lsp);
		putchar(' ');
	}
	if (lflag) {
		switch (lsp->ls_mode & S_IFMT) {
		case S_IFREG:
			putchar('-');
			break;

		case S_IFDIR:
			putchar('d');
			break;

		case S_IFCHR:
			putchar('c');
			spcl++;
			break;

		case S_IFBLK:
			putchar('b');
			spcl++;
			break;

		case S_IFPIP:
			putchar('p');
			break;

		case S_IFMPB:
		case S_IFMPC:
			putchar('m');
			spcl++;
			break;

		default:
			putchar('x');
		}
		prmode((lsp->ls_mode>>6)&07, lsp->ls_mode&S_ISUID);
		prmode((lsp->ls_mode>>3)&07, lsp->ls_mode&S_ISGID);
		prmode(lsp->ls_mode&07, 0);
		if (lsp->ls_mode & S_ISVTX)
			putchar('t'); else
			putchar(' ');
		printf("%2d ", lsp->ls_nlink);
		cp = getuname(lsp->ls_uid);
		if (cp == NULL)
			printf("%-8d ", lsp->ls_uid); else
			printf("%-8s ", cp);
		if (!spcl)
			printf("%7ld", lsp->ls_size);
		else
			printf("%3d %3d", major(lsp->ls_rdev),
			    minor(lsp->ls_rdev));
		prtime(&lsp->ls_time);
	}
	printf("%s", file);
	putchar('\n');
}

/*
 * Print out a filesize from a ls store buffer.
 * This size (in BSIZE units or blocks) takes
 * into account indirect blocks.
 * However this should be done in a more general manner.
 */
prsize(lsp)
register struct ls *lsp;
{
	long blocks, size;
	register ftype;

	size = 0;
	ftype = lsp->ls_mode & S_IFMT;
	if (ftype==S_IFREG || ftype==S_IFDIR || ftype==S_IFPIP) {
		size = blocks = (lsp->ls_size+BSIZE-1)/BSIZE;
		if (blocks > ND) {
			size++;
			blocks -= ND;
			if (blocks > NBN*NI) {
				blocks -= NBN*NI;
				size += 2 + blocks/NBN;
			}
		}
	}
	printf("%4ld", size);
	return (size);
}

/*
 * Print a time (if it is older than
 * one year) print the year instead
 * of the mm:ss part.
 */
prtime(tp)
register time_t *tp;
{
	register struct tm *tmp;
	register char *cp;
	time_t tdiff;

	cp = asctime(tmp = localtime(tp));
	if ((tdiff = curtime-*tp)>OLD || tdiff<0) {
		cp[10] = '\0';
		printf(" %s  %d ", cp, tmp->tm_year+1900);
	} else {
		cp[16] = '\0';
		printf(" %s ", cp);
	}
}

/*
 * Print 'rwx' type modes out.
 */
prmode(m, suid)
int m;
int suid;
{
	m <<= 6;
	putchar(m&S_IREAD ? 'r' : '-');
	putchar(m&S_IWRITE ? 'w' : '-');
	if (suid)
		putchar('s');
	else
		putchar(m&S_IEXEC ? 'x' : '-');
}

/*
 * Get a user name.  Either look
 * in password or group file depending
 * on `gflag'.
 */
char *
getuname(uid)
short uid;
{
	register struct passwd	*pwp;
	register struct group	*grp;
	static		id	= -1;
	static char	*name;

	if (uid == id)
		return (name);
	id = uid;
	name = NULL;
	if (gflag) {
		if ((grp=getgrgid( uid)) != NULL)
			name = grp->gr_name;
	}
	else {
		if ((pwp=getpwuid( uid)) != NULL)
			name = pwp->pw_name;
	}
	return (name);
}

/*
 * List out the files in a directory
 * If ``fflag'' is set, it may not be
 * but consider it one anyway.
 */
lsdir(dir)
char *dir;
{
	int fd;
	struct stat sb;
	struct ls ls;
	register char *np1, *np2;
	register int n;
	register struct direct *dp;
	register int nb;
	char curname[DIRSIZ+1];
	unsigned size;

	if ((fd = open(dir, 0)) < 0) {
		fprintf(stderr, "%s: cannot read\n", dir);
		return;
	}
	if (!fflag) {
		size = dirsize/sizeof (struct direct) * sizeof (struct ls);
		if ((saved = malloc(size)) == NULL) {
			fprintf(stderr, "Out of memory\n");
			exit (1);
		}
		savep = saved;
	}
	while ((nb = read(fd, dirbuf, sizeof (dirbuf))) > 0)
	for (dp=dirbuf; dp<&dirbuf[nb]; dp++) {
		if (dp->d_ino == 0)
			continue;
		np1 = dp->d_name;
		if (aflag == 0 && *np1++ == '.') {
			if (myuid != 0)
				continue;
			if (*np1=='\0' || (*np1++=='.' && *np1=='\0'))
				continue;
		}
		if (iflag) {
			sb.st_ino = dp->d_ino;
			canino(sb.st_ino);
		}
		np2 = curname;
		np1 = dp->d_name;
		n = DIRSIZ;
		do {
			*np2++ = *np1++;
		} while (--n);
		*np2 = '\0';
		if (lflag || sflag || tflag || sortflg) {
			np2 = namebuf;
			np1 = dir;
			while (*np2++ = *np1++)
				;
			--np2;
			*np2++ = '/';
			np1 = curname;
			while (*np2++ = *np1++)
				;
			if (stat(namebuf, &sb) < 0) {
				fprintf(stderr, "%s: cannot stat\n", curname);
				continue;
			}
		}
		if (fflag) {
			convert(&sb, &ls);
			prstuff(curname, &ls);
		} else
			store(curname, &sb);
	}
	if (!fflag)
		output();
	close(fd);
}

/*
 * Store data away for intra-directory
 * sorting.
 */
store(name, sbp)
char *name;
register struct stat *sbp;
{
	register struct ls *lsp;

	lsp = savep++;
	strncpy(lsp->ls_dname, name, DIRSIZ+1);
	convert(sbp, lsp);
}

/*
 * Store each argument away for inter-directory
 * sorting.
 */
astore(name, sbp)
char *name;
register struct stat *sbp;
{
	register struct ls *lsp;

	lsp = asavep++;
	if ((lsp->ls_name = malloc(strlen(name)+sizeof(char))) == NULL) {
		fprintf(stderr, "ls: out of space for argument names\n");
		exit(1);
	}
	strcpy(lsp->ls_name, name);
	convert(sbp, lsp);
}

/*
 * Convert a stat buffer into an ls store
 * buffer.
 */
convert(sbp, lsp)
register struct stat *sbp;
register struct ls *lsp;
{
	lsp->ls_ino = sbp->st_ino;
	lsp->ls_mode = sbp->st_mode;
	lsp->ls_nlink = sbp->st_nlink;
	lsp->ls_uid = gflag ? sbp->st_gid : sbp->st_uid;
	lsp->ls_size = sbp->st_size;
	lsp->ls_rdev = sbp->st_rdev;
	if (tflag == CTIME)
		lsp->ls_time = sbp->st_ctime;
	else if (tflag == MTIME)
		lsp->ls_time = sbp->st_mtime;
	else if (tflag == ATIME)
		lsp->ls_time = sbp->st_atime;
}

/*
 * Sort, output and free up space from
 * the current directory being considered.
 */
output()
{
	register struct ls *lsp, *lse;
	register unsigned nel;

	nel = savep-saved;
	qsort(saved, nel, sizeof (struct ls), qcomp);
	for (lsp=saved, lse=savep; lsp < lse; lsp++)
		prstuff(lsp->ls_dname, lsp);
	free(saved);
}

/*
 * The following are the two qsort comparison
 * routines -- one for sorting by times, one for
 * sorting by names.  They can be used in both
 * the argument sort and each directory sort.
 */

/*
 * Sort by time (either access, modify, or create setup elsewhere)
 * (forward or backward).
 */
qtcomp(lsp1, lsp2)
register struct ls *lsp1, *lsp2;
{
	register int rval = 0;

	if (lsp1->ls_time < lsp2->ls_time)
		rval++;
	else if (lsp1->ls_time > lsp2->ls_time)
		rval--;
	if (rflag)
		return (-rval);
	return (rval);
}

/*
 * Sort by directory name.
 * (forward or reverse).
 */
qdncomp(lsp1, lsp2)
struct ls *lsp1, *lsp2;
{
	register int rval;

	rval = strncmp(lsp1->ls_dname, lsp2->ls_dname, DIRSIZ);
	if (rflag)
		return (-rval);
	return (rval);
}

/*
 * Sort by full pathname.
 * (forward or reverse).
 */
qncomp(lsp1, lsp2)
struct ls *lsp1, *lsp2;
{
	register int rval;

	rval = strcmp(lsp1->ls_name, lsp2->ls_name);
	if (rflag)
		return (-rval);
	return (rval);
}

usage()
{
	fprintf(stderr, "Usage: ls [-acdfgilrstu] [files ...]\n");
	exit(1);
}
