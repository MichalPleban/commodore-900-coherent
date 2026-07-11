

/*
 * support for bitmaps
 */
#include	"egrep.h"


char	bitmask[] = {
	0001, 0002, 0004, 0010, 0020, 0040, 0100, 0200
};


/*
 * allocate bitmap for character class
 *	If `setbits'==TRUE then the map is initialized to ones, else zeros.
 */
char	*
newbits( setbits)
bool	setbits;
{
	register	i,
			c;
	register char	*p,
			*q;

	p = malloc( NCHARS/NBCHAR);
	if (p == NULL)
		nomem( );
	c = 0;
	if (setbits)
		c = ~0;
	q = p;
	i = NCHARS / NBCHAR;
	do {
		*p++ = c;
	} while (--i);
	return (q);
}
