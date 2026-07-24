/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Miscellaneous routines.
 */
#include <coherent.h>
#include <acct.h>
#include <errno.h>
#include <ino.h>
#include <stat.h>
#include <uproc.h>

#if 0
/*
 * Copy `n' bytes from `bp1' to `bp2'.
 */
kkcopy(bp1, bp2, n)
register char *bp1;
register char *bp2;
unsigned n;
{
	register unsigned n1;

	n1 = n;
	if (n1) {
		do {
			*bp2++ = *bp1++;
		} while (--n1);
	}
	return (n);
}

/*
 * Clear the next `n' bytes starting at `bp'.
 */
kclear(bp, n)
register char *bp;
register unsigned n;
{
	if (n) {
		do {
			*bp++ = 0;
		} while (--n);
	}
}
#endif

/*
 * Make sure we are the super user.
 */
super()
{
	if (u.u_uid) {
		u.u_error = EPERM;
		return (0);
	}
	u.u_flag |= ASU;
	return (1);
}

/*
 * Make sure we are the gived `uid' or the super user.
 */
owner(uid)
{
	if (u.u_uid == uid)
		return (1);
	if (u.u_uid == 0) {
		u.u_flag |= ASU;
		return (1);
	}
	u.u_error = EPERM;
	return (0);
}

/*
 * Panic.
 */
panic(a1)
char *a1;
{
	static panflag;

	if (panflag++ == 0) {
		printf("\nLife is cruel !!!\n");
		printf("Panic: %r", &a1);
		putchar('\n');
		usync();
	}
	halt();
	--panflag;
}

/*
 * Print a message from a device driver.
 */
devmsg(dev, a1)
dev_t dev;
char *a1;
{
	printf("(%d,%d): %r", major(dev), minor(dev), &a1);
	printf("\n");
}
