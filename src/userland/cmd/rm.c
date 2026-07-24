/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Rec'd from Lauren Weinstein, 7-16-84.
 * rm -- remove files or directories and their contents.
 * This command is setuid to root to allow directory unlinks.
 * Interrupts are handled to prevent mangled directories.
 * Exit status of 1 indicates an error:
 *	couldn't find current directory,
 *	couldn't find root directory,
 *	pathname too long,
 *	couldn't find parent directory,
 *	couldn't stat argument,
 *	directory argument without -r flag,
 *	no permission on directory,
 *	directory is current directory,
 *	directory is root directory,
 *	unlink failed,
 *	couldn't open directory,
 *	couldn't read directory,
 *	ran out of memory saving directory entries.
 * Failure to delete a directory because it is not empty due to interactive
 *	file selection is not an error.
 * All the synonyms for pwd and root are identified by letting the system
 *	map them into stat buffers and comparing device x inode pairs.
 */

#include <stdio.h>
#include <sys/stat.h>
#include <dir.h>
#include <access.h>
#include <signal.h>

#define	NFNAME	1000		/* Largest filename expansion in `-r' option */

char	fname[NFNAME];		/* current argument name */
struct	stat	sb;		/* current argument status buffer */
FILE	*dfp;			/* directory read stream */
char	iobuf[BUFSIZ];		/* stdio buffer */
struct	direct	db;		/* directory entry buffer */
struct	stat	dot_sb;		/* current directory status buffer */
struct	stat	root_sb;	/* root status buffer */
int	interrupted;		/* interrupt flag */
char 	*dot = ".";
char	*dotdot = "..";
char	*root = "/";
char	*cmd = "rm: ";		/* for messages */
char	*save();
extern  int errno;

int	fflag;			/* Force removal */
int	iflag;			/* Interactive removal */
int	rflag;			/* Recursive removal of directory */
int	tflag;			/* Test, do not perform removes */
int	vflag;			/* Verbose report */
int 	ntflag;			/* Non-zero if stdin not a terminal */

main(argc, argv)
char *argv[];
{
	register char *ap;
	register int i;
	register int estat = 0;

	while (argc>1 && *argv[1]=='-') {
		for (ap=&argv[1][1]; *ap != '\0'; ap++)
			switch (*ap) {
			case 'f':
				fflag = 1;
				break;

			case 'i':
				iflag = 1;
				break;

			case 'r':
				rflag = 1;
				break;

			case 't':
				tflag = 1;
				break;

			case 'v':
				vflag = 1;
				break;

			default:
				usage();
			}
		argc--;
		argv++;
	}
	if (argc < 2)
		usage();
	if (!isatty(fileno(stdin))) {
		iflag = 0;
		ntflag = 1;
	}		
	if (stat(dot, &dot_sb)) {
		lerror(dot);
		exit(1);
	}
	if (stat(root, &root_sb)) {
		lerror(root);
		exit(1);
	}

	catch(SIGINT);
	catch(SIGHUP);
	signal(SIGQUIT, SIG_IGN);

	for (i=1; i<argc; i++) {
		if (copy(fname, argv[i], NFNAME-2)) {
			toolong(argv[i]);
			estat = 1;
		} else
			estat |= (remove() == -1);
	}
	exit(estat);
}

/*
 * Remove the entry with name fname.
 * Check for all flags and permissions.
 * Return: -1 for errors; 1 for entries remain; 0 for removal.
 */
remove()
{
	register int isdir;
	register int abortf = 0;
	char c;

	if (stat(fname, &sb))
		return (didnt(NULL));
	if (isdir = ((sb.st_mode & S_IFMT) == S_IFDIR)) {
		if (!rflag)
			return (didnt("%s: directory\n"));
		if (access(fname, ALIST|ADEL|ASRCH)<0)
			return (didnt(NULL));
		if (sb.st_dev == dot_sb.st_dev && sb.st_ino == dot_sb.st_ino)
			return (didnt("%s: current directory\n"));
		if (sb.st_dev == root_sb.st_dev && sb.st_ino == root_sb.st_ino)
			return (didnt("%s: root directory|n"));
	} else if (accparent()<0)
		return (didnt(NULL));
	if (iflag) {
		if (!query("%s? "))
			return (report(1));
	} else if (!fflag && access(fname, AWRITE)<0) {
		if (ntflag)  /* stdin not a terminal? */
		   fprintf(stderr, "%sno write permission", cmd, fname);
		fprintf(stderr, "%soverride protection %o for %s? ", 
		   cmd, (sb.st_mode & 0777), fname);
		if ((c = getchar()) != 'y')
		   abortf++;  /* flag abort */  
		while (c != EOF && c != '\n')  /* flush remaining input */
		   c = getchar();
		if (abortf)   /* abort delete? */
		   return(report(1));   /* yes */
	}
	if (isdir) {
		if (isdir = rmdir())
			return (report(isdir));
	} else {
		if (rmfile() < 0)
			return (didnt(NULL));
	}
	return (report(0));
}

/*
 * Check that the parent of this file has delete permission.
 * Rmdir checks parents of directories.
 */
accparent()
{
	register char *sp;
	register int c;
	register int accpar;

	for (sp = fname; *sp; sp += 1);		/* find end */
	while (sp > fname && sp[-1] != '/')	/* find last / */
		sp -= 1;
	if (sp > fname) {			/* a real name */
		c = *sp;
		*sp = '\0';
		accpar = access(fname, ADEL);
		*sp = c;
	} else					/* simple name */
		accpar = access(dot, ADEL);
	return (accpar);
}

/*
 * Recursive removal of directories.
 * Scan the directory entries and append them to fname successively.
 * Call remove to remove everything except original fname, fname/., and
 * fname/.. which are rmfiled directly.
 */
rmdir()
{
	register char *cp, *np;
	int limit;
	int rmstat = 0;
	char *nbase = NULL, *ntops = NULL;

	/* save directory name */
	cp = fname;
	while (*cp++);
	cp[-1] = '/';
	*cp = '\0';
	limit = NFNAME - 2 - (cp - fname);

	/* get the directory */
	if ((dfp = fopen(fname, "r")) == NULL)
		return (didnt(NULL));
	setbuf(dfp, iobuf);

	/* read and save file names in directory */
	while (fread(&db, sizeof(db), 1, dfp) == 1) {
		if (db.d_ino == 0
		 || equals(db.d_name, dot)
		 || equals(db.d_name, dotdot))
			continue;
		if ((ntops = save(db.d_name)) == NULL) {
			fclose(dfp);
			return (didnt("%s: out of memory\n"));
		}
		if (nbase == NULL)
			nbase = ntops;
	}
	fclose(dfp);

	/* rescan names, form destinations, and remove */
	if (nbase != NULL) {
		for (np = nbase; np <= ntops; ) {
			if (copy(cp, np, limit)) {
				toolong(np);
				rmstat = -1;
				continue;
			}
			switch (remove()) {
			case -1:
				rmstat = -1;
				break;
			case 0:
				break;
			case 1:	
				rmstat = rmstat == -1 ? -1 : 1;
				break;
			default:
				botch("bad return from return");
				break;
			}
			while (*np++);
		}
		forget(nbase);
	}

	/* Now delete the directory, if it's empty */
	if (rmstat) {
		cp[-1] = '\0';
		return (rmstat);
	}
	if (copy(cp, dotdot, limit)) {
		toolong(dotdot);
		cp[-1] = '\0';
		return (-1);
	}
	if (access(fname, ADEL) < 0) {
		lerror(fname);
		cp[-1] = '\0';
		return (-1);
	}
	if (rmfile()
	 || copy(cp, dot, limit)
	 || rmfile()
	 || (cp[-1] = '\0')
	 || rmfile())
		botch("directory unlink error");
	return (rmstat);
}

/*
 * Unlink a single file if tflag is reset.
 */
rmfile()
{
	return (tflag ? 0 : unlink(fname));
}

/*
 * Ask a question about the current file,
 * return one if the answer begins with y or Y.
 */
query(question)
char *question;
{
	register int c;
	register int answer = 0;

	fputs(cmd, stderr);
	fprintf(stderr, question, fname);
	if ((c = getchar())=='y')
		answer = 1;
	while (c!=EOF && c!='\n')
		c = getchar();
	return (answer);
}

/*
 * Report that fname concatenated with the argument string is too long.
 */
toolong(cp)
char *cp;
{
	if (!fflag)
	   fprintf(stderr, "%s%s%s: too long\n", cmd, fname, cp);
}

/*
 * Copy src string to dst observing that no more than lim characters can fit.
 * Clean up on error.
 */
copy(dst, src, lim)
char *dst, *src;
int lim;
{
	register char *dp, *sp, *ep;

	dp = dst;
	sp = src;
	ep = dp + lim;

	while ((dp < ep) && (*dp++ = *sp++));

	if (dp == ep) {
		*dst = '\0';
		return (-1);
	}
	return (0);
}

usage()
{
	fprintf(stderr, "Usage: rm [-frivt] file ...\n");
	exit(1);
}

/*
 * Report reason for failure.
 */
didnt(reason)
char *reason;
{
	if (fflag)
	   return;
	if (reason == NULL)
   	   lerror(fname);
	else
	{  fputs(cmd, stderr);
	   fprintf(stderr, reason, fname);
	}
	return (report(-1));
}

/*
 * Report non-removal/removal and return error code.
 * Since we come here after each file or directory is done,
 *	exit if an interrupt was detected.
 */
report(err)
int err;
{
	if (vflag)
		fprintf(stderr, "%s%s: %sremoved\n", cmd, fname,
		   err ? "not ":"");
	if (interrupted)
		exit(1);
	return (err);
}

onintr()
{
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	++interrupted;
}

catch(sig)
{

	if (signal(sig, SIG_IGN) == SIG_DFL)
		signal(sig, onintr);
}

/*
 * Compare strings for equality.
 */
equals(s1, s2)
register char *s1, *s2;
{
	while (*s1++ == *s2)
		if (*s2++ == '\0')
			return (1);
	return (0);
}

/*
 * Simplified heap.
 */
char *membase = NULL;
char *curbase = NULL;
char *curtops = NULL;

char *
save(name)
register char *name;
{
	register char *saved;
	register int ntosave;
	extern char *sbrk();

	if (membase == NULL)
		membase = curbase = curtops = sbrk(0);

	saved = curbase;
	ntosave = 14;
	do {
		if (saved == curtops) {
			if (saved != sbrk(01000))
				return (NULL);
			curtops += 01000;
		}
	} while (ntosave-- && (*saved++ = *name++));
	if (ntosave < 0)
		*saved++ = '\0';
	name = curbase;
	curbase = saved;
	return (name);
}

forget(names)
register char *names;
{
	if (names < membase || names >= curtops)
		botch("memory deallocation");
	curbase = names;
}

botch(msg)
char *msg;
{
	fprintf(stderr, "%sbotched: %s at %s\n", cmd, msg, fname);
	exit(1);
}

lerror(msg)
char *msg;
{
   register int err;

   err = errno;         /* save error code for perror */
   fputs(cmd, stderr);  /* command name */
   errno = err;         /* restore error code */
   perror(msg);
}
