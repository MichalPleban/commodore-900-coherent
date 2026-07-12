/*		       
 * Rec'd from Luaren Weinstein, 7-16-84.
 * login [-q] [username]
 *
 * This version of login stty's the terminal back to the default sgttyb
 * and tchars settings unless we were exec'd from getty.
 *
 * Compile -s -n -i
 */
#include <stdio.h>
#include <pwd.h>
#include <signal.h>
#include <utmp.h>
#include <dir.h>
#include <sys/stat.h>
#include <sgtty.h>
#include <sys/tty.h>
#include <chars.h>

#define FALSE	0
#define TRUE 	1

#define	NNAME	64
#define	NPAGE	20		/* Most news to print - really ioctl parm. */
#define	TTYMODE	(S_IREAD|S_IWRITE|S_IEXEC)

#define MAXFAIL	3		/* Maximum permitted failed login attempts */
#define MAXTIME 60		/* Maximum seconds permitted for login */
#define PASSLEN 13		/* Length of encrypted passwords */
#define ACCNAME "remacc"	/* Remote access password dummy username */

/*
 * Default sgtty and tchars settings.
 */

struct	sgttyb	sgttyd = {		/* Initial sgtty */
	B9600,		 /* input speed */
	B9600,  	 /* output speed */
	'\b',		 /* erase */
	CTRLU,		 /* kill */
	EVENP|ODDP|CRMOD|ECHO|XTABS|CRT  /* flags */
};
	
struct	tchars	tchars = {		/* Initial tchars */
	CTRLC,		 /* int */
	FS,		 /* quit */
	CTRLQ,		 /* start */
	CTRLS,	         /* stop */
	CTRLD,	         /* eof */
	-1		 /* brkc */
};

int	fflag;
int	iflag;
int	qflag;
int	nline;			/* Lines output */
char	nb[NNAME];
char 	logcount = 0;		/* Login attempt count */
char 	remote = FALSE;		/* Assume not remote tty line */

struct	stat	sb;
struct  sgttyb  sgp;		/* tty structure */
int 	timeout();		

char	*env[] = {
	"PATH=:/bin",
	"PS1=% ",
	0,
	0
};

struct	news {
	size_t	n_seek;
	time_t	n_time;
}	news;

char	longnews[] =
	"[Too much opening news -- please see \"%s\" at your leisure]\n";
char	mailname[50] = "/usr/spool/mail/";
char	newslog[] = "/etc/newslog";
char	motdf[] = "/etc/motd";
char	newsf[] = "/etc/news";
char 	faillog[] = "/usr/adm/failed";  /* failed login attempt log */
char    goodlog[] = "/usr/adm/wtmp";    /* successful login log */       
char	pass1[] = "Password: ";		/* password msg 1 */
char	pass2[] = "Remote access password: ";  /* password msg 2 */

main(argc, argv)
char *argv[];
{
	register char *cp, *np;
	register struct passwd *pwp;
	int c, i, oldtime;
	char passcount = 0;
	char *tp, failed;
	char *crypt(), *getpass(), *ttyname();
	char s_name[NNAME];	/* saved name */
	int s_uid;		/* saved uid */
	int s_gid;		/* saved gid */
	char s_dir[128];	/* saved dir */
	char s_shell[128];	/* saved shell */
	char shellbuff[128] = "exec ";  /* shell exec line buffer */

	resetsigs();
	/*
	 * Called from /etc/getty?
	 */
	if (argc!=0 && argv[0][0]=='-')
		fflag = 1;
	else if (argc == 1)
		slexit(0);
	if ((tp = ttyname(2)) == NULL) {
		fprintf(stderr, "Sorry. Cannot figure out terminal name!\n");
		slexit(1);
	}

	if (settty(2)) {
		perror(tp);
		slexit(1);
	}

	gtty(1, &sgp);			/* get tty status */
	if (isremote(tp))		/* Isolate remote determination */
	{  remote = TRUE;
	   signal(SIGALRM, &timeout);   /* catch login timeout */
	   alarm(MAXTIME);  		/* set timeout alarm */
	}

again:
	failed = TRUE;  /* assume attempt will fail */
	if (remote && (++logcount > MAXFAIL))  /* count remote attempts */
	   slexit(1);  /* exit (and try force hangup) if too many attempts */

	i = 1;
	while (i<argc && argv[i][0]=='-') {
		cp = &argv[i++][1];
		while (c = *cp++) 
		{	switch (c) {
			case 'q':
				++qflag;
				break;

			default:
				usage();
			}
		}
	}
	if (passcount == 0)  /* first pass? */
	{  if (i < argc) 
           {  np = argv[i];
	      argc  = 0;
	      fflag = 0;
    	   } 
	   else 
  	   {  np = nb;
	      do {
	         printf("Coherent login: ");
	         if (fgets(nb, NNAME-1, stdin) == NULL)  
		 {  putchar('\n');
		    slexit(1);
	         }
	      } while (nb[0] == '\n' && nb[1] == 0);
	      nb[strlen(nb)-1] = 0;  /* null terminate over newline */
 	   }
	   setpwent();	        /* make sure password file is rewound */
	   pwp = getpwnam(np);  /* get name entry from password file */
	   if (pwp != NULL)	/* if entry found */
	   {  strcpy(s_name, pwp->pw_name);    /* save name */
	      s_uid = pwp->pw_uid;             /* save uid */
	      s_gid = pwp->pw_gid;             /* save gid */	
	      strcpy(s_dir, pwp->pw_dir);      /* save working dir */
	      strcpy(s_shell, pwp->pw_shell);  /* save shell */
	   }
	}
 	else
	{  setpwent();    /* rewind password file */
	   pwp = getpwnam(ACCNAME);  /* check for remote access entry */
	   if (pwp == NULL || pwp->pw_passwd[0] == 0)  /* no access pass? */
	      goto ok;	/* all done */
	}

	if (pwp == NULL || pwp->pw_passwd[0] != 0) 
	{   if ((cp = getpass(passcount == 0 ? pass1 : pass2)) != NULL
	       && cp[0] != '\0')
	    {  cp = crypt(cp, pwp==NULL ? "xx" : pwp->pw_passwd);
	       if (pwp != NULL && (strcmp(cp, pwp->pw_passwd) == 0)
 	  	  && strcmp(s_name, ACCNAME)
		  && (strlen(pwp->pw_passwd) == PASSLEN))
		  failed = FALSE;  /* success */
	    }

	    if (failed)  /* failed attempt? */
	    {  oldtime = alarm(0);  /* turn off alarm */
	       passcount = 0;       /* reset pass count */
	       setutmp(tp, np, faillog, FALSE);  /* failed attempt */
	       alarm(oldtime);	     /* continue timeout */
	       printf("Login incorrect\n");  /* incorrect */
	       goto again;
	    }
	}

	if (remote && passcount++ == 0)  /* need another pass? */
	{  logcount--;       /* don't count as login attempt */
	   goto again;	     /* yes */	
	}
ok:
	alarm(0);	/* turn off login alarm timeout */
	endpwent();	/* close password file */

	if (chdir(s_dir) < 0) {
		perror(s_dir);
		slexit(1);
	}
	setutmp(tp, np, goodlog, TRUE);  /* successful login */
	chown(tp, s_uid, s_gid);
	chmod(tp, TTYMODE);
	information(s_uid, s_name);
	sprintf(nb, "HOME=%s", s_dir);
	env[1] = nb;
	if (s_uid == 0)
	   env[2] = "PS1=# ";

	setgid(s_gid);
	setuid(s_uid);

	for (i=3; close(i)>=0; i++);

	if (s_shell[0] != '\0') 
	{	strcat(shellbuff, s_shell);  /* build shell command line */
		execle("/bin/sh", "-sh", "-c", shellbuff, NULL, env);
		fprintf(stderr, "%s: cannot execute.\n", s_shell);
	}
	execle("/bin/sh", "-sh", NULL, env);
	fprintf(stderr, "No shell.\n");
	slexit(1);
}

/*
 * Reset all signals to their default
 * state so all commands get invoked
 * with a known signal state.
 */
resetsigs()
{
	register int i;

	for (i=1; i<=NSIG; i++)
		signal(i, SIG_DFL);
}

/*
 * Determine remoteness of login.
 * Remote logins will require some password,
 * either the user's own password, or the 'remacc' pseudo
 * user's password.
 */
isremote(tp)
char *tp;	/* ttyname result */
{
#ifdef IAPX86
	unsigned ttyflags;
	ioctl(1, TIOCGETTF, &ttyflags); /* get tty flags */
	return (ttyflags & T_HPCL);  	/* assume remote line if HUPCLS */
#endif
#ifdef PDP11
	register int i;
	static char *rttys[] = {	/* Quick and dirty */
		"/dev/tty30",
		"/dev/tty31",
		"/dev/tty32",
		""
	};
	for (i = 0; rttys[i][0] != 0; i += 1)
		if (strcmp(rttys[i], tp) == 0)
			return (1);
	return (0);
#endif
}

/*
 * Read the news and message of the day file.
 * Only print new information.
 * Also print `You have mail.' message if
 * applicable.
 */
information(uid, uname)
short uid;
char *uname;
{
	register int fd = -1;
	int (*oifunc)();
	int setflag();
	size_t prseek();
	time_t time();

	if (qflag)
		return;
	if ((fd = open(newslog, 2)) >= 0) {
		lseek(fd, (size_t)uid*sizeof news, 0);
		if (read(fd, &news, sizeof news) != sizeof news)
			news.n_time = news.n_seek = 0;
	}
	oifunc = signal(SIGINT, setflag);
	iflag = 0;
	if (stat(motdf, &sb)>=0 && sb.st_mtime>news.n_time)
		prseek(motdf, (size_t)0);
	news.n_seek = prseek(newsf, news.n_seek);
	news.n_time = time((time_t)0);
	signal(SIGINT, oifunc);
	if (fd >= 0) {
		lseek(fd, (size_t)uid*sizeof news, 0);
		write(fd, &news, sizeof news);
		close(fd);
	}
	strcat(mailname, uname);
	if (stat(mailname, &sb)>=0 && sb.st_size!=0)
		printf("You have mail.\n");
}

/*
 * Print a file, starting at position `seek',
 * watching for interrupts.
 */
size_t
prseek(fn, seek)
char *fn;
size_t seek;
{
	register FILE *fp;
	register int c;

	if ((fp = fopen(fn, "r"))!=NULL) {
		fseek(fp, seek, 0);
		while ((c=getc(fp)) != EOF) {
			if (nline < NPAGE)
				putchar(c);
			if (c == '\n') {
				if (iflag)
					break;
				if (nline++ == NPAGE)
					fprintf(stderr, longnews, fn);
			}
		}
		seek = ftell(fp);
		fclose(fp);
	}
	return (seek);
}

/*
 * Write out an accounting entry for
 * `tty' and `username' into filename pointed to by "filep".
 * If "success" is TRUE (indicating a good login) also write 'utmp' entry.
 */
setutmp(tty, username, filep, success)
char *tty, *username, *filep;
{
	time_t time();
	struct utmp utmp;
	struct utmp spare;
	size_t freeslot = -1, slot = 0;
	register ufd;

	utmp.ut_time = time((time_t)0);
	strncpy(utmp.ut_line, tty+5, 8);
	strncpy(utmp.ut_name, username, DIRSIZ);
	if ((ufd = open(filep, 1)) >= 0) {
		lseek(ufd, 0L, 2);
		write(ufd, &utmp, sizeof (utmp));
		close(ufd);
	}
	if (!success)  /* failed login attempt? */
	   return;     /* all done */

	if ((ufd = open("/etc/utmp", 2)) >= 0) {
		while (read(ufd, &spare, sizeof (spare)) == sizeof (spare)) {
			if (spare.ut_line[0] == '\0')
				freeslot = slot;
			else if (strncmp(spare.ut_line, utmp.ut_line, 8) == 0) {
				freeslot = slot;
				break;
			}
			slot += sizeof (utmp);
		}
		if (freeslot >= 0)
			lseek(ufd, freeslot, 0);
		write(ufd, &utmp, sizeof (utmp));
		close (ufd);
	}
}

setflag()
{
	++iflag;
}

usage()
{
	fprintf(stderr, "Usage: login [-q] [username]\n");
	slexit(1);
}

/* login alarm timeout routine */
timeout()
{	
	printf("\n");  /* neatness */
	slexit(1);  /* exit (and try force hangup for remote line) */
}

/*
 * Set the characters for the terminal to the defaults.
 * Return non-zero on error.
 */
settty(fd)
{
	if (fflag)
		return(0);
	if (ioctl(fd, TIOCSETN, &sgttyd) < 0)
		return(1);
	if (ioctl(fd, TIOCSETC, &tchars) < 0)
		return(1);
	return(0);
}

/* sleep for 2 seconds to make sure output has flushed, then exit */
slexit(status)
{
    sleep(2);
    exit(status);
}


