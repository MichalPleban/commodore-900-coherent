

#include	<stdio.h>


#define	NAV	5			/* max # environmental args */


struct line {
	struct line	*l_next;
	char		*l_line;
};


int	width	= 80,			/* # columns across page */
	length	= 24,			/* # lines down page */
	fieldmax,			/* # columns of widest field */
	nacross,			/* # fields across page */
	ndown,				/* # fields down page */
	nfields;			/* # fields read from input */
char	*sel0( ),
	*sel1( ),
	*sel2( ),
	*(*select)( )	= sel1;
char	*getenv();

struct line	*lhead,
		*ltail;


main( argc, argv)
char	*argv[];
{
	char	obuf[BUFSIZ],
		av[NAV];

	envargs( av);
	setflags( av);
	setflags( &argv[1]);
	setbuf( stdout, obuf);

	input( );
	output( );

	return (0);
}


setflags( av)
register char	**av;
{

	for (; *av; ++av) {
		if (av[0][0] != '-')
			fatal( "%s is not a flag", av[0]);
		switch (av[0][1]) {
		case '0':
			select = sel0;
			break;
		case '1':
			select = sel1;
			break;
		case '2':
			select = sel2;
			break;
		case 'w':
			width = atoi( &av[0][2]);
			break;
		case 'l':
			length = atoi( &av[0][2]);
			break;
		default:
			fatal( "bad flag %s", av[0]);
		}
	}
}


input( )
{
	register struct line	*lp;
	register	i;
	char		lbuf[BUFSIZ];

	while ((i=getline( lbuf)) >= 0) {
		lp = malloc( sizeof *lp);
		if (lp == NULL)
			nomem( );
		if (lhead)
			ltail->l_next = lp;
		else
			lhead = lp;
		ltail = lp;
		lp->l_next = NULL;
		if (i) {
			lp->l_line = malloc( i+1);
			if (lp->l_line == NULL)
				nomem( );
			strcpy( lp->l_line, lbuf);
		}
		else
			lp->l_line = NULL;
		++nfields;
	}

	++fieldmax;
}


output( )
{
	register	i,
			j;

	nacross = (width+1) / fieldmax;
	if (nacross <= 0)
		nacross = 1;
	ndown = (nfields+nacross-1) / nacross;

	for (i=0; i<ndown; ++i) {
		for (j=0; j<nacross; ++j)
			putline( (*select)( i, j));
		putline( (char *)0);
	}

	fclose( stdout);
}


getline( lbuf)
char	lbuf[];
{
	register char	*p;
	register	col,
			xcol;
	int		c;

	p = lbuf;
	col = 0;
	xcol = 0;

	for (; ; ) {
		switch (c = getchar( )) {
		case EOF:
			return (-1);
		case '\n':
			break;
		case ' ':
			++xcol;
			continue;
		case '\t':
			xcol = (xcol|7) + 1;
			continue;
		case '\b':
			if (xcol > col)
				--xcol;
			else if (col) {
				--col;
				--xcol;
				*p++ = c;
			}
			continue;
		default:
			while (col < xcol) {
				*p++ = ' ';
				++col;
			}
			*p++ = c;
			++col;
			++xcol;
			continue;
		}
		break;
	}

	if (col > fieldmax)
		fieldmax = col;
	*p = '\0';
	return (p - lbuf);
}


putline( lbuf)
char	*lbuf;
{
	register	c;
	register char	*p;
	static		col,
			xcol;

	p = lbuf;
	if (p == NULL) {
		col = 0;
		xcol = 0;
		putchar( '\n');
		return;
	}

	while (c = *p++)
		switch (c) {
		case ' ':
			++xcol;
			break;
		case '\b':
			--col;
			--xcol;
			putchar( c);
			break;
		default:
			while ((col|7)+1 <= xcol) {
				putchar( '\t');
				col = (col|7) + 1;
			}
			while (col < xcol) {
				putchar( ' ');
				++col;
			}
			putchar( c);
			++col;
			++xcol;
			break;
		}

	xcol += fieldmax - xcol%fieldmax;
}


char	*
sel0( i, j)
{
	register struct line	*lp;
	register		m,
				n;

	n = i*nacross + j;
	m = 0;
	for (lp=lhead; lp; lp=lp->l_next)
		if (++m > n)
			return (lp->l_line? lp->l_line: "");

	return ("");
}


char	*
sel1( i, j)
{
	register struct line	*lp;
	register		m,
				n;

	n = i + j*ndown;
	m = 0;
	for (lp=lhead; lp; lp=lp->l_next)
		if (++m > n)
			return (lp->l_line? lp->l_line: "");

	return ("");
}


char	*
sel2( i, j)
{
	register struct line	*lp;
	register		m,
				n;

	n = i + j*(ndown-1) + (j>nfields%nacross? nfields%nacross: j);
	if (i*nacross+j >= nfields)
		return ("");
	lp = lhead;
	for (m=0; m<n; ++m)
		lp = lp->l_next;
	return (lp->l_line? lp->l_line: "");
}


envargs( av)
char	**av;
{
	register	fl;
	register char	*p;
	register char	**ap;

	ap = av;
	*ap = NULL;
	if ((p=getenv( "C")) == NULL)
		return;

	fl = 0;
	while (*p)
		switch (*p++) {
		case '\t':
		case '\n':
		case ' ':
			fl = 0;
			p[-1] = '\0';
			break;
		default:
			if (fl)
				break;
			if (ap >= &av[NAV-1])
				fatal( "too many env args");
			*ap++ = &p[-1];
			++fl;
			break;
		}

	*ap = NULL;
}


nomem( )
{

	fatal( "out of mem");
}


fatal( arg0)
char	*arg0;
{

	fflush( stdout);
	fprintf( stderr, "c: %r\n", &arg0);
	exit( 1);
}
