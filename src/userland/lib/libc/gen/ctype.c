/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Character types table
 * for the ASCII character set.
 * _ctype[0] is for EOF, the rest if indexed
 * by the ascii values of the characters.
 */

#include <ctype.h>

char	_ctype[] = {
	0,	/* EOF */
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _S|_C, _S|_C, _S|_C, _S|_C, _S|_C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_C, _C, _C, _C, _C, _C, _C, _C,
	_S|_X, _P, _P, _P, _P, _P, _P, _P,
	_P, _P, _P, _P, _P, _P, _P, _P,
	_D, _D, _D, _D, _D, _D, _D, _D,
	_D, _D, _P, _P, _P, _P, _P, _P,
	_P, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _U, _U, _U, _U, _U,
	_U, _U, _U, _P, _P, _P, _P, _P,
	_P, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _L, _L, _L, _L, _L,
	_L, _L, _L, _P, _P, _P, _P, _C,
};
