/*
 * Assertion verifier.
 * (tester for hacks)
 */

#if NDEBUG
#define	assert(p)
#else
#define	assert(p)	if(!(p)){printf("%s: %d: assert(%s) failed.\n",\
			    __FILE__, __LINE__, "p");exit(1);}
#endif
