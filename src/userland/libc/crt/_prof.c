/*
 * Called from the run-time start-off to enable profiling and to
 * write out the final profiling information.
 */
#include <stdio.h>
#include <types.h>


#define	STDERR	2


_profon()
{
	register unsigned	bufl;
	register short		*buf;
	extern			etext();
	static char		emsg[]	= "No room for profil buffer\n";

	bufl = (unsigned)etext / 2;
	buf = (short *)sbrk(bufl * sizeof (short));
	if (buf == NULL) {
		write(STDERR, emsg, sizeof emsg);
		abort();
	}
	monitor((vaddr_t)1, (vaddr_t)etext, buf, bufl);
}


_profoff()
{
	monitor(NULL);
}
