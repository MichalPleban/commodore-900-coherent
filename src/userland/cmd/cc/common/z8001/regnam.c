/*
 * common/z8001/regnam.c
 * Register names, including pseudos, indexed by the compiler's internal
 * register numbers (h/z8001/mch.h): R0..R15, the pairs, the quads, the byte
 * views, then the code generator's pseudo registers.  A REG tree leaf carries
 * one of these numbers; the register code packed into an address field does
 * NOT -- that is a hardware encoding (n3/z8001/icode.c names those itself).
 */

char	*regnames[] = {
	"r0",	"r1",	"r2",	"r3",	"r4",	"r5",	"r6",	"r7",
	"r8",	"r9",	"r10",	"r11",	"r12",	"r13",	"r14",	"r15",
	"rr0",	"rr2",	"rr4",	"rr6",	"rr8",	"rr10",	"rr12",	"rr14",
	"rq0",	"rq4",	"rq8",	"rq12",
	"rh0",	"rh1",	"rh2",	"rh3",	"rh4",	"rh5",	"rh6",	"rh7",
	"rl0",	"rl1",	"rl2",	"rl3",	"rl4",	"rl5",	"rl6",	"rl7",
	"None",		/* NONE   */
	"Anyr",		/* ANYR   */
	"Anyl",		/* ANYL   */
	"Pair",		/* PAIR   */
	"Temp",		/* TEMP   */
	"Lo",		/* LOTEMP */
	"Hi"		/* HITEMP */
};

/* end of common/z8001/regnam.c */
