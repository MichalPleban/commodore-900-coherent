/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Learn - Computer Aided Computer Instruction
 * Script Interpreter.
 * [NOTE: This command should be marked as
 * setuid to root -- i.e. the owner of
 * the directory `/usr/learn/play']
 */

#include <stdio.h>
#include <sys/stat.h>
#include <sgtty.h>
#include <pwd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#define	MAXLINE	120
#define	NCPS	30
#define	NPAGE	20		/* Line per page (#print) */
#define	NFNAME	60		/* Number of chars in a filename */
#define	NSTREAM	10		/* Number of maximum streams for #next */
#define	INSEP	0177		/* Input separator character */
#define	INCROK	1		/* Speed increment for correct lesson */
#define	INCRNOK	(-4)		/* Speed increment for incorrect lesson */
#define	HISPEED	10		/* Largest speed rating allowed */
#define	YES	0		/* User's answer of `yes' */
#define	NO	1		/* User's answer of `no' */
#define	NOTHING	(-1)		/* No answer at all yet */
#define	RIGHT	1		/* Right answer */
#define	WRONG	0		/* Wrong answer */

extern	char	*ctime();
extern	char	*mktemp();
char	*tutorial();
int	gexit();
int	interrupt();
struct passwd	*getpwuid();
int	xprint();
int	xcreate();
int	xuser();
int	xcpin();
int	xuncpin();
int	xcpout();
int	xuncpout();
int	xpipe();
int	xunpipe();
int	xcmp();
int	xmatch();
int	xbad();
int	xsucceed();
int	xfail();
int	xlog();
int	xnext();
int	xend();
int	xnegate();

struct	keytab {
	char	*k_name;
	int	(*k_func)();
}	keytab[] = {
	"print", xprint,
	"create", xcreate,
	"user", xuser,
	"copyin", xcpin,
	"uncopyin", xuncpin,
	"copyout", xcpout,
	"uncopyout", xuncpout,
	"pipe", xpipe,
	"unpipe", xunpipe,
	"cmp", xcmp,
	"match", xmatch,
	"bad", xbad,
	"succeed", xsucceed,
	"fail", xfail,
	"log", xlog,
	"next", xnext,
	"end", xend,
	"negate", xnegate,
	NULL, NULL,
};

/*
 * This is the structure for threading lessons
 * with #next.
 */
struct lestab {
	char	l_name[NCPS];		/* Name of lesson */
	char	l_seen;			/* Already been used? */
	char	l_speed;		/* Speed rating for this lesson */
}	lestab[2][NSTREAM] = {
	"0", 1
};

struct lestab	*currles = &lestab[1][0]; /* Current lesson */
struct lestab	*prevles = &lestab[0][0]; /* Current lesson */
int	nexti;			/* Index for #next */

int	testflg = 0;		/* Test out script */
int	lineno = 0;
int	oln;			/* Line number for pagination of #print */
int	speed = 0;		/* Student's speed rating */
int	answer;
int	result;			/* result of lesson */
int	iflag;			/* Ignore input until next control line */
int	nflag;			/* Reading next lesson values */
int	pflag;			/* For #print from script */
int	rptflag;		/* Repeating lesson */
FILE	*cpif;			/* copy file for user's input */
FILE	*cpof;			/* copy for for user's output */
FILE	*of;			/* Output file */
FILE	*pf;			/* #pipe output file */
FILE	*sf;			/* Script file */
FILE	*logfp;			/* Default logging file */
int	cpofd;		/* Saved stdout */
char	user[NCPS];		/* User-id */
char	playarea[] = "uXXXXXX";
char	more[] = "[Type `RETURN' to continue..]";
char	erase[] = "\r                              \r";
char	date[30];		/* Current date */
char	*lesson;		 /* Current lesson name */
char	line[MAXLINE];
char	uline[MAXLINE];		/* User's input line */
char	*lastline;		/* Last user's input line */
char	prompt[] = "$ ";
char	snull[] = "";
char	sorry[] =
	"The last lesson was incorrect.  Would you like to try it again?";
char	*script;		/* Script in learndir */

char	cpifile[] = ".copy";
char	cpofile[] = ".ocopy";
char	*learndir = "/usr/learn";

main(argc, argv)
int argc;
char *argv[];
{

	signal(SIGHUP, gexit);
	signal(SIGINT, interrupt);
	while (argc>1 && (*argv[1]=='-' || *argv[1]=='+')) {
		if (*argv[1] == '+')
			testflg = 1;
		else
			learndir = &argv[1][1];
		argc--;
		argv++;
	}
	if (argc > 4)
		usage();
	if (argc > 3) {
		speed = atoi(argv[3]);
		if (speed>HISPEED || speed<0) {
			speed = HISPEED;
			fprintf(stderr, "learn: speed set to %d\n", HISPEED);
		}
	}
	lesson = script = NULL;
	if (argc > 2)
		lesson = argv[2];
	if (argc > 1)
		script = argv[1]; else
		script = tutorial();
	setup(script, lesson);
	driver();
	/* NOTREACHED */
}

/*
 * Tutorial version of learn.
 * This code is invoked when the user
 * just types: `learn'.
 * It then lists all of the available
 * subjects and asks the user to pick
 * one.
 */
char *
tutorial()
{
	register char *cp;
	register int c;
	register FILE *hfp;
	static char subj[50];
	struct stat sb;

	sprintf(line, "%s/lib/subjects", learndir);
	if ((hfp = fopen(line, "r")) != NULL) {
		while (fgets(line, MAXLINE, hfp) != NULL)
			fputs(line, stdout);
		fclose(hfp);
	}
	for (;;) {
		printf("\nEnter subject ? ");
		cp = subj;
		while (cp < &subj[50-1]) {
			if ((c = getchar()) == EOF)
				gexit(1);
			if (c == '\n')
				break;
			*cp++ = c;
		}
		*cp = '\0';
		if (cp == &subj[0])
			gexit(1);
		while (c != '\n')
			c = getchar();
		sprintf(line, "%s/lib/%s", learndir, subj);
		if (stat(line, &sb)>=0 && (sb.st_mode&S_IFMT)==S_IFDIR)
			break;
		printf("No lessons about `%s', try again.\n", subj);
	}
	return (subj);
}

/*
 * driver code for learn
 */
driver()
{
	lopen();
	for (;;) {
		dolesson();
		if (rptflag == 0)
			score();
		else
			rptflag = 0;
		if (result == RIGHT) {
			printf("Good.  Lesson %s", lesson);
			printf(" (%d)", speed);
			printf("\n\n");
			nextlesson();
			lopen();
		} else if (result == WRONG) {
			printf(sorry);
			for (;;) {
				if (fgets(line, MAXLINE, stdin) == NULL)
					gexit(1);
				if (match(line, "bye")) {
					printf("Good bye.\n");
					gexit(0);
				} else if (match(line, "[yY]*")) {
					printf("Try the problem again.\n");
					rewind(sf);
					rptflag = 1;
					break;
				} else {
					nextlesson();
					lopen();
					break;
				}
			}
		} else {
			nextlesson();
			lopen();
		}
	}
}

/*
 * nextlesson tries to select a new lesson.  If the result
 * was RIGHT it goes to the next lesson selected by #next.
 * Otherwise, it is looking for another lesson to try at that
 * point (of another speed), and if none, moves on to
 * the next lesson anyway.
 */
nextlesson()
{
	register struct lestab *lp, *slp, *tp;
	register diff;
	int d;
	int i;
	int pass;

	pass = 0;
	if (result != WRONG) {
	nomatch:
		tp = prevles;
		for (i=0; i<NSTREAM; i++, tp++) {
			tp->l_name[0] = '\0';
			tp->l_seen = 0;
		}
		tp = prevles;
		prevles = currles;
		currles = tp;
	}
	lp = prevles;
	slp = NULL;
	while (lp->l_name[0] && lp-prevles < NSTREAM) {
		if (!lp->l_seen) {
			d = (d = lp->l_speed-speed) >= 0  ?  d  :  -d;
			if (d < diff) {
				diff = d;
				slp = lp;
			}
		}
		lp++;
	}
	if (slp != NULL) {
		slp->l_seen = 1;
		lesson = slp->l_name;
	} else if (result==WRONG && pass++ == 0)
		goto nomatch;
	else
		gexit(0);
}

usage()
{
	fprintf(stderr,
	    "Usage: learn [-directory] [subject [lesson [speed]]]\n");
	exit(1);
}

diag(args)
{
	fprintf(stderr, "%d: ", lineno);
	fprintf(stderr, "%r", &args);
	fprintf(stderr, "\n");
}

/*
 * For each lesson, this is called to process the
 * script which is found on the file pointer `sf'.
 */
dolesson()
{
	register char *lp;

	lineno = 0;
	iflag = nflag = pflag = 0;
	result = NOTHING;
	lastline = NULL;
	uclose(&cpif);
	while (fgets(lp=line, MAXLINE, sf) != NULL) {
		lineno++;
		if (*lp == '#') {
			control(lp+1);
			continue;
		}
		if (*lp == '\\')
			lp++;
		if (nflag)
			next(lp);
		else if (pflag)
			output(lp);
		else if (!iflag)
			result = toshell(lp);
	}
	if (pf != NULL)
		unbal("#pipe");
	if (cpif != NULL)
		unbal("#copyin");
	if (cpofd > 0)
		unbal("#copyout");
}

/*
 * Process control lines in learn
 * scripts.
 */
control(s)
register char *s;
{
	register char *cp;
	register struct keytab *kp;
	char keyword[NCPS];

	uclose(&of);
	pflag = iflag = nflag = 0;
	while (*s==' ' || *s=='\t')
		s++;
	for (cp=keyword; *s!='\n' && *s!=' ' && *s!='\t' && *s; s++)
		if (cp < &keyword[NCPS-1])
			*cp++ = *s;
	*cp = '\0';
	cp = keyword;
	for (kp=keytab; kp->k_name!=NULL; kp++)
		if (streq(kp->k_name, cp)) {
			(*kp->k_func)(s);
			return;
		}
	diag("Illegal script control `#%s'", cp);
}

/*
 * Report a warning about one
 * of the three possible bracketins
 * commands being unbalanced.
 */
unbal(type)
char *type;
{
	fprintf(stderr, "Lesson %s: nonterminated %s command\n", lesson, type);
}

/*
 * Each line describing the next lesson in the graph is
 * passed here.
 */
next(s)
register char *s;
{
	register struct lestab *lp;
	register char *cp;

	if (nexti >= NSTREAM) {
		diag("Too many #next streams");
		return;
	}
	lp = &currles[nexti++];
	while (*s==' ' || *s=='\t')
		s++;
	if (*s=='\0' || *s=='\n') {
		diag("#next syntax error");
		return;
	}
	for (cp=lp->l_name; *s!=' ' && *s!='\t' && *s!='\n' && *s; s++)
		if (cp < &lp->l_name[NCPS-1])
			*cp++ = *s;
	*cp = '\0';
	if (cp == lp->l_name) {
		diag("Null #next lesson");
		return;
	}
	while (*s==' ' || *s=='\t')
		s++;
	lp->l_speed = atoi(s);
}

/*
 * The following are the control functions
 * implementing script `#' directive lines.
 */

/*
 * #print directive
 */
xprint(s)
register char *s;
{
	register c;
	register char *cp;
	FILE *pfp;
	char fname[NFNAME];

	while (*s==' ' || *s=='\t')
		s++;
	for (cp=fname; *s!=' ' && *s!='\t' && *s!='\n' && *s; s++)
		if (cp < &fname[NFNAME-1])
			*cp++ = *s;
	*cp = '\0';
	if (rptflag)
		iflag = 1;
	else if (cp == fname) {
		pflag = 1;
		of = stdout;
	} else if ((pfp = fopen(fname, "r")) == NULL)
		diag("Cannot open #print file %s", fname);
	else {
		while ((c = getc(pfp)) != EOF)
			putchar(c);
		fclose(pfp);
	}
}

/*
 * #create - create file containing contents
 * of script until next control (`#') line.
 */
xcreate(s)
register char *s;
{
	register char *cp;
	char fname[NFNAME];

	while (*s==' ' || *s=='\t')
		s++;
	for (cp=fname; *s!=' ' && *s!='\t' && *s!='\n' && *s; s++)
		if (cp < &fname[NFNAME-1])
			*cp++ = *s;
	*cp = '\0';
	if ((of = fopen(fname, "w")) == NULL) {
		diag("Cannot create %s", fname);
		iflag++;
	}
	pflag = 1;
}

/*
 * #user - take input from user's terminal
 */
xuser(s)
char *s;
{
	register char *cp, *wp;
	char word[NCPS];

	for (;;) {
		moutput(prompt);
		oln = 0;
		if (fgets(uline, MAXLINE, stdin) == NULL)
			gexit(1);
		if (cpif != NULL)
			fprintf(cpif, "%s", uline);
		lastline = uline;
		for (cp=uline; *cp==' ' || *cp=='\t'; )
			cp++;
		wp = word;
		while (*cp!=' ' && *cp!='\t' && *cp!='\n' && *cp) {
			if (wp < &word[NCPS-1])
				*wp++ = *cp;
			cp++;
		}
		*wp = '\0';
		if (streq(word, "answer")) {
			lastline = cp;
			break;
		} else if (streq(word, "yes")) {
			answer = YES;
			break;
		} else if (streq(word, "no")) {
			answer = NO;
			break;
		} else if (streq(word, "bye")) {
			printf("Good bye.\n");
			gexit(0);
		} else if (streq(word, "ready"))
			break;
		else
			toshell(uline);
	}
	result = NOTHING;
}

/*
 * #copyin - initiate redirection of user's input to
 * `cpifile'.
 */
xcpin(s)
char *s;
{
	uclose(&cpif);
	if ((cpif = fopen(cpifile, "w")) == NULL)
		diag("#copyin file create failure");
}

/*
 * #uncopyin - terminate redirection of user's input
 * to `cpifile'.
 */
xuncpin(s)
char *s;
{
	uclose(&cpif);
}

xcpout(s)
char *s;
{
	int pdes[2], pid;
	FILE *ifp;
	register int c;
	register int ignoring = 0;

	if (pipe(pdes)<0 || (pid=fork())<0)
		diag("#copyout failure");
	if (pid) {		/* parent */
		/*
		 * Save stdout and
		 * change to pipe
		 */
		close(pdes[0]);
		cpofd = dup(fileno(stdout));
		dup2(pdes[1], fileno(stdout));
		close(pdes[1]);
	} else {		/* child */
		close(pdes[1]);
		if ((cpof = fopen(cpofile, "w")) == NULL) {
			diag("#copyout failure");
			exit(1);
		}
		ifp = fdopen(pdes[0], "r");
		while ((c = getc(ifp)) != EOF) {
			if (c == INSEP) {
				ignoring ^= 01;
				continue;
			}
			putc(c, stderr);
			if (!ignoring)
				putc(c, cpof);
		}
		exit(0);
	}
}

/*
 * #uncopyout - wait for child to get all the data
 * and restore stdout to original value.
 */
xuncpout(s)
char *s;
{
	int status;

	if (cpofd > 0) {
		dup2(cpofd, fileno(stdout));
		close(cpofd);
		cpofd = 0;
	} else
		diag("Not in #copyout");
	wait(&status);
}

xpipe(s)
char *s;
{
	int pdes[2], pid;

	if (pipe(pdes)<0 || (pid = fork())<0)
		diag("#pipe failure");
	if (pid) {		/* parent */
		pf = fdopen(pdes[1], "w");
		close(pdes[0]);
	} else {		/* child */
		close(pdes[1]);
		if (dup2(pdes[0], 0) != 0)
			diag("#pipe dup error");
		close(pdes[0]);
		execl("/bin/sh", "learnsh", NULL);
		diag("#pipe cannot exec shell");
	}
}

xunpipe(s)
char *s;
{
	int status;

	if (pf != NULL) {
		fclose(pf);
		pf = NULL;
		wait(&status);
	}
}

/*
 * Compare two files.
 * #cmp file1 file2
 * #cmp file1
 *	this uses the script file until next # line
 */
xcmp(s)
register char *s;
{
	register char *cp;
	register c;
	FILE *f1, *f2;
	char fname[NFNAME];

	while (*s==' ' || *s=='\t')
		s++;
	for (cp=fname; *s!=' ' && *s!='\t' && *s!='\n' && *s; s++)
		if (cp < &fname[NFNAME-1])
			*cp++ = *s;
	*cp = '\0';
	if ((f1 = fopen(fname, "r")) == NULL)
		diag("Cannot open %s in #cmp", fname);
	while (*s==' ' || *s=='\t')
		s++;
	for (cp=fname; *s!=' ' && *s!='\t' && *s!='\n' && *s; s++)
		if (cp < &fname[NFNAME-1])
			*cp++ = *s;
	*cp = '\0';
	if (cp == fname) {
		register int nlf = 0;

		if (f1==NULL) {
			iflag++;
			return;
		}
		result = RIGHT;
		for (;;) {
			if ((c = getc(sf))=='#' && nlf) {
				ungetc(c, sf);
				c = EOF;
			}
			nlf = 0;
			if (c == '\n')
				nlf++;
			if (c != getc(f1))
				result = WRONG;
			if (c == EOF)
				break;
		}
	} else {
		if ((f2 = fopen(fname, "r")) == NULL)
			diag("Cannot open %s in #cmp", fname);
		if (f1!=NULL && f2!=NULL) {
			if (result == NOTHING)
				result = RIGHT;
			while ((c = getc(f1)) != EOF)
				if (c != getc(f2))
					result = WRONG;
		}
		uclose(&f2);
	}
	uclose(&f1);
}

/*
 * #match - if string matches last input, set result to RIGHT.
 * Also, print out text if right.
 */
xmatch(s)
register char *s;
{
	while (*s==' ' || *s=='\t')
		s++;
	if (lastline == NULL)
		lastline = snull;
	if (match(lastline, s)) {
		pflag = 1;
		of = stdout;
		result = RIGHT;
	} else
		iflag = 1;
}

/*
 * #bad - opposite of #match - checks for wrong answers and
 * will print helpful hints.
 */
xbad(s)
register char *s;
{
	while (*s==' ' || *s=='\t')
		s++;
	if (lastline == NULL)
		lastline = snull;
	if (match(lastline, s)) {
		pflag = 1;
		of = stdout;
		result = WRONG;
	} else
		iflag = 1;
}

/*
 * #succeed - print message on success.
 */
xsucceed(s)
char *s;
{
	if (result == RIGHT) {
		pflag = 1;
		of = stdout;
	} else
		iflag = 1;
}

/*
 * #fail - print message upon failure in lesson
 */
xfail(s)
char *s;
{
	if (result != RIGHT) {
		pflag = 1;
		of = stdout;
	} else
		iflag = 1;
}

/*
 * #log - write out a logging entry
 * showing the status of this lesson.
 */
xlog(s)
register char *s;
{
	register char *cp;
	char fname[NFNAME];
	FILE *lf;

	while (*s==' ' || *s=='\t')
		s++;
	for (cp=fname; *s!=' ' && *s!='\t' && *s!='\n' && *s; s++)
		if (cp < &fname[NFNAME-1])
			*cp++ = *s;
	*cp = '\0';
	if (cp == fname)
		lf = logfp;
	else
		lf = fopen(fname, "a");
	if (lf == NULL) {
		if (lf == logfp)
			diag("cannot open default log file"); else
			diag("Cannot open log file `%s'", fname);
		return;
	}
	fprintf(lf, "%s:\t%s\t%s\tspeed=%d\t%s\n", lesson, user,
	    result==RIGHT ? "right" : "wrong", speed, date);
	if (lf != logfp)
		fclose(lf);
}

/*
 * #next - selection of next lessons
 * -- fills up current lesson structure
 * which `nextlesson' will use if right
 * answer.
 */
xnext(s)
char *s;
{
	register struct lestab *lp;
	register i;


	lp = currles;
	for (i=0; i<NSTREAM; i++, lp++) {
		lp->l_name[0] = '\0';
		lp->l_seen = 0;
	}
	nflag++;
	nexti = 0;
}

/*
 * Do nothing.  This is to terminate text
 * for example in #print or #create so a
 * command can be inserted.
 */
xend(s)
/* ARGSUSED */
char *s;
{
}

/*
 * Negate the result that has been produced
 * up to now.  If no result has been found,
 * don't do anything.
 */
/* ARGSUSED */
xnegate(s)
char *s;
{
	if (result == WRONG)
		result = RIGHT;
	else if (result == RIGHT)
		result = WRONG;
}

/*
 * Change speed score, dependent upon the
 * `result' of the lesson.
 */
score()
{
	if (result == RIGHT)
		speed += INCROK; else
		speed += INCRNOK;
	if (speed > 10)
		speed = HISPEED;
	if (speed < 0)
		speed = 0;
}

/*
 * Conditionally close given unit, making the file
 * pointer null for future reference.
 */
uclose(fpp)
register FILE **fpp;
{
	if (*fpp != NULL) {
		if (*fpp != stdout)
			fclose(*fpp);
		*fpp = NULL;
	}
}

/*
 * Upon an interrupt either leave or resume
 * where we left off.
 */
interrupt()
{
	signal(SIGINT, SIG_IGN);
	if (question("Do you really want to leave LEARN"))
		gexit(1);
	else
		signal(SIGINT, interrupt);
}


/*
 * Ask a question and return 1 if true, 0 otherwise.
 */
question(s)
char *s;
{
	register int c, c1;

	printf("%s ?", s);
	for (;;) {
		c1 = c = getchar();
		while (c1!=EOF && c1!='\n')
			c1 = getchar();
		if (c=='y' || c=='Y')
			return (1);
		else if (c=='n' || c=='N')
			return (0);
		printf("`yes' or `no' ? ");
	}
}

/*
 * Graceful exit from learn
 */
gexit(s)
int s;
{
	int status;

	if (pf != NULL) {
		fclose(pf);
		wait(&status);
	}
	printf("\n");
	if (user[0] != '\0')
		rmdir();
	exit(s);
}

/*
 * Remove user-directory as
 * part of cleaning up.
 * Chdir("/") necessary as we cannot remove
 * our current directory (limitation in rmdir).
 */
rmdir()
{
	int pid;
	int status;
	char fname[NFNAME];

	chdir("/");
	if ((pid = fork()) < 0)
		diag("Cannot cleanup");
	else if (pid)
		wait(&status);
	else {
		close(0);
		close(1);
		close(2);
		open("/dev/null", 2);
		dup(0);
		dup(0);
		sprintf(fname, "%s/play/%s", learndir, playarea);
		execl("/bin/rm", "rm", "-rf", fname, NULL);
		cerr("cannot cleanup");
	}
}

/*
 * Pass string (`s') to shell as a command
 * line to be executed.
 */
toshell(s)
char *s;
{
	register char *cp;

	if (pf != NULL) {
		fprintf(pf, "%s", s);
		fflush(pf);
		return (NOTHING);
	} else {
		for (cp = s; *cp != '\0'; cp++)
			if (*cp == '\n')
				*cp = '\0';
		if (*s == '\0')
			return (RIGHT);
		return (system(s) ? WRONG : RIGHT);
	}
}

/*
 * Output a line to file or terminal.
 * If it is the terminal (stdout) paginate
 * the output.
 */
output(s)
register char *s;
{
	static struct sgttyb sgb;
	static int sgflag;

	if (of == NULL)
		return;
	if (of == stdout)
		if (++oln > NPAGE) {
			fputs(more, stdout);
			if (sgflag == 0) {
				ioctl(fileno(stdout), TIOCGETP, &sgb);
				sgflag = sgb.sg_flags;
			}
			sgb.sg_flags &= ~ECHO;
			ioctl(fileno(stdout), TIOCSETN, &sgb);
			while (getchar() != '\n')
				;
			sgb.sg_flags = sgflag;
			ioctl(fileno(stdout), TIOCSETN, &sgb);
			fputs(erase, stdout);
			oln = 0;
		}
	fputs(s, of);
}

/*
 * Output message.  If in #copyout, have
 * to surround this message with `INSEP'
 * so it gets stripped by the #uncopyout processor.
 * If in #pipe, just return and print no prompt.
 */
/* VARARGS */
moutput(x)
{
	if (pf != NULL)
		return;
	if (cpofd > 0)
		printf("%c%r%c", INSEP, &x, INSEP); else
		printf("%r", &x);
}

/*
 * Initial setup code
 * Passed script name (directory) for lopen's benefit.
 * Unless lesson is non-NULL, all of the script
 * is setup.
 * During this code, the play area is created and
 * we lose our setuid privileges once this is done.
 */
setup(sn, ln)
register char *sn, *ln;
{
	extern int errno;
	char fname[NFNAME];
	register char *cp;
	register struct passwd *pwp;
	short int uid, gid;
	long xtime;

	uid = getuid();
	gid = getgid();
	if ((pwp = getpwuid(uid)) == NULL)
		pwp->pw_name = "Guess who?";
	strcpy(user, pwp->pw_name);
	if (chdir(learndir)<0 || chdir("play")<0)
		cerr("bad learn directory structure");
	if (mkdir(mktemp(playarea, 0777))<0)
		cerr("can't create user area (learn probably not setuid)");
	chown(playarea, uid, gid);
	chdir(playarea);
	time(&xtime);
	cp = ctime(&xtime);
	cp[24] = '\0';
	strcpy(date, cp);
	script = sn;
	if (ln == NULL) {
		printf("If you left off in the middle, type in the lesson\n");
		printf("number (e.g. 2.3) that learn last typed out.\n");
		printf("To start at the beginning just type RETURN:\n");
		fgets(line, MAXLINE, stdin);
		for (cp=line; *cp!='\0'; cp++)
			if (*cp == '\n')
				*cp = '\0';
		for (cp=line; *cp==' ' || *cp=='\t'; cp++)
			;
		if (*cp != '\0')
			ln = cp;
	}
	if (ln != NULL) {
		for (cp=prevles->l_name; *ln; ln++)
			if (cp < &prevles->l_name[NCPS-1])
				*cp++ = *ln;
		*cp = '\0';
	}
	lesson = prevles->l_name;
	sprintf(fname, "%s/log/%s", learndir, script);
	logfp = fopen(fname, "a");
	/*
	 * Turn off our setuid
	 * privileges here.
	 */
	setuid(uid);
}

/*
 * Match string to pattern.  Pattern possibly contains
 * glob-type metacharacters.
 */
match(s, p)
register char *s, *p;
{
	register char *cp;

	for (cp = s; *cp==' ' || *cp=='\t'; cp++)
		;
	for (s = cp; *cp!='\n' && *cp!='\0'; cp++)
		;
	*cp = '\0';
	for (cp = p; *cp!='\n' && *cp!='\0'; cp++)
		;
	*cp = '\0';
	return (pnmatch(s, p, 0));
}

/*
 * Open lesson script file
 */
lopen()
{
	char fname[NFNAME];
	static notfirst;

	sprintf(fname, "%s/lib/%s/L%s", learndir, script, lesson);
	uclose(&sf);
	if ((sf = fopen(fname, "r")) == NULL) {
		if (notfirst)
			cerr("script error--lesson `%s' not found", lesson);
		else
			cerr("There are no lessons about `%s'", script);
	}
	if (!notfirst)
		notfirst = 1;
}

/*
 * Kludge until sys mkdir comes back.
 */
mkdir(fn, mode)
char *fn;
int mode;
{
	extern int errno;
	register int pid;
	int status;

	if ((pid = fork()) < 0)
		return (-1);
	if (pid) {
		while (wait(&status) >= 0)
			;
		if (status) {
			errno = status & 0377;
			return (-1);
		}
		return (0);
	}
	close(0);
	close(1);
	close(2);
	open("/dev/null", 2);
	dup(0);
	dup(0);
	execl("/bin/mkdir", "mkdir", fn, NULL);
	exit(1);
}

/*
 * Error output
 */
/* VARARGS */
cerr(x)
{
	fprintf(stderr, "learn: %r\n", &x);
	exit(1);
}
