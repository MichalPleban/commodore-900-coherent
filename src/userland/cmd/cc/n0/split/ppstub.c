/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * n0/split/ppstub.c
 * C preprocessor.
 * What the standalone preprocessor (CPPONLY) links instead of the parser.
 * The symbol table (n0/cc0sym.c) and the lexer (n0/lex.c) are shared with
 * the compiler and name a few parser services on paths cpp never takes:
 * block-scope exit, declarator and struct-info release, tag lookup, and
 * the reading of a floating constant, whose value cpp never uses.
 */
#include "cc0.h"

dbdown()
{
}

dropdim(dp) DIM *dp;
{
}

istag(sp) SYM *sp;
{
	return 0;
}

dvalread(dp, s) char *dp; char *s;
{
	/* the lexer reads a floating constant in a #define body or a #if too;
	 * the preprocessor only ever copies its TEXT, so the value is nothing */
}

xdropinfo(t, ip) int t; INFO *ip;
{
}

/* end of n0/ppstub.c */
