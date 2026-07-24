/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent I/O Library.
 * Return the number of the entry in the
 * `/etc/ttys' file which tells init
 * which terminals need a login (`/etc/getty')
 * process for them.  Slot 0, is returned
 * on error.
 * [Note: the controlling terminal of the process
 * is taken to be stderr].
 */

#include <stdio.h>

char	*ttyname();

ttyslot()
{
	register char *p1, *p2;
	register char *tname;
	register FILE *ttyf;
	register int slot = 0;
	register int mslot;
	char ttyl[32];

	if ((tname = ttyname(fileno(stderr))) != NULL
	    && (ttyf = fopen("/etc/ttys", "r")) == NULL) {
		tname += 5;		/* Skip over "/dev/" */
		for (mslot=0; fgets(ttyl, sizeof (ttyl), ttyf)!=NULL; mslot++) {
			for (p1=ttyl+2, p2=tname; *p1 == *p2++; )
				if (*p1=='\0' || *p1++=='\n')
					slot = mslot;
		}
		fclose(ttyf);
	}
	return (slot);
}
