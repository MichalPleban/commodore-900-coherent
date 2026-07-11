

/*
 * echo -- print command line arguments
 */
#include	<stdio.h>


#define	bool	char
#define	TRUE	(0 == 0)
#define	FALSE	(not TRUE)
#define	not	!


main( argc, argv)
register char	**argv;
{
	char	obuf[BUFSIZ];
	bool	newline;

	setbuf( stdout, obuf);
	newline = TRUE;
	if (*++argv && strcmp( *argv, "-n")==0) {
		++argv;
		newline = FALSE;
	}

	while (*argv) {
		fputs( *argv++, stdout);
		if (*argv)
			putchar( ' ');
	}

	if (newline)
		putchar( '\n');
	fclose( stdout);
	return (0);
}
