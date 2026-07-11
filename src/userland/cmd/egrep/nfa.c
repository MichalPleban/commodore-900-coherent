

/*
 * creation of the NFA
 */
#include	<ctype.h>
#include	"egrep.h"
#include	"newt.h"


/* only one is set by init( )
 */
char	*regexp;		/* user's regular expression */
FILE	*rexf;			/* file containing user's rex */


static	curc;			/* current char from rex */

struct newt	*getrex( ),
		*getterm( ),
		*getfac( ),
		*getatom( ),
		*newnewt( );


/*
 * create NFA from rex
 *	A pointer to the NFA is returned.
 */
struct newt	*
makenfa( )
{
	register struct newt	*p;

	advance( );
	p = getrex( );
	if (curc)
		misplaced( curc);
	if (p == NULL)
		fatal( "empty pattern");
	return (p);
}


/*
 * check NFA semantics
 *	Ensure correct usage of '^' and '$'.
 */
ncheck( npp)
register struct newt	**npp;
{
	register struct newt	*np;

	++uniq;

	while (np = *npp++)
		if (np->n_cp && np->n_c!=EPSILON) {
			if (np->n_flags & N_EOL)
				nbeeline( np);
			nwalk( np->n_cp);
		}
}


/*
 * add finishing touch to NFA
 *	Prepend a sort of ".*" to the front of the NFA.
 */
struct newt	*
npolish( np)
register struct newt	*np;
{
	register struct newt	*p;

	p = newnewt( );
	p->n_b = newbits( TRUE);
	p->n_cp = newnewt( );
	p->n_cp->n_c = EPSILON;
	p->n_cp->n_cp = p;
	p->n_ep = np;
	p->n_fp = np->n_fp;
	return (p);
}


/*
 * get regular expression
 *	A trailing '\n' is tolerated, to accommodate the "-f" option.
 */
static struct newt	*
getrex( )
{
	register struct newt	*p,
				*q,
				*start,
				*final;

	start = getterm( );
	if (start == NULL)
		return (start);
	final = newnewt( );
	start->n_fp->n_c = EPSILON;
	start->n_fp->n_cp = final;
	start->n_fp = final;

	for (; ; ) {
		switch (curc) {
		case '|':
			advance( );
			if (p = getterm( ))
				break;
			misplaced( '|');
		case '\n':
			advance( );
			if (p = getterm( ))
				break;
			if (curc != '\0')
				misplaced( '\n');
		default:
			return (start);
		}
		p->n_fp->n_c = EPSILON;
		p->n_fp->n_cp = final;
		q = newnewt( );
		q->n_c = EPSILON;
		q->n_cp = p;
		q->n_ep = start;
		q->n_fp = final;
		start = q;
	}
}


/*
 * get term
 */
static struct newt	*
getterm( )
{
	register struct newt	*start,
				*p;

	start = getfac( );
	if (start == NULL)
		return (start);

	while (p = getfac( )) {
		start->n_fp->n_c = EPSILON;
		start->n_fp->n_cp = p;
		start->n_fp = p->n_fp;
	}

	return (start);
}


/*
 * get factor
 */
static struct newt	*
getfac( )
{
	register struct newt	*p,
				*q,
				*start;

	start = getatom( );
	if (start == NULL)
		return (start);
	p = start;

	if (curc == '*') {
		advance( );
		start = newnewt( );
		q = newnewt( );
		start->n_c = EPSILON;
		start->n_cp = p;
		start->n_ep = q;
		p->n_fp->n_c = EPSILON;
		p->n_fp->n_cp = q;
		p->n_fp->n_ep = p;
		start->n_fp = q;
	}
	else if (curc == '+') {
		advance( );
		q = newnewt( );
		p->n_fp->n_c = EPSILON;
		p->n_fp->n_cp = q;
		p->n_fp->n_ep = p;
		start->n_fp = q;
	}
	else if (curc == '?') {
		advance( );
		start = newnewt( );
		start->n_c = EPSILON;
		start->n_cp = p;
		start->n_ep = p->n_fp;
		start->n_fp = p->n_fp;
	}
	return (start);
}


/*
 * get atom
 *	The interpretation of the "-y" option is given by this example:
 *		egrep -y 'R\i\c\o H[a-z]* Tudor'
 * means
 *		egrep 'Rico H[A-Za-z]* T[Uu][Dd][Oo][Rr]'
 */
static struct newt	*
getatom( )
{
	register struct newt	*start;
	char			*charclass( );

	start = newnewt( );
	start->n_cp = newnewt( );
	start->n_fp = start->n_cp;

	switch (curc) {
	case '\0':
	case '\n':
	case '|':
	case '*':
	case ')':
	case ']':
	case '+':
	case '?':
		nonewts( start);
		return (0);
	case '.':
		start->n_b = newbits( TRUE);
		bitclr( '\n', start->n_b);
		equiv( start);
		break;
	case '^':
		start->n_flags |= N_BOL;
		start->n_c = '\n';
		equiv( start);
		break;
	case '$':
		start->n_flags |= N_EOL;
		start->n_c = '\n';
		equiv( start);
		break;
	case '[':
		advance( );
		start->n_b = charclass( );
		equiv( start);
		break;
	case '(':
		advance( );
		nonewts( start);
		start = getrex( );
		if (curc == '\0')
			fatal( "missing ')'");
		if ((start == NULL) || curc!=')')
			misplaced( curc);
		break;
	case '\\':
		advance( );
		if (curc=='\0' || curc=='\n')
			misplaced( '\\');
		start->n_c = curc;
		equiv( start);
		break;
	default:
		if (yflag && islower( curc)) {
			start->n_b = newbits( FALSE);
			bitset( curc, start->n_b);
			bitset( toupper( curc), start->n_b);
		}
		else
			start->n_c = curc;
		equiv( start);
	}
	advance( );

	return (start);
}


static char	*
charclass( )
{
	register char	*p;
	register	c;
	register bool	compl;

	compl = FALSE;
	p = newbits( FALSE);
	if (curc == '^') {
		compl = TRUE;
		advance( );
	}
	if (curc == '\0')
		badclass( );

	do {
		c = curc;
		advance( );
		if (c == '\0')
			badclass( );
		if (curc == '-') {
			advance( );
			if (curc == '\0')
				badclass( );
			if (curc == ']') {
				bitset( '-', p);
				bitset( c, p);
			}
			else if (c > curc)
				fatal( "bad char class range %c-%c", c, curc);
			else {
				do {
					bitset( c, p);
				} while (++c <= curc);
				advance( );
			}
		}
		else
			bitset( c, p);
	} while (curc != ']');

	if (bittst( '\n', p))
		misplaced( '\n');
	if (yflag)
		for (c='a'; c<='z'; ++c)
			if (bittst( c, p))
				bitset( toupper( c), p);
	if (compl) {
		for (c=0; c<NCHARS; ++c)
			bitcom( c, p);
		bitclr( '\n', p);
	}
	return (p);
}


/*
 * allocate new newt struct
 */
static struct newt	*
newnewt( )
{
	register struct newt	*p;

	p = malloc( sizeof *p);
	if (p == NULL)
		nomem( );
	p->n_c = 0;
	p->n_flags = 0;
	p->n_b = NULL;
	p->n_cp = NULL;
	p->n_ep = NULL;
	p->n_fp = NULL;
	p->n_uniq = 0;
	p->n_id = n_id++;

	return (p);
}


/*
 * free newt structs
 *	Used to free unused atoms.
 */
static
nonewts( np)
struct newt	*np;
{

	free( (char *)np->n_cp);
	free( (char *)np);
	n_id -= 2;
}


/*
 * get next char from rex
 *	The next char from the file or the string is placed in `curc'.
 */
static
advance( )
{
	register	c;

	if (regexp)
		c = *regexp++;
	else {
		c = getc( rexf);
		if (c == EOF)
			c = '\0';
	}
	curc = c & 0177;
}


/*
 * report misplaced '^'
 */
static
nwalk( np)
register struct newt	*np;
{

	while (np->n_cp) {
		if (np->n_uniq == uniq)
			return;
		np->n_uniq = uniq;
		if (np->n_flags & N_EOL)
			nbeeline( np);
		if (np->n_flags & N_BOL)
			misplaced( '^');
		if (np->n_ep)
			nwalk( np->n_ep);
		np = np->n_cp;
	}
}


/*
 * report misplaced '$'
 */
static
nbeeline( np)
register struct newt	*np;
{

	while (np = np->n_cp) {
		if ((np->n_cp && np->n_c!=EPSILON)
		or (np->n_ep))
			misplaced( '$');
	}
}


static
misplaced( c)
{
	static char	s[]	= "`c'";

	s[1] = c;
	fatal( "misplaced %s in pattern", c=='\n'? "newline": s);
}


static
badclass( )
{

	fatal( "non-terminated char class");
}
