/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Assertion tester.
 */

#if NDEBUG
#define	assert(p)
#else
#define	assert(p)	if(!(p)){printf("%s: %d: assert(%s) failed.\n",\
			    __FILE__, __LINE__, "p");;}
#endif










