/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Simple xmail command. Almost identical to mail(1).
 * /bin/xmail must be setuid to root. This is regarded as a failing.
 * If users are given xmail scans the directory /usr/spool/pubkey for their
 * public key files to find out if they are enrolled.
 * Note that because of the protection bits on /usr/spool/pubkey
 * that this is possible only because xmail is setuid to root.
 */
#include <stdio.h>
#include <ctype.h>
#include <timeb.h>
#include <pwd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#define	and	&&
#define	or	||
#define	not	!
#define	TRUE	(0==0)
#define	FALSE	(not TRUE)

#define MMSIZE	2048			/* Maximum message size */
#define	LINESIZ	256			/* Line length for fgetline() */

/*
 * Pkfile will be the full pathname of each recipients public knapsack key.
 * Pkhook is the location in pkfile where we append the recipient's name.
 */
char	pkfile[] = "/usr/spool/pubkey/xxxxxxxxxxxxxx";
char	*pkhook = &pkfile[18];

char	message[MMSIZE];		/* Message buffer */
char	*mp;				/* Pointer to message end */

/*
 * Functions.
 */
char	*str_cpy();
char	*concat();
char	*getlogin();
char	*gethome();
char	*getmyname();
FILE	*xread();
FILE	*xopen();
struct	passwd *getpwnam();
struct	passwd *getpwuid();

main(ac, av)
int ac;
register char *av[];
{
	register int valid;
	register char **bv;

	ac = 0;
	signal(SIGPIPE, SIG_IGN);
	if (*++av == NULL) {
		readmail('m');			/* 'm' for 'maybe' */
		return;
	}
	if (av[1] == NULL  and  av[0][1] == '-')
		switch (av[0][1]) {
		case 'n':
			readmail('n');
			return;
		case 'y':
			readmail('y');
			return;
		case 'x':
			readmail('x');
			return;
		default:
			readmail('m');
			return;
		}

	/*
	 * Check if all recipients are enrolled. Mark those who aren't (by
	 * marking their positions in av[] with the address of pkfile).
	 * Count the number of valid recipients.
	 */
	valid = 0;
	for (bv = av; *bv != NULL; ++bv) {
		register int n;
		str_cpy(pkhook, *bv);
		if ((n = open(pkfile, 0)) >= 0) {
			close(n);
			++valid;
			continue;
		}
		fputs(*bv, stderr);
		fputs(" is not enrolled.\n", stderr);
		*bv = pkfile;		/* pkfile is used as a flag here. */
	}

	/*
	 * If there aren't any valid recipients, die.
	 */
	if (valid == 0)
		exit(1);

	/*
	 * Prepare the header.
	 */
	mp = message;
	header();
	if (valid > 1) {
		mp = str_cpy(mp, "(cc:");
		for (bv = av; *bv != NULL; ++bv) {
			if (*bv == pkfile)
				continue;
			*mp++ = ' ';
			mp = str_cpy(mp, *bv);
		}
		mp = str_cpy(mp, ")\n");
	}

	/*
	 * Read the message and send it to each valid recipient.
	 */
	readmsg();
	for (bv = av; *bv != NULL; ++bv) {
		if (*bv == pkfile)
			continue;
		sendmail(*bv);
	}

	/*
	 * Wait for all our children (xencode processes).
	 */
	wait((int *)0);
}

/*
 * Return my login name.
 */
char *
getmyname()
{
	register struct passwd *pwp;
	register char *ret;

	if ((ret = getlogin()) == NULL) {
		if ((pwp = getpwuid(getuid())) == NULL)
			panic("Who are you?");
		ret = pwp->pw_name;
	}
	return (ret);
}

/*
 * Get the home directory of user. The data is overwritten each call.
 */
char *
gethome(user)
char *user;
{
	register struct passwd *pwp;

	if ((pwp = getpwnam(user)) == NULL)
		return (NULL);
	return (pwp->pw_dir);
}

/*
 * Print out any mail we have.  If `sflag' is  'n' we don't save mail, if 'y'
 * we save the encrypted mail in xmbox, if 'x' we save the clear mail in mbox,
 * otherwise we ask. Any other reply causes no action.
 */
readmail(sflag)
int sflag;
{
	register char *home;
	register char *mailbox;
	register char *mbox;
	register FILE *x;

	if ((home = gethome(getmyname())) == NULL)
		panic("Can't find $HOME");
	mailbox = concat(home, "xmailbox");

	x = xread(mailbox);

	if (sflag == 'm') {
		fputs("\nSave ?", stdout);
		fgetline(message, LINESIZ, stdin);
		sflag = message[0];
	}

	switch (sflag) {
	case 'x':
		if (x == NULL)
			panic("Cannot save clear text, no action taken");
		mbox = concat(home, "mbox");
		move(mailbox, x);
		goto APPEND;
	case 'y':
		mbox = concat(home, "xmbox");
	APPEND:	append(mailbox, mbox);
		unlink(mbox);
		link(mailbox, mbox);
		unlink(mailbox);
		break;
	case 'n':
		unlink(mailbox);
		break;
	default:
		break;
	}
	wait((int *)0);
	return;
}

/*
 * Open and unlink rathole. This provides a safe place to store cleartext -
 * no one can get at it. Read xmail from mailbox via xdecode filter, display
 * and copy it to rathole. Return the (FILE *) of rathole.
 */
FILE *
xread(mailbox)
char *mailbox;
{
	register FILE *ret, *u;
	register int n;
	static char rathole[] = "/tmp/xmxxxxxx";
	struct stat stbuf;

	mktemp(rathole);
	ret = fopen(rathole, "wr");
	unlink(rathole);

	if (stat(mailbox, &stbuf) < 0
	or  stbuf.st_size == (size_t)0
	or  (u = xopen(mailbox, 0, NULL)) == NULL)
		panic("No xmail");

	while ((n = fgetline(message, LINESIZ, u)) != 0) {
		fwrite(message, 1, n, stdout);
		fwrite(message, 1, n, ret);
	}
	fclose(u);
	return (ret);
}

/*
 * Put the header on the message.
 */
header()
{
	struct timeb timeb;

	ftime(&timeb);
	mp = str_cpy(mp, "\nFrom ");
	mp = str_cpy(mp, getmyname());
	*mp++ = ' ';
	mp = str_cpy(mp, ctime(&timeb.time));
	/* The ctime() string has a newline on the end already. */
}

/*
 * Read the message the user wants to send and store it in message.
 */
readmsg()
{
	register int n;

	while ((n = fgetline(mp, LINESIZ, stdin)) != 0) {
		if (mp[0] == '.'  and  mp[1] == '\n')
			break;
		mp += n;
	}
	return;
}

/*
 * Send the message to the given user. Unlike regular mail, we do not put
 * undeliverable mail in "dead.letter".
 */
sendmail(user)
register char *user;
{
	register FILE *u;
	register char *cp;

	if ((cp = gethome(user)) == NULL) {
		fputs("Cannot send to ", stderr);
		fputs(user, stderr);
		putc('\n', stderr);
		return;
	}
	cp = concat(cp, "xmailbox");
	if ((u = xopen(cp, 1, user)) != NULL) {
		if (fwrite(message, 1, mp - message, u) != 0)
			notify(cp);
		fclose(u);
	}
	free(cp);
	return;
}

/*
 * Send notification of xmail to the recipient's regular mailbox. Cp is assumed
 * to be "$HOME/xmailbox", which we transform into "$HOME/mailbox".
 */
notify(cp)
register char *cp;
{
	register char *cp0;
	register FILE *fp;
	static char msg[] = "\nFrom xmail:\nYou have xmail.\n";

	cp0 = &cp[strlen(cp) - 8];	/* Address of 'x' in 'x'mailbox. */
	while ((*cp0 = cp0[1]) != '\0')
		++cp0;
	fp = fopen(cp, "a");
	fwrite(msg, 1, strlen(msg), fp);
	fclose(fp);
	return;
}

/*
 * Copy b to a. Return pointer to the '\0' char terminating the result.
 */
char *
str_cpy(a, b)
register char *a, *b;
{
	while (*a++ = *b++)
		;
	return (a - 1);
}

/*
 * Concatenate a and b. Return a pointer to the malloced result.
 */
char *
concat(a, b)
register char *a;
char *b;
{
	register char *ret;
	char	*malloc();

	ret = malloc(strlen(a) + strlen(b) + 2);
	if (ret == NULL)
		panic("Out of memory");
	a = str_cpy(ret, a);
	*a++ = '/';
	str_cpy(a, b);
	return (ret);
}

/*
 * Print out an error message and exit.
 */
panic(cp)
char *cp;
{
	fputs(cp, stderr);
	putc('\n', stderr);
	exit(1);
}

/*
 * Move the contents of the file pointed to by 'f2' to 'file'. Assume f2 is 
 * open for reading and writing. The contents of 'file' are overwritten.
 */
move(file, f2)
char *file;
register FILE *f2;
{
	register FILE *f1;
	register int n;

	if ((f1 = fopen(file, "w")) == NULL)
		return;
	rewind(f2);
	while ((n = fread(message, 1, MMSIZE, f2)) != 0)
		fwrite(message, 1, n, f1);
	fclose(f1);
	return;
}

/*
 * Append the contents of file2 to file1.
 */
append(file1, file2)
char *file1, *file2;
{
	register FILE *f1, *f2;
	register int n;

	if ((f1 = fopen(file1, "a")) == NULL)
		return;
	if ((f2 = fopen(file2, "r")) == NULL) {
		fclose(f1);
		return;
	}
	while ((n = fread(message, 1, MMSIZE, f2)) != 0)
		fwrite(message, 1, n, f1);
	fclose(f1);
	fclose(f2);
	return;
}

/*
 * Xopen reads from (mode 0) or appends to (mode 1) file. The io is filtered:
 * For reading, the returned (FILE *) is a pipe from the filter xdecode, for
 * writing it's a pipe to the filter "xencode user".
 */
FILE *
xopen(file, mode, user)
char *file;
register int mode;
char *user;
{
	int pfildes[2];
	char *cmd;
	int n;
	register FILE *fp;
	register int *pp = pfildes;

	if ((fp = fopen(file, (mode == 0 ? "r" : "a"))) == NULL)
		return (fp);
	if (pipe(pp) < 0)
		panic("Cannot make pipe");
	if ((n = fork()) < 0)
		panic("Cannot fork");

	/*
	 * Parent.
	 */
	if (n > 0) {
		fclose(fp);
		if (mode == 0) {
			close(pp[1]);
			return (fdopen(pp[0], "r"));
		}
		else {
			close(pp[0]);
			return (fdopen(pp[1], "w"));
		}
	}

	/*
	 * Now we are in the child.
	 */
	if (mode == 0) {
		cmd = "/bin/xdecode";
		dup2(pp[1], 1);
		dup2(fileno(fp), 0);
	}
	else {
		cmd = "/bin/xencode";
		dup2(pp[0], 0);
		dup2(fileno(fp), 1);
	}
	close(pp[0]);
	close(pp[1]);
	fclose(fp);
	if (mode == 0)
		execl(cmd, cmd, NULL);
	else
		execl(cmd, cmd, user, NULL);
	exit(1);
}

/*
 * fgets() isn't quite what we want, so we have our own. Fgetline() reads
 * from fp until a newline, an EOF or until 'lim' characters have been read.
 * Characters read are placed in buf. The number of chars read is returned.
 * The string is null-terminated. This means buf should have room for lim + 1
 * characters. Once EOF is reached fgetline() continually returns 0.
 */
fgetline(buf, lim, fp)
char *buf;
int lim;
register FILE *fp;
{
	register int c;
	register char *cp = buf;

	while ((c = *cp++ = getc(fp)) != '\n') {
		if (c == EOF) {
			ungetc(c, fp);
			--cp;
			break;
		}
		if (--lim == 0)
			break;
	}
	*cp = '\0';
	return (cp - buf);
}

