/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "mprec.h"


/*
 *	Mzero and mone are pointers to mints with values zero and one
 *	respectively.  They are used as convenient constants in various
 *	routines.  Mminint and Mmaxint are pointers to mints with value
 *	the minimum and maximum integer which will fit into an int.
 *	They are used to check for overflow on conversion.  Care should
 *	be taken that none of these mints is ever changed.
 */

static char	__mzero = 0;
static mint	_mzero = {1, &__mzero};
mint	*mzero = &_mzero;

static char	__mone = 1;
static mint	_mone = {1, &__mone};
mint	*mone = &_mone;

static char	__mminint[] = {0, 0, 0176, 0177};	/* min int (-32768) */
static mint	_mminint = {4, __mminint};
mint	*mminint = &_mminint;

static char	__mmaxint[] = {0177, 0177, 1};	/* max int (32767) */
static mint	_mmaxint = {3, __mmaxint};
mint	*mmaxint = &_mmaxint;
