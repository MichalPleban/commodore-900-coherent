/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * n0/split/nocpp.c
 * C compiler.
 * The parser-only cc0.
 *
 * The preprocessor is a separate program (cpp: this same source built with
 * CPPONLY), so this cc0 reads preprocessed text.  What survives of the
 * preprocessor here is what cpp's output still carries:
 *
 *	#line N ["file"]	and the ATT form	# N ["file"]
 *	#pragma ...		handed to the machine layer's pragma()
 *
 * Macro expansion is gone: expand() only reads the identifier, which is
 * what the lexer relies on it for.
 */
#include "cc0.h"

expand(c) int c;
{
	getid(c);
	return 0;
}

/*
 * Entry to process a control line after # has been read.
 * Called from within get(), with incpp set, so get() returns
 * EOF at the end of the line.
 */
control()
{
	register int c;
	register char *p;
	char buf[NFNAME + 16];

	c = getnb();
	if (c < 0)
		return;
	if (ct[c] == ID) {
		getid(c);
		if (strcmp(id, "pragma") == 0) {
			p = buf;
			while ((c = get()) >= 0)
				if (p < buf + sizeof(buf) - 1)
					*p++ = c;
			*p = 0;
			if (pragma(buf) == 0)
				cwarn("unrecognized #pragma ignored: %s", buf);
			return;
		}
		if (strcmp(id, "line") != 0)
			goto bad;
		c = getnb();
	}
	if (c < 0 || ct[c] != CON)
		goto bad;
	getnum(c, 0);
	line = ival - 1;		/* the directive's newline counts one: see n0/sharp.c */
	c = getnb();
	if (c == '"') {
		p = buf;
		while ((c = get()) >= 0 && c != '"')
			if (p < buf + sizeof(buf) - 1)
				*p++ = c;
		*p = 0;
		setid(buf);
		setfname();
	} else
		unget(c);
	while ((c = get()) >= 0)
		;
	return;
bad:
	cerror("illegal control line");
	while ((c = get()) >= 0)
		;
}

/*
 * Hide-set markers (bytes >= SET0) exist only inside macro expansions,
 * which never reach this program; the lexer still asks.
 */
hideint(c, set) int c, set;
{
	return set;
}

/* end of n0/nocpp.c */
