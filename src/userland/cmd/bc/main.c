#include <stdio.h>
#include "bc.h"


main(argc, argv)
register int	argc;
register char	*argv[];
{
	register FILE	*fp;

	init();
	++argv;
	if (--argc > 0 && **argv == '-')
		if (strcmp(*argv++, "-l") == 0) {
			fp = fopen("/usr/lib/lib.b", "r");
			if (fp == NULL)
				die("Can't open /usr/lib/lib.b");
			scan(fp);
			--argc;
		} else
			usage();
	while (--argc >= 0) {
		fp = fopen(*argv, "r");
		if (fp == NULL)
			die("Can't open %s", *argv);
		scan(fp);
		++argv;
	}
	scan(stdin);
	return (0);
}


scan(fp)
FILE	*fp;
{
	infile = fp;
	yyparse();
	fclose(fp);
}


die(str)
char	*str;
{
	fprintf(stderr, "bc: %r\n", &str);
	exit(1);
}


usage()
{
	fprintf(stderr, "Usage: bc [-l] file ... file\n");
	exit(1);
}
