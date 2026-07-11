/*
 * Time buffer.
 */
#ifndef	 TIMEB_H
#define	 TIMEB_H
#include <types.h>

struct timeb {
	time_t	time;			/* Time since 1970 */
	unsigned short millitm;		/* Milliseconds */
	short	 timezone;		/* Time zone */
	short	 dstflag;		/* Daylight saving time applies */
};

#endif
