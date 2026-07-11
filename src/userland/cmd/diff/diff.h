/*
 * Common declarations between the diff parts.
 */

#include <stdio.h>

#define	LSIZE	BUFSIZ
#define	NBIGF	25000		/* Size of large file for -d */

extern	int	bflag;
extern	int	eflag;
extern	int	sflag;
extern	int	rflag;
extern	char	*csymbol;
extern	char	*fn1;
extern	char	*fn2;

char	*calloc();
char	*alloc();
char	*sprintf();
extern	int	(*equal)();
