/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include	<stdio.h>


yyerror( mesg)
char	*mesg;
{
	fprintf( stderr, "%s\n", mesg);
}
