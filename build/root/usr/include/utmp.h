/*
 * Structure of the login records in `/etc/utmp'
 * as well as the cummulative records in
 * `/usr/adm/wtmp'.
 */

#ifndef DIRSIZ
#define	DIRSIZ	14
#endif
#ifndef	TYPES_H
#include <types.h>
#endif

struct	utmp {
	char	ut_line[8];		/* tty name */
	char	ut_name[DIRSIZ];	/* User name */
	time_t	ut_time;		/* time signed on */
};
