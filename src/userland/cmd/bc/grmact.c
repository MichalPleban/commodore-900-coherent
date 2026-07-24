/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <assert.h>
#include "bc.h"


/*
 *	Gerror is used by the various grammer actions when a
 *	semantic error is discoverd.  It simply prints out a
 *	message and sets allok to FALSE.
 */

gerror(str)
char	*str;
{
	fprintf(stderr, "%r\n", &str);
	allok = FALSE;
}


/*
 *	Sload emits the code to load the scalar with dictionary entry
 *	var.
 */

sload(var)
register dicent	*var;
{
	switch (var->localt) {
	default:
		gerror("`%s' is wrong local type", var->word);
		break;
	case SCALAR:
		emitop(PLOSC);
		emitcnt(var->localv);
		break;
	case UNDEFINED:
		switch (var->globalt) {
		default:
			gerror("`%s' is wrong global type", var->word);
			break;
		case UNDEFINED:
			var->globalt = SCALAR;
			newscalar(&var->globalv.rvalue);
			/* fall thru */
		case SCALAR:
			emitop(PGLSC);
			emitnum(&var->globalv.rvalue);
			break;
		}
	}
}


/*
 *	Aeload emits the code to load the array element with dictionary
 *	entry var.
 */

aeload(var)
register dicent	*var;
{
	switch (var->localt) {
	default:
		gerror("`%s' is wrong local type", var->word);
		break;
	case ARRAY:
		emitop(PLOAE);
		emitcnt(var->localv);
		break;
	case UNDEFINED:
		switch (var->globalt) {
		default:
			gerror("`%s' is wrong global type", var->word);
			break;
		case UNDEFINED:
			var->globalt = ARRAY;
			newarray(&var->globalv.arvalue);
			/* fall thru */
		case ARRAY:
			emitop(PGLAE);
			emitarry(&var->globalv.arvalue);
			break;
		}
	}
}


/*
 *	Arload emits the code to load the whole array with dictionary
 *	entry var.
 */

arload(var)
register dicent	*var;
{
	switch (var->localt) {
	default:
		gerror("`%s' is wrong local type", var->word);
		break;
	case ARRAY:
		emitop(PLOAR);
		emitcnt(var->localv);
		break;
	case UNDEFINED:
		switch (var->globalt) {
		default:
			gerror("`%s' is wrong global type", var->word);
			break;
		case UNDEFINED:
			var->globalt = ARRAY;
			newarray(&var->globalv.arvalue);
			/* fall thru */
		case ARRAY:
			emitop(PGLAR);
			emitarry(&var->globalv.arvalue);
			break;
		}
	}
}


/*
 *	If there is room for another code item, incloc advances loc
 *	and returns the old loc.  If not then incloc calls gerror.
 *	Note that both the value returned by incloc and loc itself
 *	always point to a location in cstream.
 */

code	*
incloc()
{
	register code	*res;

	res = loc++;
	if (res >= &cstream[MAXCODE - 1]) {
		loc = res;
		gerror("Too much code");
	}
	return (res);
}


/*
 *	Negate returns the opcode which branches on the opposite
 *	condition of when `op' branches.
 */

opcode
negate(op)
opcode op;
{
	switch (op) {
	case BRALW:
		return (BRNEV);
	case BRNEV:
		return (BRALW);
	case BRLT:
		return (BRGE);
	case BRLE:
		return (BRGT);
	case BREQ:
		return (BRNE);
	case BRGE:
		return (BRLT);
	case BRGT:
		return (BRLE);
	case BRNE:
		return (BREQ);
	}
	assert(FALSE);
}


/*
 *	Locaddr sets the local addresses (ie frame pointer offsets) for
 *	all local variables and parameters.  `vec' is an array of `len'
 *	pointers to dictionary entries and `base' is the frame offset
 *	of the first entry.
 *	Note that locaddr assumes that it is never called with len zero.
 */

locaddr(vec, len, base)
register dicent	*vec[];
register int	len,
		base;
{
	do {
		(*vec++)->localv = base++;
	} while (--len > 0);
}


/*
 *	Chkfunc checks to make sure that the identifier with dictionary
 *	entry pointed to by `dicp' can be used as a function.  If it
 *	is not already a function, then the body and types fields of the
 *	func value are set to NULL.
 */

chkfunc(dicp)
register dicent	*dicp;
{
	if (dicp->globalt == FUNCTION)
		return;
	if (dicp->globalt == UNDEFINED) {
		dicp->globalt = FUNCTION;
		dicp->globalv.fvalue.body = dicp->globalv.fvalue.types = NULL;
	} else
		gerror("`%s' is not a function", dicp->word);
}


/*
 *	Install installs the definition of the function `fnc'.
 *	`pvec' and `lpvec' (`avec' and `lavec') are the vector
 *	of formal parameters (respectively automatic variables)
 *	and its length.
 */

install(fnc, pvec, lpvec, avec, lavec)
register func	*fnc;
dicent	**pvec,
	**avec;
int	lpvec,
	lavec;
{
	int	csize;

	mpfree(fnc->body);
	mpfree(fnc->types);
	fnc->nparams = lpvec;
	fnc->nautos = lavec;
	if (lpvec + lavec == 0)
		fnc->types = NULL;
	else {
		fnc->types = (type *)mpalc((lpvec + lavec) * sizeof (type));
		copylty(pvec, fnc->types, lpvec);
		copylty(avec, &fnc->types[lpvec], lavec);
	}
	csize = (loc - cstream) * sizeof (code);
	fnc->body = (code *)mpalc(csize);
	copy((char *)cstream, (char *)fnc->body, csize);
	remloc(pvec, lpvec);
	remloc(avec, lavec);
}


/*
 *	Copylty copyies the local types from the dictionary vector
 *	`dvec' to the type vector `tvec'.  `len' is the number of
 *	entries to copy.
 */

copylty(dvec, tvec, len)
register dicent	*dvec[];
register type	tvec[];
register int	len;
{
	while (--len >= 0)
		*tvec++ = (*dvec++)->localt;
}


/*
 *	Copy copyies the block of bytes at `from' to that at `to'.
 *	`len' is the number of bytes to copy.  Note that these
 *	blocks should not overlap.
 */

copy(from, to, len)
register char	*from,
		*to;
register int	len;
{
	while (--len >= 0)
		*to++ = *from++;
}


/*
 *	Remloc is used to remove the local types from the dictionary
 *	vector `dvec'.  `len' is the length of the vector.
 */

remloc(dvec, len)
register dicent	**dvec;
register int	len;
{
	while (--len >= 0)
		(*dvec++)->localt = UNDEFINED;
}
