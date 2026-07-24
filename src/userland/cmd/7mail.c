/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * The mail command.
 * Coherent electronic postal system.
 * (NOTE: this command is written in such a way that
 * it assumed that it is setuid on execution to `root'.
 * All file accession is checked on this basis).
 */

char helpmessage[] = "\
\
mail -- computer mail\n\
Usage:	mail [ options ] [ user ... ]\n\
Options:\n\
	-f file		Print mail from 'file' instead of the default\n\
	-m		Notify each logged in recipient when mail is sent\n\
	-p		Print mail non-interactively\n\
	-q		Exit on interrupt, leaving mail unchanged\n\
	-r		Print mail in reverse order, latest first\n\
If 'user' is present, send each a mail message read from standard input.\n\
Mail message ends with EOF of a line containing only '.'.  Otherwise, read\n\
and print the invoking user's mailbox.\n\
\
";

char	isummary[] = "\
\
Command summary:\n\
	d		Delete current message and print the next message\n\
	m [user ...]	Mail current message to each named 'user'\n\
	p		Print current message again\n\
	q		Quit and update the mailbox\n\
	s [file ...]	Save message in each named 'file'\n\
	t [user ...]	Mail standard input to each named 'user'\n\
	w [file ...]	Save message in each named 'file' without its header\n\
	x		Exit without updating mailbox\n\
	newline		Print the next message\n\
	+		Print the next message\n\
	-		Print the previous message\n\
	EOF		Put undeleted mail back into mailbox and quit\n\
	?		Print this command summary\n\
	!command	Pass 'command' to the shell to execute\n\
If no 'file' is specified, 'mbox' in user's home directory is default.\n\
If no 'user' is specified, the invoking user is default.\n\
\
";

#include <stdio.h>
#include <pwd.h>
#include <utmp.h>
#include <types.h>
#include <access.h>
#include <signal.h>
#include <mdata.h>
#include <stat.h>

#define	SPOOLDIR	"/usr/spool/mail/"
#define	NARGS	64		/* Maximum # args to interactive command */
#define	NLINE	512		/* Longest line in a message */
#define	NCLINE	256		/* Length of an interactive command */

int	mflag;			/* `You have mail.' message to recipient */
int	rflag;			/* Reverse order of print */
int	qflag;			/* Exit after interrrupts */
int	pflag;			/* Print mail */


struct	msg {
	struct msg *m_next;		/* Link to next message */
	struct msg *m_prev;		/* Link to previous message */
	int	m_flag;			/* Flags - non-zero if deleted */
	int	m_hsize;		/* Size of header of message */
	size_t	m_seek;			/* Start position of message */
	size_t	m_end;			/* End of message */
};
struct msg	*m_first = NULL;	/* First message */
struct msg	*m_last = NULL;		/* Last message */

char	iusage[] = "Bad command--type '?' for command summary\n";
char	nombox[] = "No mailbox '%s'.\n";
char	nomail[] = "No mail.\n";
char	noperm[] = "Mailbox '%s' access denied.\n";
char	nosend[] = "Can't send mail to '%s'\n";
char	moerr[] = "Cannot open mailbox '%s'\n";
char	wrerr[] = "Write error on '%s'\n";
char	toerr[] = "Cannot create temporary file\n";
char	nosave[] = "Cannot save letter in '%s'\n";

FILE	*mfp;				/* Mailbox stream */
int	myuid;				/* User-id of mail user */
int	mygid;				/* Group-id of mail user */
char	myname[25];			/* User name */
char	mymbox[256];			/* $HOME/mbox */
char	mydead[256];			/* $HOME/dead.letter */
char	spoolname[50] = SPOOLDIR;
char	*mailbox = spoolname;

char	*args[NARGS];			/* Interactive command arglist */
char	msgline[NLINE];
char	cline[NCLINE] = "+\n";

char	*temp;				/* Currently open temp file */
char	templ[] = "/tmp/mailXXXXXX";	/* Temp file name template */

size_t	ftell();
char	*getlogin();
char	*mktemp();
char	*index();
char	*parent();
int	catchintr();
char	*malloc();

main(argc, argv)
char *argv[];
{
	register char *ap;

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, catchintr);
	if (argc>1 && *argv[1]=='-') {
		while (argc>1 && *argv[1]=='-') {
			for (ap=&argv[1][1]; *ap != '\0'; ap++)
				switch (*ap) {
				case 'f':
					if (argc < 3)
						usage();
					mailbox = argv[2];
					argv++;
					argc--;
					break;

				case 'm':
					mflag++;
					break;

				case 'p':
					pflag++;
					break;

				case 'q':
					qflag++;
					break;

				case 'r':
					rflag++;
					break;

				default:
					usage();
				}
			argv++;
			argc--;
		}
	}
	setname();
	if (argc > 1) {
		qflag = 1;
		send(stdin, &argv[1], (size_t)0, (size_t)MAXLONG);
	} else {
		commands();
	}
	rmexit(0);
}

/*
 * Setup all the identities for the current user.
 */
setname()
{
	register struct passwd *pwp;
	register char *np;

	if ((np = getlogin()) == NULL) {
		myuid = getuid();
		if ((pwp = getpwuid(myuid)) == NULL)
			merr("Who are you?\n");
		np = pwp->pw_name;
	} else {
		if ((pwp = getpwnam(np)) != NULL)
			myuid = pwp->pw_uid;
	}
	mygid = pwp->pw_gid;
	strcat(spoolname, np);
	strcpy(myname, np);
	strcpy(mymbox, pwp->pw_dir);
	strcat(mymbox, "/mbox");
	strcpy(mydead, pwp->pw_dir);
	strcat(mydead, "/dead.letter");
	mktemp(templ);
}

/*
 * Send the message found on
 * the file pointer to the list
 * of people (argv style) with
 * a NULL pointer at the end.
 * The message is copied to a temp-file
 * from position `start' to `end' (or EOF).
 */
send(fp, users, start, end)
FILE *fp;
register char **users;
size_t start, end;
{
	register char **ulist;
	register char *cp;
	register struct passwd *pwp;
	register int senderr = 0;
	time_t curtime;
	register int fromtty;
	char boxname[256];
	FILE *tfp, *xfp;

	temp = templ;
	if ((tfp = fopen(temp, "w")) != NULL) {
		fclose(tfp);
		if ((tfp = fopen(temp, "r+w")) == NULL)
			merr(toerr);
	} else
		merr(toerr);
	fseek(fp, start, 0);
	end -= start;
	for (;;) {
		if (fgets(msgline, sizeof msgline, fp) == NULL
		 || (fp == stdin && strcmp(".\n", msgline) == 0))
			break;
		if (strncmp("From ", msgline, 5) == 0)
			putc('>', tfp);
		fputs(msgline, tfp);
		if ((end -= strlen(msgline)) <= 0)
			break;
	}
	if (ftell(tfp) == 0 || intcheck()) {
		fclose(tfp);
		unlink(temp);
		temp = NULL;
		return;
	}
	if (msgline[0] != '\n')
		putc('\n', tfp);
	fromtty = isatty(fileno(fp));
	for (ulist = users; *ulist!=NULL; ulist++) {
		rewind(tfp);
		if ((cp = index(*ulist, '!')) != NULL) {
			*cp++ = '\0';
			if (rsend(*ulist, cp, tfp))
				senderr = 1;
			continue;
		}
		sprintf(boxname, "%s%s", SPOOLDIR, *ulist);
		if ((pwp = getpwnam(*ulist)) == NULL) {
			mmsg(nosend, *ulist);
			senderr = 1;
			continue;
		}
		mlock(pwp->pw_uid);
		if ((xfp = fopen(boxname, "a")) == NULL) {
			mmsg(nosend, *ulist);
			senderr = 1;
			munlock();
			continue;
		}
		chown(boxname, pwp->pw_uid, pwp->pw_gid);
		time(&curtime);
		fprintf(xfp, "From %s %s", myname, ctime(&curtime));
		if (users[0]!=NULL && users[1]!=NULL) {
			register char **upp;

			fprintf(xfp, "(cc:");
			for (upp = users; *upp != NULL; upp++)
				fprintf(xfp, " %s", *upp);
			fprintf(xfp, ")\n");
		}
		if (mcopy(tfp, xfp, (size_t)0, (size_t)MAXLONG)) {
			merr(wrerr, boxname);
			senderr = 1;
		}
		fclose(xfp);
		munlock();
		advise(*ulist);
	}
	if (senderr && fromtty) {
		if (maccess(mydead) < 0
		 || (xfp = fopen(mydead, "a")) == NULL
		 || mcopy(tfp, xfp, (size_t)0, (size_t)MAXLONG))
			mmsg(nosave, mydead);
		else
			mmsg("Letter saved in %s\n", mydead);
		if (xfp != NULL) {
			chown(mydead, myuid, mygid);
			fclose(xfp);
		}
	}
	fclose(tfp);
	unlink(temp);
	temp = NULL;
}

/*
 * Send a message to `user' on remote `system'
 * from the temp-file described by the stream
 * `fp' (which is rewound).
 */
rsend(system, user, fp)
char *system;
char *user;
FILE *fp;
{
	mmsg("Cannot send to '%s' ", user);
	mmsg("on remote system '%s\n", system);
	fp = NULL;
	return (1);
}

/*
 * If the `-m' option is specified, advise
 * the recipient of the presence of mail.
 */
advise(recipient)
char *recipient;
{
	register FILE *fp;
	register FILE *tfp;
	struct utmp ut;
	char tty[30];
	struct stat sb;

	if (!mflag)
		return;
	if ((fp = fopen("/etc/utmp", "r")) == NULL)
		return;
	while (fread(&ut, sizeof ut, 1, fp) == 1)
		if (strncmp(ut.ut_name, recipient, DIRSIZ) == 0) {
			sprintf(tty, "/dev/%s", ut.ut_line);
			if (stat(tty, &sb)<0 || (sb.st_mode&S_IEXEC)==0)
				continue;
			if ((tfp = fopen(tty, "w")) != NULL) {
				fprintf(tfp, "\7%s: you have mail.\n", myname);
				fclose(tfp);
			}
		}
	fclose(fp);
}

/*
 * Check access on a file.
 */
maccess(name)
char *name;
{
	struct stat sb;

	if (stat(name, &sb) < 0) {
		if (access(parent(name), ACREAT) < 0)
			return (-1);
	} else if (access(name, AWRITE) < 0)
		return (-1);
	return (0);
}

/*
 * Find the parent directory for access permissions.
 */
char *
parent(name)
char *name;
{
	register char *cp, *xp;
	static char p[256];
	char *rindex();

	xp = rindex(name, '/');
	if (xp == NULL)
		return (".");
	if (xp == name)
		return ("/");
	if (xp - name >= 256)
		return ("");
	cp = p;
	while (name < xp)
		*cp++ = *name++;
	*cp = 0;
	return (p);
}

/*
 * Copy from the file stream `ifp' (starting at
 * position `start' and ending at `end' or EOF)
 * to the file stream `ofp' which is assumed
 * to be already correctly positioned.
 * Returns non-zero on errors.
 */
mcopy(ifp, ofp, start, end)
register FILE *ifp, *ofp;
size_t start, end;
{
	register int c;

	fseek(ifp, start, 0);
	end -= start;
	while (end-- > 0  &&  (c = getc(ifp))!=EOF)
		putc(c, ofp);
	fflush(ofp);
	if (ferror(ofp))
		return (1);
	return (0);
}

/*
 * Process mail's interactive commands
 * for reading/deleting/saving mail.
 */
commands()
{
	register struct msg *mp;
	struct msg *dest;
	register char **fnp;
	register FILE *fp;
	size_t seek;

	readmail();
	mprint(mp = rflag ? m_last : m_first);
	for (;;) {
		readmail();
		intcheck();
		if ( ! pflag) {
			mmsg("? ");
			if (fgets(cline, sizeof cline, stdin) == NULL) {
				if (intcheck())
					continue;
				break;
			}
		}
		switch (cline[0]) {
		case 'd':
			if (cline[1] != '\n')
				goto usage;
			mp->m_flag += 1;
			goto advance;

		case 'm':
		case 't':
			if (csplit(cline, args) == 1) {
				args[1] = myname;
				args[2] = NULL;
			}
			if (cline[0] == 'm')
				send(mfp, args+1, mp->m_seek, mp->m_end);
			else
				send(stdin, args+1, 0L, (size_t)MAXLONG);
			break;

		case 'p':
			if (cline[1] != '\n')
				goto usage;
			if (mprint(mp))
				break;
			goto advance;

		case 'q':
			if (cline[1] != '\n')
				goto usage;
			mquit();
			/* NOTREACHED */

		case 's':
		case 'w':
			if (csplit(cline, args) == 1) {
				args[1] = mymbox;
				args[2] = NULL;
			}
			seek = mp->m_seek;
			if (cline[0] == 'w')
				seek += mp->m_hsize;
			for (fnp = &args[1]; *fnp != NULL; fnp++) {
				fp = NULL;
				if (maccess(*fnp) < 0
				 || (fp = fopen(*fnp, "a")) == NULL
				 || mcopy(mfp, fp, seek, mp->m_end))
					mmsg(nosave, *fnp);
				if (fp != NULL) {
					fclose(fp);
					chown(*fnp, myuid, mygid);
				}
			}
			break;

		case 'x':
			if (cline[1] != '\n')
				goto usage;
			rmexit(0);
			/* NOTREACHED */

		case '!':
			if (system(cline+1) == 127)
				mmsg("Try again\n");
			else
				mmsg("!\n");
			break;

		case '?':
			mmsg(isummary);
			break;

		case '-':
			if (cline[1] != '\n')
				goto usage;
			dest = rflag ? m_last : m_first;
			goto nextmsg;

		case '+':
			if (cline[1] != '\n')
				goto usage;
		case '\n':
		advance:
			dest = rflag ? m_first : m_last;
		nextmsg:
			do {
				if (mp == dest) {
					if (pflag)
						return;
					mmsg("No more messages.\n");
					break;
				}
				mp = (dest==m_last) ? mp->m_next : mp->m_prev;
			} while (mprint(mp) == 0);
			break;

		default:
		usage:
			mmsg(iusage);
			break;
		}
	}
	putc('\n', stderr);
	mquit();
}

/*
 * Read through the mail-box file
 * constructing list of letters.
 * On subsequent calls, append any additional mail
 * and notify the user.
 */
readmail()
{
	register struct msg *mp, *lmp;
	struct stat sb;

	if (m_first == NULL) {
		if (stat(mailbox, &sb) < 0)
			merr(nombox, mailbox);
		if (sb.st_size == 0)
			merr(nomail);
		if (access(mailbox, AREAD) < 0)
			merr(noperm, mailbox);
		if ((mfp = fopen(mailbox, "r")) == NULL)
			merr(moerr, mailbox);
		mp = lmp = NULL;
	} else {
		fstat(fileno(mfp), &sb);
		if (sb.st_size == m_last->m_end)
			return;
		mmsg("More mail received.\n");
		mp = lmp = m_last;
		fseek(mfp, mp->m_end, 0);
	}
	mlock(myuid);
	while (fgets(msgline, sizeof msgline, mfp) != NULL) {
		 if (strncmp("From ", msgline, 5) == 0) {
			mp = (struct msg *)malloc(sizeof(*mp));
			mp->m_prev = NULL;
			mp->m_next = NULL;
			mp->m_flag = 0;
			mp->m_hsize = strlen(msgline);
			mp->m_seek = ftell(mfp)-mp->m_hsize;
			if (lmp == NULL) {
				m_first = mp;
			} else {
				lmp->m_next = mp;
				lmp->m_end = mp->m_seek;
			}
			mp->m_prev = lmp;
			m_last = lmp = mp;
		}
	}
	if (mp == NULL)
		merr("Not mailbox format '%s'\n", mailbox);
	mp->m_end = ftell(mfp);
	munlock();
}

/*
 * Split a command line up into
 * argv (passed) and argc (returned).
 */
int
csplit(command, args)
char *command;
char **args;
{
	register char *cp;
	register char **ap;
	register int c;

	cp = command;
	ap = args;
	for (;;) {
		while ((c = *cp)==' ' || c=='\t')
			*cp++ = '\0';
		if (*cp == '\n')
			*cp = '\0';
		if (*cp == '\0')
			break;
		*ap++ = cp;
		while ((c = *cp)!=' ' && c!='\t' && c!='\n' && c!='\0')
			cp++;
	}
	*ap = NULL;
	return (ap - args);
}

/*
 * Lock the appropriately-numbered mailbox
 * (numbered by user-number) from multiple
 * accesses. There is a (small) race here
 * which will be overlooked for now.
 */
char	*lockname;

mlock(uid)
int uid;
{
	register int fd;
	static char lock[32];

	lockname = lock;
	sprintf(lock, "/tmp/maillock%d", uid);
	while (access(lockname, 0) == 0)
		sleep(1);
	if ((fd = creat(lockname, 0)) >= 0)
		close(fd);
}

/*
 * Unlock the currently-set lock (by `mlock')
 * also called from rmexit.
 */
munlock()
{
	if (lockname != NULL)
		unlink(lockname);
	lockname = NULL;
}

/*
 * Exit, after rewriting the
 * mailbox
 */
mquit()
{
	register struct msg *mp;
	register FILE *nfp;
	struct stat sb;

	readmail();
	mlock(myuid);
	if (mailbox != spoolname && maccess(mailbox) < 0)
		merr(noperm, mailbox);
	fstat(fileno(mfp), &sb);
	signal(SIGINT, SIG_IGN);
	unlink(mailbox);
	if ((nfp = fopen(mailbox, "w")) == NULL)
		merr("Cannot re-write '%s'\n", mailbox);
	chown(mailbox, sb.st_uid, sb.st_gid);
	chmod(mailbox, sb.st_mode&0777);
	for (mp = m_first; mp != NULL; mp = mp->m_next)
		if (mp->m_flag == 0
		 && mcopy(mfp, nfp, mp->m_seek, mp->m_end))
			merr(wrerr, mailbox);
	fclose(nfp);
	fclose(mfp);
	munlock();
	rmexit(0);
}

/*
 * Print the current message, given by
 * the message pointer (`mp').
 */
mprint(mp)
register struct msg *mp;
{
	if (mp->m_flag)
		return 0;
	mcopy(mfp, stdout, mp->m_seek, mp->m_end);
	return (1);
}

/*
 * Errors, usage, and exit removing
 * any tempfiles left around.
 */
mmsg(x, s)
char *x, *s;
{
	fprintf(stderr, x, s);
}

merr(x, s)
char *x, *s;
{
	mmsg(x, s);
	rmexit(1);
}

usage()
{
	mmsg(helpmessage);
	rmexit(1);
}

rmexit(s)
int s;
{
	if (temp != NULL)
		unlink(temp);
	munlock();
	exit(s);
}

/*
 * Catch interrupts, taking the
 * appropriate action based on
 * the `-q' option.
 */
int	intflag;		/* On when interrupt sent */

catchintr()
{
	signal(SIGINT, SIG_IGN);
	if (qflag)
		rmexit(1);
	intflag = 1;
	signal(SIGINT, catchintr);
}

intcheck()
{
	if (intflag) {
		intflag = 0;
		putc('\n', stderr);
		return (1);
	}
	return (0);
}

/*
 * Call the system to execute a command line
 * which is passed as an argument.
 * Change the user id and group id.
 */
system(line)
char *line;
{
	int status, pid;
	register wpid;
	register int (*intfun)(), (*quitfun)();

	if ((pid = fork()) < 0)
		return (-1);
	if (pid == 0) {		/* Child */
		setuid(myuid);
		setgid(mygid);
		execl("/bin/sh", "sh", "-c", line, NULL);
		exit(0177);
	}
	intfun = signal(SIGINT, SIG_IGN);
	quitfun = signal(SIGQUIT, SIG_IGN);
	while ((wpid = wait(&status))!=pid && wpid>=0)
		;
	if (wpid < 0)
		status = wpid;
	signal(SIGINT, intfun);
	signal(SIGQUIT, quitfun);
	return (status);
}
