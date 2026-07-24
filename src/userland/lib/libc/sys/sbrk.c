/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Sbrk - grow memory in data segment by
 * a specified increment.
 * Special version that does Commodore Large model Z8001
 */
#include <stdio.h>
#include <types.h>

extern	int	errno;
extern	vaddr_t	__end;

char *
sbrk(incr)
unsigned int	incr;
{
	extern	char	*brk();
	register vaddr_t send,
			rend;

#if 1		/* On the z8001 (at least) reduce the waste */
	rend = __end;
#else
	rend = brk(NULL);
#endif
	if (incr == 0)
		return (rend);
#if Z8001
	if (((unsigned)rend+incr) < (unsigned)rend)
		rend = rend - (unsigned)rend +0x01000000L;
	send = rend + incr;
#else
	send = rend + incr;
	if (send < rend)
		return (NULL);
#endif
	errno = 0;
	brk(send);
	if (errno)
		return (NULL);
	return (rend);
}
