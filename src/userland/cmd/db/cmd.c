/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - the `:' commands (binary 0x2786..0x3855).
 *
 * Formatted display of one datum, escape rendering, the `:' command
 * dispatcher, breakpoint table management and listing, the map listing.
 *
 * Reconstructed from the shipped binary (_disasm/db.asm); the analysis
 * copy with the original addresses is _disasm/db_part3.c.
 */

#include "db.h"

/* ---- externs from other parts ---- */

/* ---- globals (see db_part3.names) ---- */

/* 0x2786 fmtitem(buf, space, fmt, radix, readfn) — render one datum of display format `fmt' at dot into buf; returns end of text, NULL on failure */
char *
fmtitem(buf, space, fmt, radix, readfn)
register char *buf;
int space;
int fmt;
int radix;
int (*readfn)();
{
	int iv;
	double dv;
	long fv;
	long lv;
	long pv;
	int wv;
	char b3[4];		/* NOTE: 3 bytes read, then l3tol */
	char cv;
	register char *lim;

	switch (fmt) {

	case 'b':
		if (!(*readfn)(space, &cv, 1))
			return (NULL);
		dsize = 1;
		if (radix != 'd')
			iv = cv & 0377;
		else
			iv = cv;
		break;

	case 'h':
		if (!(*readfn)(space, &wv, 2))
			return (NULL);
		dsize = 2;
		iv = wv;		/* the `d' test is a no-op for a word */
		break;

	case 'w':
		if (!(*readfn)(space, &iv, 2))
			return (NULL);
		dsize = 2;
		break;

	case 'c':
		if (!(*readfn)(space, &cv, 1))
			return (NULL);
		dsize = 1;
		return (escchar(buf, cv & 0377));

	case 'C':
		if (!(*readfn)(space, &cv, 1))
			return (NULL);
		dsize = 1;
		if ((cv & 0377) >= ' ' && (cv & 0377) < 0177)
			*buf++ = cv;
		else
			*buf++ = '.';
		return (buf);

	case 'f':
		if (!(*readfn)(space, &fv, 4))
			return (NULL);
		dsize = 4;
		dv = (double)fv;
		sprintf(buf, numfmt(fmt, radix), dv);
		goto skip;

	case 'F':
		if (!(*readfn)(space, &dv, 8))
			return (NULL);
		dsize = 8;
		sprintf(buf, numfmt(fmt, radix), dv);
		goto skip;

	case 'i':
		buf = disasm(buf, space);
		dsize = 2;
		return (buf);

	case 'l':
		if (!(*readfn)(space, &lv, 4))
			return (NULL);
		dsize = 4;
		sprintf(buf, numfmt(fmt, radix), lv);
		goto skip;

	case 'p':
		if (!(*readfn)(space, &pv, 4))
			return (NULL);
		dsize = 4;
		return (symstr(buf, 0, pv, 0x7fff));

	case 's':
	case 'S':
		lim = buf + 0x40;
		cv = 0;
		for (;;) {
			if (!(*readfn)(space, &cv, 1))
				return (NULL);
			if (cv == 0) {
				dsize = 2;
				return (buf);
			}
			if (fmt == 's')
				buf = escchar(buf, cv & 0377);
			else
				*buf++ = cv;
			if (buf > lim)
				return (NULL);
		}

	case 'v':
		if (!(*readfn)(space, b3, 3))
			return (NULL);
		dsize = 3;
		l3tol(&lv, b3, 1);
		sprintf(buf, numfmt(fmt, radix), lv);
		goto skip;

	case 'Y':
		buf = disptime(buf, dotspace, readfn);
		dsize = 2;
		return (buf);

	default:
		return (NULL);
	}
	sprintf(buf, numfmt(fmt, radix), iv);
skip:
	while (*buf)
		buf++;
	return (buf);
}

/* 0x2ba8 escchar(buf, c) — append character c to buf in C escape form; returns end of text */
char *
escchar(buf, c)
register char *buf;
register int c;
{
	*buf++ = '\\';
	switch (c) {
	case 0:
		*buf++ = '0';
		break;
	case '\b':
		*buf++ = 'b';
		break;
	case '\n':
		*buf++ = 'n';
		break;
	case '\f':
		*buf++ = 'f';
		break;
	case '\r':
		*buf++ = 'r';
		break;
	case '\\':
		*buf++ = '\\';
		break;
	default:
		if (c < ' ' || c >= 0177) {
			sprintf(buf, "%03o", c);
			while (*buf)
				buf++;
		} else
			buf[-1] = c;
	}
	return (buf);
}

/* 0x2c7c disptime(buf, space, readfn) — read a long time value at dot and append its ctime() text (24 chars) to buf */
char *
disptime(buf, space, readfn)
register char *buf;
int space;
int (*readfn)();
{
	long t;
	register char *p;
	register int n;

	if (!(*readfn)(space, &t, 4))
		return (NULL);
	p = ctime(&t);
	n = 24;
	do
		*buf++ = *p++;
	while (--n);
	return (buf);
}

/* 0x2ce2 colon(ap) — execute one `:' command; ap = the parsed address/count records; returns 0 when the child is to run, 1 otherwise */
colon(ap)
register struct opnd *ap;
{
	register int c;
	register int k;

	c = getch();
	switch (c) {

	case '=':
		if (!eol())
			goto illegal;
		print("%lo\n", evalue2(ap, dotaddr));
		return (1);

	case '?':
		if (!eol())
			goto illegal;
		if (lasterr == NULL)
			return (1);
		print("%s\n", lasterr);
		return (1);

	case 'a':
		if (!eol())
			goto illegal;
		prsym(espace(ap, -1), evalue(ap, dotaddr), 0x7fff);
		outch('\n');
		return (1);

	case 'b':
		if (objloaded == 0)
			goto illegal;
		k = c = getch();
		if (k != 'r') {
			ungetch(c);
			c = 'b';
		}
		getrest("i+.:a\ni+.?i\n:x");
		if (bptset(c, evalue(ap, dotaddr), linebuf) != 0)
			return (1);
		synerr("Cannot set breakpoint");
		return (1);

	case 'c':
		if (!eol())
			goto illegal;
		c = '\n';
		if (childalive == 0)
			goto illegal;
		if (!eempty(ap))
			setpc(evalue(ap, 0L));
		runmode = 0;
		return (0);

	case 'd':
		if (objloaded == 0)
			goto illegal;
		k = c = getch();
		if (k != 'r' && c != 's' && c != 'a') {
			ungetch(c);
			c = 'b';
		}
		if (!eol())
			goto illegal;
		if (c == 'a') {
			bptclear();
			return (1);
		}
		if (bptdel(c, evalue(ap, dotaddr)) != 0)
			return (1);
		synerr("Cannot delete breakpoint");
		return (1);

	case 'e':
		if (!eempty(ap))
			setpc(evalue(ap, 0L));
		getrest(nullstr);
		return (execcmd());

	case 'f':
		if (!eol())
			goto illegal;
		print("%s\n", faultname != NULL ? faultname : "No fault");
		return (1);

	case 'k':
		if (childalive == 0)
			goto illegal;
		if (!eol())
			goto illegal;
		if (eempty(ap)) {
			synerr("No signal specified");
			return (1);
		}
		error("Cannot send %d", (int)evalue2(ap, 0L));
		return (1);

	case 'm':
		if (!eol())
			goto illegal;
		maplist();
		return (1);

	case 'o':
		if (!eol())
			goto illegal;
		swapmode = (int)evalue2(ap, 0L);
		return (1);

	case 'p':
		if (!eol())
			goto illegal;
		bptlist();
		return (1);

	case 'q':
		if (!eol())
			goto illegal;
		if (evalue2(ap, 1L) != 0)
			quit();
		return (1);

	case 'r':
		if (!eol())
			goto illegal;
		c = '\n';
		if (regsok == 0)
			goto illegal;
		dumpregs();
		return (1);

	case 's':
		if (childalive == 0)
			goto illegal;
		if (!eempty(ap))
			setpc(evalue(ap, 0L));
		stepcnt = (int)evalue2(&ap[1], 1L);
		runmode = 2;
		k = c = getch();
		if (k != 'c') {
			runmode = 1;
			ungetch(c);
		}
		getrest("i+.?i");
		stepcmds = savestr(stepcmds, linebuf);
		return (0);

	case 't':
		if (!eol())
			goto illegal;
		c = '\n';
		if (regsok == 0)
			goto illegal;
		traceback((int)evalue2(ap, 0L));
		return (1);

	case 'x':
		if (!eol())
			goto illegal;
		if (evalue2(ap, 1L) != 0) {
			runreq = runmode;
			runmode = 0;
			pushfile(stdin);
		}
		return (1);
	}
illegal:
	synerr("Illegal command");
	while (c != '\n')
		c = getch();
	return (1);
}

/* 0x326c eol() — nonzero if the next input character is a newline (end of command) */
eol()
{
	register int c;

	c = getch();
	return (c == '\n');
}

/* 0x328c savestr(buf, s) — copy s (at most 127 chars) into buf, allocating a 128-byte buffer if buf is NULL; returns buf */
char *
savestr(buf, s)
char *buf;
register char *s;
{
	register char *p;

	if (buf == NULL)
		buf = alloc(0x80);
	p = buf;
	while (*s != 0 && p < buf + 0x7f)
		*p++ = *s++;
	*p++ = 0;
	return (buf);
}

/* 0x32e0 bptclear() — clear the breakpoint table (`:da'; also at startup) */
bptclear()
{
	register struct bpt *bp;
	register int i;

	/* NOTE: `--i' loop visits entries 0..14 only, not the 16th */
	for (bp = bpts, i = 16; --i; bp++) {
		bp->b_flags = 0;
		bp->b_cmd = NULL;
		bp->b_rcmd = NULL;
	}
}

/* 0x3314 bptset(kind, addr, cmds) — set a `b' breakpoint (kind 'b') or a return breakpoint (any other kind) with command text; returns 1 on success */
bptset(kind, addr, cmds)
int kind;
long addr;
char *cmds;
{
	long raddr;
	long rframe;
	register int r;

	if (kind == 'b')
		r = bptenter(1, addr, 0L, cmds);
	else {
		unwind1(&raddr, &rframe);
		r = bptenter(2, raddr, rframe, cmds);
	}
	return (r);
}

/* 0x3376 bptenter(kind, addr, aux, cmds) — enter a breakpoint of flag `kind' at addr into the table; returns 1 on success, 0 if unreadable / full / duplicate */
bptenter(kind, addr, aux, cmds)
int kind;
long addr;
long aux;
char *cmds;
{
	int w;
	register struct bpt *bp;
	register int i;

	dot = addr;
	if (!rdmem(1, &w, 2))
		return (0);
	for (bp = bpts, i = 16; --i; bp++)
		if (bp->b_flags != 0 && bp->b_addr == addr)
			goto found;
	for (bp = bpts, i = 16; --i; bp++)
		if (bp->b_flags == 0)
			goto found;
	return (0);
found:
	if (kind != 1 && (bp->b_flags & kind) != 0)
		return (0);
	bp->b_flags |= kind;
	bp->b_addr = addr;
	switch (kind) {
	case 1:
		bp->b_cmd = savestr(bp->b_cmd, cmds);
		break;
	case 2:
		bp->b_frame = aux;
		bp->b_rcmd = savestr(bp->b_rcmd, cmds);
		break;
	case 3:
		break;
	case 4:
		bp->b_frame4 = aux;
		break;
	}
	return (1);
}

/* 0x3476 bptdel(kind, addr) — delete the breakpoint of kind 'b'/'r'/'s' at addr; returns 1 if one was removed */
bptdel(kind, addr)
int kind;
long addr;
{
	register int mask;

	switch (kind) {
	case 'b':
		mask = 1;
		break;
	case 'r':
		mask = 2;
		break;
	case 's':
		mask = 4;
		break;
	/* NOTE: for any other kind the mask register is left uninitialised */
	}
	return (bptclr(mask, addr));
}

/* 0x34d2 bptclr(mask, addr) — clear the flag bits `mask' of the table entry at addr; returns 1 if found */
bptclr(mask, addr)
register int mask;
long addr;
{
	register struct bpt *bp;
	register int i;

	for (bp = bpts, i = 16; --i; bp++) {
		if ((mask & bp->b_flags) == 0)
			continue;
		if (bp->b_addr != addr)
			continue;
		bp->b_flags &= ~mask;
		return (1);
	}
	return (0);
}

/* 0x351a bptlist() — `:p': list all breakpoints */
bptlist()
{
	register struct bpt *bp;
	register int i;

	for (bp = bpts, i = 16; --i; bp++) {
		if (intrseen())
			return;
		if (bp->b_flags & 1)
			bptprint(' ', bp->b_addr, bp->b_cmd);
		if (intrseen())
			return;
		if (bp->b_flags & 2)
			bptprint('r', bp->b_addr, bp->b_rcmd);
		if (intrseen())
			return;
		if (bp->b_flags & 4)
			bptprint('s', bp->b_addr, nullstr);
	}
}

/* 0x35b4 bptprint(kind, addr, cmds) — print one breakpoint line: address, kind, symbolic address, command text with \n and \\ escaped */
bptprint(kind, addr, cmds)
int kind;
long addr;
register char *cmds;
{
	register int c;

	print("%08lx", addr);
	print("%c (", kind);
	prsym(1, addr, 0x7fff);
	print(") ");
	while ((c = *cmds++) != 0) {
		if (c == '\n')
			print("\\n");
		else if (c == '\\')
			print("\\\\");
		else
			outch(c);
	}
	outch('\n');
}

/* 0x3656 maplist() — `:m': print the address maps of the three spaces */
maplist()
{
	register struct map *m;
	register int i;

	for (i = 0; i < 3; i++) {
		m = maps[i];
		if (m == NULL)
			continue;
		print("%s", spacename[i]);
		if (i < 2 && maps[i+1] == m) {
			i++;
			print(" == %s", spacename[i]);
		}
		print("\n");
		for (; m != NULL; m = m->m_next) {
			print("[");
			mapbound(m->m_lo);
			print(", ");
			mapbound(m->m_hi);
			print("] => ");
			mapbound(m->m_off);
			print(" (%d)\n", m->m_which);
			if (intrseen())
				return;
		}
	}
}

/* 0x3760 mapbound(v) — print a map bound: "%08lx", or 8 `-' / `+' for the extreme values 0x80000000 / 0x7fffffff */
mapbound(v)
long v;
{
	register int c;
	register int n;

	if (v != 0x80000000L && v != 0x7fffffffL) {
		print("%08lx", v);
		return;
	}
	c = (v == 0x80000000L) ? '-' : '+';
	for (n = 8; n--; )
		putc(c, stdout);
}

/* 0x37f8 exprlist(ap) — parse a comma separated list of up to ten expressions into ap[]; unused slots are marked empty; returns 1, or 0 on a parse error */
exprlist(ap)
register struct opnd *ap;
{
	register int c;
	register int n;

	n = 10;
	for (;;) {
		if (expr(ap, 0) < 0)
			return (0);
		ap++;
		n--;
		c = getch();
		if (c != ',')
			break;
	}
	ungetch(c);
	while (n--) {
		*(char *)ap = 1;		/* NOTE: byte 1 at offset 0 marks an empty record */
		ap++;
	}
	return (1);
}
