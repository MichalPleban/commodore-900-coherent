#include <stdio.h>
#include <ctype.h>
#include "bc.h"


/*
 *	Getnum reads in a number from standard input.  It allows
 *	decimal points and sets the scale field of the number accordingly.
 *	In addition, although the default base is ibase, it recognizes
 *	the C conventions of leading `0' for base 8 and `0x' for base
 *	16.
 */

rvalue	*
getnum(ch)
register int	ch;
{
	register rvalue	*result;
	register FILE	*inf = infile;
	mint	dig;
	int	val,
		dot,
		base = ibase;

	result = (rvalue *)mpalc(sizeof *result);
	newscalar(result);
	minit(&dig);
	if (ch == '0' && (ch = getc(inf)) != '.')
		if (ch == 'x') {
			ch = getc(inf);
			base = 16;
		} else
			base = 8;
	for (dot = FALSE;; ch = getc(inf))
		if (isascii(ch)&&isdigit(ch) || 'A'<=ch&&ch<='F') {
			val = isdigit(ch) ? ch-'0' : ch+0xA-'A';
			mitom(val, &dig);
			smult(&result->mantissa, base, &result->mantissa);
			madd(&result->mantissa, &dig, &result->mantissa);
			if (dot)
				++result->scale;
		} else if (ch == '.' & !dot)
			dot = TRUE;
		else
			break;
	ungetc(ch, inf);
	mvfree(&dig);
	return (result);
}


/*
 *	Sibase takes the rvalue pointed to by lval and set ibase
 *	to it if it is in range (ie. between 2 and 16).
 */

sibase(lval)
rvalue	*lval;
{
	register int	base;

	base = rtoint(lval);
	if (2 > base || base > 16)
		bcmerr("Invalid input base");
	ibase = base;
}
