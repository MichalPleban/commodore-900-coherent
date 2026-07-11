/*
 * Enroll users in the knapsack system.
 */
#include <stdio.h>
#include <signal.h>
#include "knapsack.h"

static	struct	knapsack k;
static	mint	key[K];
static	mint	t;
static	char	s1[PPSIZ], s2[PPSIZ];	/* passphrase buffers */
char	*getlogin();

main(ac, av)
int ac;
char *av[];
{
	register int i;
	register char *pname;
	register FILE *f;

	switch (ac) {
	case 1:
		if ((pname = getlogin()) == NULL)
			byebye(1, "Who are you?");
		break;
	case 2:
		pname = av[1];
		break;
	default:
		fputs("Usage: enroll [user]\n", stderr);
		break;
	}
	pname = pubkeyfile(pname);

	/*
	 * Is the user already enrolled? If so verify that he knows the
	 * passphrase that generates the public key stored in pathname.
	 */
	if ((f = fopen(pname, "r")) != NULL) {
		gpph("Current Passphrase: ", s1);
		knapsack(&k, s1);
		public(key, &k);
		for (i = 0; i < K; ++i) {
			pkin(&t, f);
			if (mcmp(&t, &key[i]) != 0)
				byebye(2,
				"Passphrase does not fit public key.");
		}
		fclose(f);
	}

	/*
	 * Get new passphrase, get a second copy and check for typos.
	 * If passphrase is empty remove the public key file.
	 */
	gpph("New Passphrase: ", s1);
	gpph("Repeat, please: ", s2);
	if (strcmp(s1, s2) != 0)
		byebye(2, "Passphrase not changed.");
	if (s1[0] == '\0') {
		unlink(pname);
		exit(0);
	}

	/*
	 * Catch signals while we are rewriting the public key file.
	 */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	/*
	 * Create a new public key file.
	 */
	if ((f = fopen(pname, "w")) == NULL)
		byebye(3, "Cannot open public key file for writing.");
	knapsack(&k, s1);
	public(key, &k);
	for (i = 0; i < K; ++i)
		pkout(&key[i], f);
	return (0);
}

static
byebye(n, s)
int n;
char *s;
{
	fputs(s, stderr);
	putc('\n', stderr);
	exit(n);
}

