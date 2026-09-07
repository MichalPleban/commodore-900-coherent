/*
 * n1/z8001/fixtop.c
 * Adjust the type at the top of a tree node to a computational type.
 * Z8001: the ALU is 16-bit, so the natural compute width is WORD (int). Bytes
 * are computed in word registers (S8->S16, U8->U16); word and long already ARE
 * compute widths and stay as-is (UNLIKE the i386, whose 32-bit ALU widened
 * everything to S32/U32). Float computes as double.
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

fixtoptype(tp)
register TREE *tp;
{
	register type;

	type = tp->t_type;
	if (type == S8)
		tp->t_type = S16;
	else if (type == U8)
		tp->t_type = U16;
	else if (type == F32)
		tp->t_type = F64;
}

/* end of n1/z8001/fixtop.c */
