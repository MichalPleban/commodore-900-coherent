

/*
 * general header
 */
#include	<stdio.h>
#include	<rico.h>


/*
 * bit diddling
 */
#define	NCHARS	128		/* chars in ASCII set */
#define	NBCHAR	8		/* bits per char */

#define	bitset( c, p)	((p)[(c)>>3] |= bitmask[(c)&7])
#define	bitclr( c, p)	((p)[(c)>>3] &= ~bitmask[(c)&7])
#define	bitcom( c, p)	((p)[(c)>>3] ^= bitmask[(c)&7])
#define	bittst( c, p)	((p)[(c)>>3] & bitmask[(c)&7])


extern bool	eflag;		/* next arg is regular expression */
extern bool	fflag;		/* next arg is file containing rex */
extern bool	vflag;		/* line matches if rex NOT found */
extern bool	cflag;		/* only print # matches */
extern bool	lflag;		/* only print name of files that match */
extern bool	nflag;		/* also print line # */
extern bool	bflag;		/* also print block # */
extern bool	sflag;		/* only provide exit status */
extern bool	hflag;		/* do not print file names */
extern bool	yflag;		/* lower case also matches upper case input */
extern char	bitmask[];	/* used by bit-ops */

extern char	*newbits( );
