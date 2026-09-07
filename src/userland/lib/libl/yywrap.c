/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Default yywrap() for lex(1) scanners: called at end of input; a
 * program that wants to continue on another file defines its own and
 * returns 0 after switching.  A separate archive member so that a
 * program's own definition is the one linked.
 */
yywrap()
{
	return (1);
}
