/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Default main() for lex(1) scanners linked with -ll: run yylex() over
 * the standard input.  Its own archive member, so a program's main wins.
 */
main()
{
	yylex();
	exit(0);
}
