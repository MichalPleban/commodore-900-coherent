/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Kevin Dedon.
 * Copyright (c) 2026 Michał Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * n0/get.c
 * C preprocessor.
 * Read next character.
 */

#include <time.h>
#ifdef   vax
#include "INC$LIB:cc0.h"
#else
#include "cc0.h"
#endif

/*
 * Look ahead for '#' or '(' and restore the stream if you fail.
 * This gets reentered looking for '#'
 * when the search for a '(' passes newline.
 */
skipto(d) int d;
{
	register char *p, *sp;
	register int c;

	sp = dpshp;
	while ((c = get()) >= 0 && ct[c] == SKIP)
		dpshc(c);
	if (c >= 0)
		dpshc(c);
	if (c != d) {
		for (p = dpshp; p < sp; ++p)
			unget(*p);
		dpshp = sp;
		return 0;
	}
	dpshp = sp;
	return 1;
}

/*
 * Look at a newly opened file.
 * Add implicit newline if empty.
 * This is almost certainly extraneous after steve's 9/13/94 change to get().
 */
emptyfilep()
{
	register int c;

	c = getc(ifp);
	if (c == EOF)
		c = '\n';
	ungetc(c, ifp);
}

/*
 * Get a character.
 * This function returns the next character,
 * and implements one character pushback,
 * as well as the expanded macro stack.
 */
get()
{
    register int c;

    for (;;) {
	switch (dstackp->ds_type) {

	case DS_FUNGET:	/* Ungotten character from file */
		c = (dstackp++)->ds_char & 0377;
		goto file_char;

	case DS_FILE:	/* Read next character from source file */
		if (ifp == NULL)	/* terminal EOF already seen: stay at EOF, */
			return EOF;	/* never re-getc or re-fclose a closed stream */
		while ((c = getc(ifp)) < 0) {
#if	GEMDOS || MSDOS
eof:
#endif
			/* Insert implicit newline before EOF if absent. */
			if (lastchar != '\n' && lastchar != EOF) {
				c = '\n';
				goto file_char;
			}
			/* EOF seen, close input file and pop include stack. */
			fclose(ifp);
			ifp = NULL;	/* guard a double fclose if re-entered past EOF */
			if (istackp < istack+NLEV) {
				if (cstackp != istackp->i_cstackp)
					cerror("missing #endif");
				cstackp = istackp->i_cstackp;
				line = istackp->i_line;
#ifndef	CPPONLY
				if (notvariant(VCPP)) {
					bput(LINE);
					iput((ival_t)line);
				}
#endif
				idp = istackp->i_file;
				setfname();
				ifp = istackp->i_fp;
				++istackp;
				continue;
			}
			if (cstackp < cstack+NLEV)
				cerror("missing #endif");
			cstackp = cstack + NLEV;
			lastchar = c;
			return c;		/* ie, EOF */
		}
file_char:
		lastchar = c;
		switch (ct[c]) {

		case SKIP:			/* New line? */
			/*
			 * A carriage return is nothing, on every host: the
			 * sources this compiles are edited on machines that end
			 * lines CR LF, and a CR that survived here made the
			 * splice below miss.  (Was DOS-only.)
			 */
			if (c == '\r')
				continue;
			if (c == '\n') {
				if (incpp) {
				    ungetc(c, ifp);
				    return EOF;
				}
				if (instring) {
				    /*
				     * Standalone-preprocessor mode copies text
				     * whose language is the CONSUMER's, not C:
				     * an apostrophe in an assembler comment is
				     * not an unterminated character literal.
				     * End the quote at the newline and hand the
				     * newline on, so the damage cannot run past
				     * the line it started on.
				     */
				    if (isvariant(VCPP)) {
					instring = 0;
					notskip = 0;
				    } else {
					cerror("new line in %s literal",
					  instring=='"' ? "string" : "character");
					++line;
					c = ' ';
				    }
				} else
				    notskip = 0;
			}
			notskip -= 1;
			break;

		case QUEST:		/* Trigraph? */
			if (notvariant(V3GRAPH))
				break;
			c = get_non_EOF(0);
			if (c != '?') {
				ungetc(c, ifp);
				c = '?';
				break;
			}
			c = get_non_EOF(0);
			switch (c) {
			case '=': ungetc('#', ifp); continue;
			case '(': ungetc('[', ifp); continue;
			case '/': ungetc('\\', ifp); continue;
			case ')': ungetc(']', ifp); continue;
			case '\'': ungetc('^', ifp); continue;
			case '<': ungetc('{', ifp); continue;
			case '!': ungetc('|', ifp); continue;
			case '>': ungetc('}', ifp); continue;
			case '-': ungetc('~', ifp); continue;
			default:
				ungetc(c, ifp);
				dspushc(DS_FUNGET, '?');
				c = '?';
			}
			break;

		case DIV:		/* Comment? */
			if (instring || cstate != 0)
				break;
			if (isvariant(VCPP) && isvariant(VCPPC))
				break;
			c = get_non_EOF(0);
			if (isvariant(VCPLUS) && c == '/') {
				/* Ignore //-delimited C++ online comment. */
				while ((c = getc(ifp)) != '\n') {
					if (c < 0) {
						c = '\n';	/* implicit at EOF */
						break;
					}
				}
				ungetc(c, ifp);
				--line;		/* '\n' will get read again */
				break;
			} else if (c != '*') {
				ungetc(c, ifp);
				c = '/';
				break;
			}
			for (;;) {
				c = get_non_EOF(1);
				if (c == '*') {
					c = get_non_EOF(1);
					if (c == '/')
						break;
					ungetc(c, ifp);
				} else if (c == '/') {
					c = get_non_EOF(1);
					if (c == '*')
						cwarn("nested comments");
					ungetc(c, ifp);
				} else if (c == '\n')
					++line;
			}
			c = ' ';
			break;

		case BACKDIV:			/* Splice? */
			c = get_non_EOF(0);
			while (c == '\r')		/* backslash CR LF is a splice too */
				c = get_non_EOF(0);
			if (c != '\n') {
				ungetc(c, ifp);
				c = '\\';
				break;
			}
			++line;
			continue;

		case HIGH0:
		case HIGH1:
		case HIGH2:
		case HIGH3:
		case JUNK:
#if	GEMDOS || MSDOS				/* Assuming use of bingetc() */
			if (c == 26)		/* ^Z */
				goto eof;
#endif
			/*
			 * isvariant(VCPP): a byte that is not C is still text
			 * to copy when this run is only a preprocessor -- what
			 * is legal is the consumer's question.
			 */
			if (instring || (c > ' ' && c < 0177) || cstate != 0
			 || isvariant(VCPP))
				break;
			cerror("illegal cpp character (%d decimal)", c);
			continue;

		case SHARP:
			if (notskip != 0)
				break;
			++incpp;
			control();
			--incpp;
			continue;
		}
		notskip += 1;
		break;

	case DS_UNGET:	/* Read ungotten character */
		c = (dstackp++)->ds_char & 0377;
		break;

	case DS_STRNG:
		if ((c = *dstackp->ds_ptr++ & 0377) == 0) {
			++dstackp;
			continue;
		}
		break;

	case DS_UFILE:	/* Convert file name into string */
		if ((c = *dstackp->ds_ptr++ & 0377) == 0) {
			++dstackp;
			continue;
		}
		if (c == '\\' || c == '"') {
			unget(c);
			c = '\\';
		}
		break;

	case DS_SHARP:	/* Read and stringize */
		if ((c = *dstackp->ds_ptr++ & 0377) == 0) {
			++dstackp;
			continue;
		}
		if (dstackp->ds_char) {
			if (c == dstackp->ds_char) {
				dstackp->ds_char = 0;
				if (c == '"') {
					unget(c);
					c = '\\';
				}
			} else if (c == '\\') {
				c = *dstackp->ds_ptr++;
				unget(c);
				if (c == '"')
					unget('\\');
				unget('\\');
				c = '\\';
			}
		} else if (c == '"') {
			dstackp->ds_char = c;
			unget(c);
			c = '\\';
		} else if (c == '\'')
			dstackp->ds_char = c;
		else if (c >= SET0)
			continue;
		break;

	case DS_DPUTP:
		dputp = (dstackp++)->ds_ptr;
		continue;

	case DS_NAME:
		++dstackp;
		continue;

	case DS_IEOF:
		c = EOF;
		break;

	default:
		cbotch("bad ds_type %d", dstackp->ds_type);
	}
	if (c == '\n') ++line;
	if (cstate != 0 && incpp == 0)
		continue;
	return c;
    }
}

/*
 * Get a character with fatal error if EOF occurs.
 * The flag is true if in a comment.
 */
get_non_EOF(cflag) int cflag;
{
	register int c;

	if ((c = getc(ifp)) < 0)
		cfatal((cflag) ? "EOF in comment" : "EOF in midline");
	return c;
}

/*
 * One character pushback for the callers of get().
 */
unget(c) register int c;
{
	if (c < 0)
		return;
	dspushc(DS_UNGET, c);
	if (c == '\n')
		--line;
}

/* end of n0/get.c */
