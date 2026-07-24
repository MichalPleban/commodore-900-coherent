/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 */
#include <coherent.h>
#include <proc.h>
#include <seg.h>
#include <uproc.h>

#ifndef VERSION		/* This should be specified at compile time */
#define VERSION	"..."
#endif

/*
 * Initialise various things.  When we return we will return to user mode.
 */
char version[] = VERSION;
char copyright[] = "(c) 1982-1985 Mark Williams Company, Chicago\n(c) 2026 OpenCoherent contributors\n";
main()
{
	register SEG *sp;

	u.u_error = 0;
	bufinit();
	cltinit();
	pcsinit();
	seginit();
	devinit();
	printf("\nOpenCoherent (%uK, %u) version %s\n", msize, asize, version);

	/*
	 * Turn on clock, start off processes, mount root device
	 * and return.
	 */
	batflag = 1;
	if ((sp=salloc((size_t)UPASIZE, SFSYST|SFNCLR|SFNSWP)) == NULL)
		panic("Cannot allocate user area");
	if ((iprocp=process(idle))==NULL || (eprocp=process(NULL))==NULL)
		panic("Cannot create process");
	eveinit(sp);
	fsminit();
	printf(copyright);
}
