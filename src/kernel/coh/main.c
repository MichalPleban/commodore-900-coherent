/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
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
char copyright[] = "(c) 1982 - 1985 Mark Williams Company, Chicago\n(c) OpenCoherent contributors\n";
main()
{
	register SEG *sp;

	u.u_error = 0;
	bufinit();
	cltinit();
	pcsinit();
	seginit();
	devinit();
	printf("\rOpenCoherent (%uK, %u) Version %s\n", msize, asize, version);

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
