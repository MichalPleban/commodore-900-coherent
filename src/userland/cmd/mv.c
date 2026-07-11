/*
 * Move (rename) files.
 * mv file1 file2
 *   or
 * mv file ... directory
 */

#include <stdio.h>
#include <stat.h>
#include <errno.h>
#include <access.h>
#include <signal.h>
#include <dir.h>
#define NOTREACHED return

char	*usage = "\
Usage:	mv [-f] file1 file2\n\
	mv [-f] file ... directory\n\
";
char	*nowrite = "unwritable: %s";
char	*nolink = "link %s %s failed";
char	*nounlink = "unlink %s failed";
char	*cmd = "mv: ";		/* for error routines */
int	myuid;
int	mygid;
int	newid;
char	*child;
struct	stat	sb1, sb2, sb3;
int	estat;
int	fflag;
int	interrupted;
char	*concat(),
	*strcpy(),
	*strcat(),
	*strncpy(),
	*getparent(),
	*getchild();

main(argc, argv)
char *argv[];
{
	register int i;

	if (argc>1 && *argv[1]=='-')
		if (argv[1][1] == 'f' && argv[1][2] == '\0') {
			fflag = 1;
			argc--;
			argv++;
		} else {
			fputs(usage, stderr);
			exit(EINVAL);
			NOTREACHED;
		}

	catch(SIGINT);
	catch(SIGHUP);
	signal(SIGQUIT, SIG_IGN);

	myuid = getuid();
	mygid = getgid();

	if (--argc >= 2
	 && stat(argv[argc], &sb2) >= 0
	 && (sb2.st_mode&S_IFMT) == S_IFDIR)
		for (i = 1; i < argc && !interrupted; i += 1) {
			child = getchild(argv[i]);
			mv(argv[i], concat(0, argv[argc], child));
		}
	else if (argc == 2) {
		child = getchild(argv[1]);
		mv(argv[1], argv[2]);
	} else {
		fputs(usage, stderr);
		exit(EINVAL);
		NOTREACHED;
	}
	exit(interrupted ? EINTR : estat);
}

/*
 * move f1 to f2 if at all possible.
 */
mv(f1, f2)
char *f1, *f2;
{
	int isdir, isxdev, nocopy;
	char *p2, *lp2;

	/* Check existence, format, delete permission on source */
	if (stat(f1, &sb1) < 0)
		return (warn(ENOENT, f1));
	nocopy = (sb1.st_mode&S_IFMT) != S_IFREG;
	isdir = (sb1.st_mode&S_IFMT) == S_IFDIR;
	newid = (sb1.st_uid != myuid);
	if (access(getparent(f1), ADEL) < 0)
		return (warn(EACCES, f1));

	/* Check existence, format, create permission on parent of dest */
	if (stat(p2 = getparent(f2), &sb2) < 0)
		return (warn(ENOENT, p2));
	if ((sb2.st_mode&S_IFMT) != S_IFDIR)
		return (warn(ENOTDIR, p2));
	if (access(p2, ADEL) < 0)
		return (warn(EACCES, p2));

	/* Check for cross device mv's */
	isxdev = sb1.st_dev != sb2.st_dev;
	if (isxdev && nocopy)
		return (warn(EXDEV, "%s to %s", f1, f2));

	/* Check for legal directory moves */
	if (isdir) {
		/* Can't mv . or .. */
		if (equals(".", child) || equals("..", child))
			return (warn(-1, "rename %s forbidden", f1));
		/* Can't mv directory to child of itself */
		for (lp2 = p2; ; ) {
			/* If dev and ino of source file appear in the path
			 * from dest parent to root, then the dest
			 * will be a child of the source.
			 */
			if (sb1.st_dev==sb2.st_dev && sb1.st_ino==sb2.st_ino)
				return (warn(-1, "%s parent of %s", f1, f2));
			sb3 = sb2;
			lp2 = concat(1, lp2, "..");
			if (stat(lp2, &sb2) < 0)
				return (warn(errno, "%s", lp2));
			if (sb2.st_dev==sb3.st_dev && sb2.st_ino==sb3.st_ino)
				break;
		}
	}

	/* Check for existence, format, and writability of dest */
	if (stat(f2, &sb2) >= 0) {
		if ((sb2.st_mode&S_IFMT) == S_IFDIR)
			return (warn(EISDIR, f2));
		if (!fflag && access(f2, AWRITE) < 0)
			return (warn(-1, nowrite, f2));

		/* Check for identity of source and dest */
		if (sb1.st_dev == sb2.st_dev && sb1.st_ino == sb2.st_ino)
			return (warn(-1, "%s and %s are identical", f1, f2));

		if (unlink(f2)) {
			fatal(errno, nounlink, f2);
			NOTREACHED;
		}
	}

	/* Do the mv, either cp or ln/rm */
	if (isxdev)
		cp(f1, f2, sb1.st_mode);
	else {
		if (link(f1, f2))
			return (warn(errno, nolink, f1, f2));
		if (isdir) {
			if (unlink(lp2 = concat(1, f2, ".."))) {
				fatal(errno, nounlink, lp2);
				NOTREACHED;
			}
			if (link(p2, lp2)) {
				fatal(errno, nolink, p2, lp2);
				NOTREACHED;
			}
		}
	}
	if (unlink(f1)) {
		fatal(errno, nounlink, f1);
		NOTREACHED;
	}
}

/*
 * Copy f1 to f2.
 * The mode must be maintained.
 */
cp(f1, f2, mode)
char *f1, *f2;
{
	register int fd1, fd2;
	register int n;
	char	iobuf[BUFSIZ];

	if ((fd1 = open(f1, 0)) < 0) {
		fatal(errno, "open %s failed", f1);
		NOTREACHED;
	}
	if ((fd2 = creat(f2, 0)) < 0) {
		fatal(errno, "create %s failed", f2);
		NOTREACHED;
	}
	if (newid)
		mode &= 0777;
	chmod(f2, mode&07777);
	chown(f2, myuid, mygid);
	while ((n = read(fd1, iobuf, BUFSIZ)) != 0) {
		if (n < 0) {
			fatal(errno, "read %s failed", f1);
			NOTREACHED;
		}
		if (n == BUFSIZ && iszero(iobuf)) {
			if (lseek(fd2, (long) BUFSIZ, 1) < 0) {
				fatal(errno, "seek %s failed", f2);
				NOTREACHED;
			}
		} else if (write(fd2, iobuf, n) != n) {
			fatal(errno, "write %s failed", f2);
			NOTREACHED;
		}
	}
	close(fd1);
	close(fd2);
}

/*
 * Check for zeroes in buffer.
 */
iszero(buf)
char *buf;
{
	register int i;
	register char *bp;

	i = BUFSIZ;
	bp = buf;
	do if (*bp++) return (0); while (--i);
	return (1);
}

/*
 * Check two path names for equality knowing that p2 could be '/' terminated.
 */
equals(p1, p2)
register char *p1, *p2;
{
	while (*p1++ == *p2)
		if (*p2++ == '\0')
			return (1);
	if (*p2 == '/' && *--p1 == '\0')
		return (1);
	return (0);
}

/*
 * Concatenate s1 and s2 with a '/' between them.
 */
char *
concat(l, s1, s2)
int l;
register char *s1, *s2;
{
	register char *s3;
	static char *tmp[2];
	int n;

	n = strlen(s1) + strlen(s2) + 2;
	if ((s3 = malloc(n)) == NULL) {
		nomemory();
		NOTREACHED;
	}
	strcpy(s3, s1);
	strcat(s3, "/");
	strcat(s3, s2);
	if (tmp[l] != NULL)
		free(tmp[l]);
	tmp[l] = s3;
	return (s3);
}

warn(err, arg1)
int err;
char *arg1;
{
	fprintf(stderr, "%s%r", cmd, &arg1);
	if (err > 0 && err < sys_nerr)
		fprintf(stderr, " %s", sys_errlist[err]);
	fputs("\n", stderr);
	estat = err;
}

/*
 * return name of parent
 */
char *
getparent(dir)
char	*dir;
{
	register	i;
	register char	*p;
	static char	*par;

	if (par)
		free( par);
	i = strlen( dir);
	par = malloc( i+1);
	if (par == NULL) {
		nomemory( );
		NOTREACHED;
	}
	strcpy( par, dir);

	for (p=par+i; p>par; )
		if (*--p != '/')
			break;
	for (++p; *--p!='/'; )
		if (p == par) {
			*p = '.';
			break;
		}
	*++p = '\0';
	return (par);
}


/*
 * return rightmost component of pathname
 */
char *
getchild(dir)
register char	*dir;
{
	register	i;
	register char	*p;
	static char	ch[DIRSIZ+1];

	i = strlen( dir);
	if (i == 0) {
		fatal( -1, "NULL?");
		NOTREACHED;
	}
	for (p=dir+i; *--p=='/'; )
		if (p == dir) {
			fatal( -1, "don't be silly");
			NOTREACHED;
		}
	while (p > dir)
		if (*--p == '/') {
			++p;
			break;
		}
	return (strncpy( ch, p, DIRSIZ));
}



nomemory( )
{

	fatal( ENOMEM, "out of mem");
	NOTREACHED;
}


onintr( )
{

	signal( SIGINT, SIG_IGN);
	signal( SIGHUP, SIG_IGN);
	++interrupted;
}


catch( sig)
{

	if( signal( sig, SIG_IGN) == SIG_DFL)
		signal( sig, onintr);
}


fatal(err, arg1)
unsigned int err;
char *arg1;
{
	fputs(cmd, stderr);
	if (err < sys_nerr) {
		fputs(sys_errlist[err], stderr);
		fputs(": ", stderr);
	}
	fprintf(stderr, "%r\n", &arg1);
	exit(err);
	NOTREACHED;
}
