/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent - event/error logging facility.
 */
#include <coherent.h>

#define NEVENT 256
typedef struct {
	char	*e_event;	/* Function pointer or whatever */
	int	e_time;		/* Timer tick of event */
} EVENT;

EVENT events[NEVENT];
unsigned curevent = 0;
unsigned totevent = 0;

elog(eventp)
char *eventp;
{
	register EVENT *ep;

	totevent += 1;
	ep = &events[curevent++];
	curevent %= NEVENT;
	ep->e_event = eventp;
	ep->e_time = timer.t_time;
}
