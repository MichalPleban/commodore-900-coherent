/*
 * Nroff/Troff.
 * Miscellaneous routines.
 */
#include <stdio.h>
#include "roff.h"
#include "str.h"

/*
 * Read a block into the buffer `bp' starting at seek position
 * `l' in the temp file.
 */
nread(l, bp)
long l;
char *bp;
{
	if (tmpseek-l <= DBFSIZE) {
		copystr(bp, diskbuf, DBFSIZE, 1);
		return;
	}
	lseek(fileno(tmp), (long) l, 0);
	if (read(fileno(tmp), bp, DBFSIZE) != DBFSIZE)
		panic("Temp file read error");
}

/*
 * Write the buffer `bp' which contains `n' elements of size `s'
 * onto the end of the temp file.
 */
nwrite(bp, s, n)
register char *bp;
unsigned s;
register unsigned n;
{
	unsigned bno, off;
	register char *dp;

	bno = tmpseek / DBFSIZE;
	off = tmpseek % DBFSIZE;
	if (bno!=0 && off==0) {
		--bno;
		off += DBFSIZE;
	}
	dp = &diskbuf[off];
	s = n *= s;
	while (n--) {
		if (dp >= &diskbuf[DBFSIZE]) {
			lseek(fileno(tmp), (long) bno++*DBFSIZE, 0);
			if (write(fileno(tmp), diskbuf, DBFSIZE) != DBFSIZE)
				panic("Temp file write error");
			dp = &diskbuf[0];
		}
		*dp++ = *bp++;
	}
	tmpseek += s;
}

/*
 * Given a string, return a pointer to a copy of it.
 */
char	*
duplstr(cp0)
register char *cp0;
{
	register char *cp1, *cp2;

	cp1 = cp0;
	while (*cp1++)
		;
	cp2 = (char *) nalloc(cp1-cp0);
	cp1 = cp2;
	while (*cp1++=*cp0++)
		;
	return (cp2);
}

/*
 * Copy the array of `n' elements containing a structure of size
 * `size' from `s2' to `s1'.
 */
copystr(s1, s2, size, n)
register char *s1, *s2;
register int n;
{
	if ((n*=size) == 0)
		return;
	do {
		*s1++ = *s2++;
	} while (--n);
}

/*
 * Allocate `n' bytes.
 */
char	*
nalloc(n)
{
	register char *cp;

	if ((cp=malloc(n)) == NULL)
		panic("Out of core");
	return (cp);
}

/*
 * Release the given storage.
 */
nfree(cp)
char *cp;
{
	free(cp);
}

/*
 * Print out a warning.
 */
printe(a1)
char *a1;
{
	register STR *sp;

	for (sp=strp; sp; sp=sp->s_next) {
		if (sp->s_type == SFILE) {
			fprintf(stderr, "%d: ", sp->s_clnc);
			break;
		}
	}
	fprintf(stderr, "%r\n", &a1);
	if (debflag)
		fprintf(stderr, "Request: %s\n", miscbuf);
}

/*
 * An irrecoverable error was found.
 * Print out an error message and leave.
 */
panic(s)
{
	fprintf(stderr, "%r\n", &s);
	abort();
}
