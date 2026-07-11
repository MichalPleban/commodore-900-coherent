/*
 * Init.
 */
#include <dir.h>
#include <signal.h>
#include <utmp.h>

/*
 * Miscellaneous constants.
 */
#define	NULL	((char *)0)

/*
 * Structure containing information about each terminal.
 */
struct tty {
	struct	 tty *t_next;		/* Pointer to next entry */
	int	 t_pid;			/* Process id */
	int	 t_flag;		/* Flag */
	char	 t_baud[2];		/* Baud descriptor */
	char	 t_tty[5+DIRSIZ+1];	/* tty name */
};

/*
 * Enviroment list for the shell.
 */
char *envl[] ={
	"PATH=:/bin",
	"HOME=/etc",
	"PS1=# ",
	"PS2=> ",
	NULL
};

/*
 * Functions.
 */
extern	int	sighang();
extern	int	sigquit();
extern	struct	tty *findtty();

/*
 * Variables.
 */
struct	tty *ttyp;			/* Terminal list */
int	sinflag;			/* Go to single user */
int	ttyflag;			/* Scan tty file */

main(argc, argv)
register int argc;
char *argv[];
{
	register struct tty *tp;
	register int n;
	unsigned status;

	if ((n = creat("/etc/boottime", 0644)) >= 0)
		close(n);
	if (argc >= 2)
		loadswp(argv[1]);
	for (n=2; n<argc; n++)
		loaddrv(argv[n]);
	putwtmp("~", "");
	signal(SIGHUP, sighang);
	signal(SIGQUIT, sigquit);
	for (;;) {
		while (attendc() == 0) {
			n = spawn("/dev/console",
				"/bin/sh", "-sh", NULL);
			waitc(n);
			if (sinflag) {
				sinflag = 0;
				kill(-1, 9);
				continue;
			}
			if (access("/etc/rc", 0) == 0) {
				n = spawn("/dev/null",
					"/bin/sh", "sh", "/etc/rc", NULL);
				waitc(n);
			}
			scantty();
		}
		n = wait(&status);
		if (sinflag) {
			sinflag = 0;
			for (tp=ttyp; tp!=NULL; tp=tp->t_next)
				tp->t_flag = 0;
			kill(-1, 9);
		}
		if (ttyflag) {
			ttyflag = 0;
			scantty();
			n = 0;
		}
		if (n > 0) {
			for (tp=ttyp; tp; tp=tp->t_next) {
				if (n != tp->t_pid)
					continue;
				tp->t_pid = 0;
				putwtmp(&tp->t_tty[5], "");
				clrutmp(&tp->t_tty[5]);
				chmod(tp->t_tty, 0700);
				chown(tp->t_tty, 0, 1);
				if ((status>>8) == 0377)
					tp->t_flag = 0;
			}
		}
	}
}

/*
 * Called when we get a hangup.
 */
sighang()
{
	signal(SIGHUP, sighang);
	sinflag = 1;
}

/*
 * Called when a quit is received.
 */
sigquit()
{
	signal(SIGQUIT, sigquit);
	ttyflag = 1;
}

/*
 * Load the swapper.
 */
loadswp(np)
char *np;
{
	if (np[0] == '\0')
		return;
	if (fork() != 0)
		return;
	execve(np, NULL, NULL);
	panic("Cannot load ", np);
}

/*
 * Load the given driver.
 */
loaddrv(np)
char *np;
{
	register int pid;
	int status;

	pid = spawn("/dev/null", "/etc/load", "load", np, NULL);
	while (wait(&status) != pid)
		;
	if (status != 0)
		exit(status);
}

/*
 * If there are any terminals which need to be serviced, service them.
 * Return the number of active terminals.
*/
attendc()
{
	register struct tty *tp;
	register int n;

	n = 0;
	for (tp=ttyp; tp!=NULL; tp=tp->t_next) {
		if (tp->t_pid == 0) {
			if (tp->t_flag == 0)
				continue;
			else
				login(tp);
		}
		n++;
	}
	return (n);
}

/*
 * Wait for the given process to complete.
 */
waitc(p1)
register int p1;
{
	register int p2;

	while ((p2=wait(NULL))>=0 && p2!=p1)
		;
}

/*
 * Scan the tty file.
 */
scantty()
{
	register struct tty *tp;
	register int fd;
	register int nflag;
	struct tty tty;
	extern char *malloc();

	if ((fd=open("/etc/ttys", 0)) < 0)
		return;
	while (readtty(&tty, fd) != 0) {
		if ((tp=findtty(&tty)) == NULL) {
			tp = malloc(sizeof(*tp));
			*tp = tty;
			tp->t_next = ttyp;
			ttyp = tp;
			continue;
		}
		nflag = 0;
		if (tp->t_flag != tty.t_flag) {
			nflag = 1;
			tp->t_flag = tty.t_flag;
		}
		if (tp->t_baud[0] != tty.t_baud[0]) {
			nflag = 1;
			tp->t_baud[0] = tty.t_baud[0];
		}
		if (nflag!=0 && tp->t_pid!=0)
			kill(tp->t_pid, 9);
	}
	close(fd);
}

/*
 * Read a line from the terminal file and save the appropriate fields in
 * the terminal structure.
 */
readtty(tp, fd)
register struct tty *tp;
{
	register char *lp;
	char c[1];
	char line[2+DIRSIZ+1];

	lp = line;
	for (;;) {
		if (read(fd, c, sizeof(c)) != sizeof(c))
			return (0);
		if (c[0] == '\n')
			break;
		if (lp < &line[2+DIRSIZ])
			*lp++ = c[0];
	}
	*lp++ = '\0';
	if (lp < &line[2])
		return (0);
	lp = line;
	tp->t_flag = *lp++ - '0';
	tp->t_pid = 0;
	tp->t_baud[0] = *lp++;
	tp->t_baud[1] = '\0';
	strcpy(tp->t_tty, "/dev/");
	strncpy(&tp->t_tty[5], lp, DIRSIZ);
	tp->t_tty[5+DIRSIZ] = '\0';
	return (1);
}

/*
 * The body of this function, the tail of readtty() above, and the head of
 * login() below were lost to disc corruption in the recovered source and
 * have been reconstructed from disasm/init.asm.
 * Find the terminal-list entry whose device name matches np's.
 */
struct tty *
findtty(np)
register struct tty *np;
{
	register struct tty *tp;

	for (tp=ttyp; tp!=NULL; tp=tp->t_next)
		if (strcmp(np->t_tty, tp->t_tty) == 0)
			return (tp);
	return (NULL);
}

/*
 * Start a getty running on the given terminal.
 */
login(tp)
register struct tty *tp;
{
	register int pid;

	pid = spawn(tp->t_tty, "/etc/getty", "-", tp->t_baud, NULL);
	if (pid < 0)
		tp->t_flag = 0;
	else
		tp->t_pid = pid;
}

/*
 * Spawn off a command.
 */
spawn(tp, np, ap)
char *tp;
char *np;
char *ap;
{
	register int pid;
	register int fd;

	if ((pid=fork()) != 0)
		return (pid);
	if ((fd=open(tp, 2)) < 0)
		panic("Cannot open ", tp, NULL);
	dup2(0, 1);
	dup2(0, 2);
	execve(np, &ap, envl);
	panic("Cannot execute ", np, NULL);
	return (pid);
}

/*
 * Write an entry onto the wtmp file.
 */
putwtmp(lp, np)
{
	register int fd;
	struct utmp utmp;
	extern time_t time();

	if ((fd=open("/usr/adm/wtmp", 1)) < 0)
		return;
	strncpy(utmp.ut_line, lp, 8);
	strncpy(utmp.ut_name, np, DIRSIZ);
	utmp.ut_time = time(NULL);
	lseek(fd, 0L, 2);
	write(fd, (char *)&utmp, sizeof(utmp));
	close(fd);
}

/*
 * Clear out a utmp entry.
 */
clrutmp(tty)
char *tty;
{
	register int fd;
	struct utmp utmp;
	static struct utmp ctmp;

	if ((fd=open("/etc/utmp", 2)) < 0)
		return;
	while (read(fd, &utmp, sizeof(utmp)) == sizeof(utmp)) {
		if (strncmp(utmp.ut_line, tty, 8) != 0)
			continue;
		lseek(fd, (long)-sizeof(utmp), 1);
		write(fd, &ctmp, sizeof(ctmp));
		break;
	}
	close(fd);
}

/*
 * Print out a list of error messages and exit.
 */
panic(cp)
char *cp;
{
	register char **cpp;

	close(0);
	open("/dev/console", 2);
	for (cpp=&cp; *cpp!=NULL; cpp++)
		printl(*cpp);
	printl("\n");
	exit(0377);
}

/*
 * Print out a string on the standard output.
 */
printl(cp1)
register char *cp1;
{
	register char *cp2;

	for (cp2=cp1; *cp2; cp2++)
		;
	write(0, cp1, cp2-cp1);
}
