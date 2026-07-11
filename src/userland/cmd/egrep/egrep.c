
/*
 * egrep --  pattern matcher
 *
 *	Lines of input are searched for the user-supplied extended
 * regular expression.  Command line options determine what "egrep"
 * does on a match: usually the line is printed.  Lines over BUFSIZ
 * chars in length may be fatal when read from pipes or raw devices.
 *	The implementation goal is speed and, to this end, a DFA is
 * employed.  Two key strategies reduce the time spent constructing
 * the DFA by a factor of 100, in typical cases:  (1) the use of
 * equivalence classes, (2) construction of the DFA dynamically.
 * These strategies win because of the way "egrep" tends to be used.
 * Most patterns use only 10-20 characters in the ASCII set of 128.
 * Most input exercises only 10% of the total possible transitions
 * (so why compute the remaining 90%?).  As a side benefit, these
 * strategies reduce "egrep"'s gigantic appetite for memory to merely
 * huge.  Strategy (2) has the important psychological effect of making
 * "egrep" appear to start running instantly; the inevitable time spent
 * constructing the DFA is amortized over the running time of the
 * program.
 */

#include	"egrep.h"
#include	"dragon.h"
#include	<access.h>

bool	eflag;		/* next arg is regular expression */
bool	fflag;		/* next arg is file containing rex */
bool	vflag;		/* line matches if rex NOT found */
bool	cflag;		/* only print # matches */
bool	lflag;		/* only print name of files that match */
bool	nflag;		/* also print line # */
bool	bflag;		/* also print block # */
bool	sflag;		/* only provide exit status */
bool	hflag;		/* do not print file names */
bool	yflag;		/* lower case also matches upper case input */

/*
 * egrep
 *	Return 0 if any matches found in the input files, else 1.
 */
main( argc, argv)
register char	**argv;
{
	register struct newt	*np;
	register struct dragon	*dp;
	struct newt		*makenfa( );
	struct newt		*npolish( );
	struct dragon		*initdfa( );
	int	status;
	int	i = 0;
	char	**init( );
	bool	search( );

	argv = init( argc, argv);
	np = makenfa( );
	ncheck( initdfa( np)->d_s);
	np = npolish( np);
	dp = initdfa( np);

	status = 1;
	while (*argv)
	{   if (access(*argv, AREAD))
	    {  fprintf(stderr, "egrep: can't open %s\n", *argv);
	       *argv[0] = 0;  /* flag arg as invalid */
	    }
	    i++;     /* count arg */
	    argv++;  /* advance to next arg */
	}

	argv -= i;  /* back to start of filename args to process */
	if (*argv)
		do {
			if (*argv[0] == 0)  /* arg to skip? */ 
			   continue;        /* yes */
			if (freopen( *argv, "r", stdin) == NULL)
			{	fprintf(stderr, 
				   "egrep: can't open %s\n", *argv);
				continue;
			}
			if (search( *argv, dp, np))
				status = 0;
		} while (*++argv);
	else
		if (search( "(stdin)", dp, np))
			status = 0;

	return (status);
}


/*
 * initialization
 *	Process command line.  Return `argv' pointing to first file arg.
 */
static char	**
init( argc, argv)
char	**argv;
{
	register char	**av,
			*a;
	register bool	gotrex;
	extern char	*regexp;
	extern FILE	*rexf;

	gotrex = FALSE;
	av = &argv[1];

	while (a = *av++) {
		if (*a != '-') {
			if (not gotrex) {
				gotrex = TRUE;
				regexp = a;
				a = *av++;
			}
			break;
		}
		while (*++a)
			switch (*a) {
			case 'e':
				if (gotrex)
					onlyone( );
				if (*av == NULL)
					fatal( "missing arg to -e");
				regexp = *av++;
				gotrex = TRUE;
				break;
			case 'f':
				if (gotrex)
					onlyone( );
				if (*av == NULL)
					fatal( "missing arg to -f");
				rexf = fopen( *av, "r");
				if (rexf == NULL)
					fatal( "can't open %s", *av);
				++av;
				gotrex = TRUE;
				break;
			case 'v':
				vflag = TRUE;
				break;
			case 'c':
				cflag = TRUE;
				break;
			case 'l':
				lflag = TRUE;
				break;
			case 'n':
				nflag = TRUE;
				break;
			case 'b':
				bflag = TRUE;
				break;
			case 's':
				sflag = TRUE;
				break;
			case 'h':
				hflag = TRUE;
				break;
			case 'y':
				yflag = TRUE;
				break;
			default:
				fatal( "no such flag -%c", *a);
			}
	}

	if (not gotrex)
	{	fprintf(stderr,
		    "Usage: egrep [ -bcefhlnsvy ] pattern [ file ...]\n");
		exit(2);
	}

	--av;
	if (argc-(av-argv) <= 1)
		hflag = TRUE;
	return (av);
}


static
onlyone( )
{

	fatal( "exactly one pattern required");
}


nomem( )
{

	fatal( "out of mem");
}


/*
 * fatal error
 */
fatal( arg0)
char	*arg0;
{

	fflush( stdout);
	fprintf( stderr, "egrep: %r\n", &arg0);
	exit( 2);
}
