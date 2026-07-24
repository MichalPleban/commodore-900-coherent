/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Substitute user-id temporarily
 * or become super user (as you wish).
 */

#include <stdio.h>
#include <pwd.h>

char	*getpass();
char	*crypt();

short	gid;
char	*password;		/* Set by getuname */
char	salt[3];
char	shell[] = "/bin/sh";
char	*shargs[] = {
	"su",
	NULL
};

char	prs[50] = "PS1=";
char	*prompt = prs;

main(argc, argv)
char *argv[];
{
	register char *cp;
	short uid;
	char *command;
	char **args;

	uid = getuname(argc>1 ? argv[1] : "0");
	if (password[0] != '\0') {
		while ((cp = getpass("Password:")) == NULL)
			;
		if (strcmp(crypt(cp, salt), password) != 0) {
			fprintf(stderr, "Sorry\n");
			exit(1);
		}
	}
	if (argc > 2) {
		command = argv[2];
		args = &argv[2];
	} else {
		command = shell;
		args = shargs;
	}
	setgid(gid);
	setuid(uid);
	addenviron(uid==0 ? "# " : "$ ");
	execvp(command, args);
	printf("%s: not found\n", command);
}

/*
 * Get a user-name from a string.
 * If the string is numeric use the
 * number directly.
 * The string `password' is set with
 * the user's password for checking later.
 */
getuname(s)
register char *s;
{
	register struct passwd *pwp;
	register short uid;

	if (*s>='0' && *s<='9') {
		uid = atoi(s);
		if ((pwp = getpwuid(uid)) == NULL) {
			fprintf(stderr, "%d: bad user number\n", uid);
			exit(1);
		}
	} else if ((pwp = getpwnam(s)) == NULL) {
		fprintf(stderr, "%s: not a user name\n", s);
		exit(1);
	}
	password = pwp->pw_passwd;
	salt[0] = pwp->pw_passwd[0];
	salt[1] = pwp->pw_passwd[1];
	salt[2] = '\0';
	gid = pwp->pw_gid;
	return (pwp->pw_uid);
}

/*
 * Add string `s' to the environment as "PS1".
 */
addenviron(s)
char *s;
{
	extern char **environ;
	register char **epp1, **epp2;
	register char **newenv;
	int n;
	char *malloc();

	for (epp1 = environ; *epp1!=NULL; epp1++)
		;
	n = (epp1-environ+1) * sizeof (char *);
	if ((newenv = (char **)malloc(n)) == NULL) {
		fprintf(stderr, "Out of memory for environments\n");
		exit(1);
	}
	strcat(prompt, s);
	for (epp1=environ, epp2=newenv; *epp1 != NULL; epp1++)
		if (strncmp(*epp1, "PS1=", 4) != 0)
			*epp2++ = *epp1;
		else {
			*epp2++ = prompt;
			prompt = NULL;
		}
	*epp2++ = prompt;
	*epp2 = NULL;
	environ = newenv;
}
