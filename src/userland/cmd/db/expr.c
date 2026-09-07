/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - expressions, input and target access (binary 0x3856..0x4c41).
 *
 * The expression parser, the command-input stack (files and pushed-back
 * strings), symbolic address formatting, symbol-table lookup, the
 * target-memory access layer (address maps) and the error printers.
 *
 * Reconstructed from the shipped binary (_disasm/db.asm); the analysis
 * copy with the original addresses is _disasm/db_part4.c.
 */

#include "db.h"

/*
 * An expression operand / result (6 bytes).
 * e_flags: 1 = empty (no expression was present: e_val is not valid),
 *          2 = e_space is valid (the operand named a segment);
 *          0 = value only (a number: the caller supplies the space).
 */

/*
 * Command-input source stack element (10 bytes, malloc'ed).
 */

/*
 * Address map: a run of target addresses [m_lo, m_hi) in one space,
 * backed by a file region starting at m_off, accessed through the
 * two callbacks (which get m_which as their first argument).  26 bytes.
 */

/*
 * In-core symbol index entry (6 bytes), built by part 1/5 (symidx).
 */





/* 0x3856 expr(ep, prec) — parse an expression into *ep; binary operators bind only above prec; 1 ok, 0 empty, -1 error */
expr(ep, prec)
register struct opnd *ep;
int	prec;
{
	int	c;
	int	t;
	struct	opnd e2;

	c = token(ep);
	if (c == -2)
		goto binop;
	if (c == -3)
		return (-1);
	switch (c & 0xff) {
	case '~':
		if (expr(ep, 0x400) <= 0)
			return (-1);
		ep->e_val = ~ep->e_val;
		break;
	case '-':
		if (expr(ep, 0x400) <= 0)
			return (-1);
		ep->e_val = -ep->e_val;
		break;
	case '*':
		if (expr(ep, 0x400) <= 0)
			return (-1);
		dot = ep->e_val;
		rdmem((ep->e_flags&2) ? ep->e_space : 0, &t, 2);
		ep->e_val = (unsigned)t;
		ep->e_flags = 2;
		ep->e_space = 0;
		break;
	case '(':
		if (expr(ep, 0) <= 0)
			return (-1);
		c = token((struct opnd *)0);
		if (c != ')') {
			untoken(c);
			synerr("Missing ')'");
			return (-1);
		}
		break;
	default:
		untoken(c);
		ep->e_flags = 1;
		return (0);
	}
binop:
	for (;;) {
		c = token((struct opnd *)0);
		if ((c & 0xff00) <= prec) {
			untoken(c);
			return (1);
		}
		switch (c & 0xff) {
		case '*':
			if ((t = expr(&e2, 0x200)) <= 0)
				goto noopr;
			ep->e_val *= e2.e_val;
			continue;
		case '/':
			if ((t = expr(&e2, 0x200)) <= 0)
				goto noopr;
			ep->e_val /= e2.e_val;
			continue;
		case '+':
			if ((t = expr(&e2, 0x100)) <= 0)
				goto noopr;
			ep->e_val += e2.e_val;
			continue;
		case '-':
			if ((t = expr(&e2, 0x100)) <= 0)
				goto noopr;
			ep->e_val -= e2.e_val;
			continue;
		default:		/* ',' and '.' carry an operator bit too */
			untoken(c);
			synerr("Unimplemented operator");
			return (-1);
		}
	}
noopr:
	if (t == 0)
		synerr("Missing operand");
	return (-1);
}

/* 0x3b54 token(ep) — next expression token: -2 operand stored in *ep, -3 error, else the character with 0x100 (additive) / 0x200 (multiplicative) operator bits */
token(ep)
register struct opnd *ep;
{
	register int c;

	for (;;) {
		c = getch();
		switch (c) {
		case '\t':
		case ' ':
			continue;
		case '*':
		case '/':
			return (c | 0x200);
		case '+':
		case '-':
			return (c | 0x100);
		case '.':
			if (ep == 0) {
				synerr("Missing opr before '.'");
				ungetch(c);
				return (-3);
			}
			ep->e_flags = 2;
			ep->e_space = dotspace;
			ep->e_val = dotaddr;
			return (-2);
		}
		if (c >= '0' && c <= '9')
			return (number(ep, c));
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')
			return (symopnd(ep, c));
		return (c);
	}
}

/* 0x3c5e untoken(c) — push a token back unless it is the -2/-3 pseudo-token */
untoken(c)
int	c;
{
	if (c != -3 && c != -2)
		ungetch(c & 0xff);
}

/* 0x3c8c number(ep, c) — parse a number whose first digit is c (0 octal, 0x hex, # decimal, else current radix) */
number(ep, c)
struct	opnd *ep;
register int c;
{
	register int d;
	long	val;

	val = 0;
	if (ep == 0) {
		synerr("Missing opr before number");
		ungetch(c);
		return (-3);
	}
	if (c == '0') {
		radix = 8;
		c = getch();
		if (c == 'x') {
			radix = 16;
			goto digits;
		}
	} else if (c == '#') {
		radix = 10;
		goto digits;
	}
	ungetch(c);
digits:
	for (;;) {
		c = getch();
		if ((d = c - ('a'-10)) < 10)
			if ((d = c - ('A'-10)) < 10)
				if ((d = c - '0') < 0 || d > 9)
					break;
		if (d >= radix)
			break;
		val = val * radix - d;	/* accumulate negated */
	}
	ungetch(c);
	ep->e_flags = 0;
	ep->e_val = -val;
	return (-2);
}

/* 0x3d7e symopnd(ep, c) — symbol operand: push c back and look the identifier up */
symopnd(ep, c)
struct	opnd *ep;
int	c;
{
	ungetch(c);
	if (ep == 0) {
		synerr("Missing opr before symbol");
		return (-3);
	}
	if (lookup(ep))
		return (-2);
	return (-3);
}

/* 0x3dc8 espace(ep, dflt) — space of an operand, or dflt when it named none */
espace(ep, dflt)
register struct opnd *ep;
int	dflt;
{
	if (ep->e_flags & 2)
		return (ep->e_space);
	return (dflt);
}

/* 0x3dec evalue(ep, dflt) — value of an operand, or dflt when the expression was empty */
long
evalue(ep, dflt)
register struct opnd *ep;
long	dflt;
{
	if (ep->e_flags & 1)
		return (dflt);
	return (ep->e_val);
}

/* 0x3e0e evalue2(ep, dflt) — identical to evalue (a second copy: count vs. address use) */
long
evalue2(ep, dflt)
register struct opnd *ep;
long	dflt;
{
	if (ep->e_flags & 1)
		return (dflt);
	return (ep->e_val);
}

/* 0x3e30 eempty(ep) — 1 if the expression was empty */
eempty(ep)
register struct opnd *ep;
{
	return (ep->e_flags & 1);
}

/* 0x3e4c getrest(dflt) — read the rest of the line into linebuf (backslash escapes; "\<nl>" continues); an empty line yields dflt */
getrest(dflt)
register char *dflt;
{
	register char *p;
	register int c;

	p = linebuf;
	c = getch();
	if (c == '\n') {
		while (*dflt)
			*p++ = *dflt++;
	} else {
		for (; c != '\n'; c = getch()) {
			if (c == '\\') {
				c = getch();
				if (c != '\n' && c != '\\')
					*p++ = '\\';
			}
			*p++ = c;
		}
	}
	*p++ = '\n';
	*p = 0;
}

/* 0x3ed0 pushstr(s) — push a string as the current command-input source */
pushstr(s)
char	*s;
{
	register struct insrc *ip;

	ip = (struct insrc *)alloc(sizeof(struct insrc));
	ip->i_next = insrc;
	ip->i_kind = 2;
	ip->i_ptr = s;
	insrc = ip;
}

/* 0x3f10 pushfile(fp) — push a FILE as the current command-input source */
pushfile(fp)
FILE	*fp;
{
	register struct insrc *ip;

	ip = (struct insrc *)alloc(sizeof(struct insrc));
	ip->i_next = insrc;
	ip->i_kind = 1;
	ip->i_ptr = (char *)fp;
	insrc = ip;
}

/* 0x3f50 getch() — next command character: pushed-back char first, then the source stack (popping exhausted sources); -1 when empty */
getch()
{
	register struct insrc *ip;
	register int c;

	if (ungotc) {
		c = ungotc;
		ungotc = 0;
		return (c);
	}
	for (;;) {
		ip = insrc;
		if (ip == 0)
			return (-1);
		switch (ip->i_kind) {
		case 1:
			if ((c = getc(((FILE *)ip->i_ptr))) != -1)
				return (c);
			if (((FILE *)ip->i_ptr) == stdin) {
				if (intrseen())
					continue;
				quit();
			}
			fclose(((FILE *)ip->i_ptr));
			break;
		case 2:
			if ((c = *ip->i_ptr++) != 0)
				return (c);
			break;
		}
		insrc = ip->i_next;
		release(ip);
	}
}

/* 0x4038 ungetch(c) — push back one input character */
ungetch(c)
int	c;
{
	ungotc = c;
}

/* 0x404e prsym(space, addr, maxoff) — print addr symbolically (symbol+offset, or a number) */
prsym(space, addr, maxoff)
int	space;
long	addr;
int	maxoff;
{
	register char *p;

	p = symstr(linebuf, space, addr, maxoff);
	*p = 0;
	print("%s", linebuf);
}

/* 0x4096 rdmem(space, buf, n) — read n bytes at dot from space into buf (words byte-swapped if needed); 1 ok, 0 fail */
rdmem(space, buf, n)
int	space;
char	*buf;
int	n;
{
	if (mapxfer(space, buf, n, 0) == 0)
		return (0);
	swapwords(buf, n);
	return (1);
}

/* 0x40d4 wrmem(space, buf, n) — write n bytes from buf at dot into space (swapping first); 1 ok, 0 fail */
wrmem(space, buf, n)
int	space;
char	*buf;
int	n;
{
	swapwords(buf, n);
	return (mapxfer(space, buf, n, 1));
}

/* 0x4108 swapwords(buf, n) — swap the bytes of every word in buf when swapmode == 1 and n is even */
swapwords(buf, n)
register char *buf;
unsigned n;
{
	register unsigned i;
	register int t;

	if (swapmode == 1 && (n & 1) == 0) {
		for (i = 0; i < n; i += 2) {
			t = buf[i];
			buf[i] = buf[i+1];
			buf[i+1] = t;
		}
	}
}

/* 0x415e mapxfer(space, buf, n, wr) — transfer n bytes at dot via the maps of space, piecewise per map; advances dot on success */
mapxfer(space, buf, n, wr)
int	space;
char	*buf;
int	n;
int	wr;
{
	register struct map *mp;
	register int left, cnt;
	int	(*fn)();
	long	addr;

	addr = dot;
	for (left = n; left != 0; left -= cnt) {
		mp = findmap(space, addr);
		if (mp == 0)
			return (0);
		cnt = left;
		if (addr + (long)(unsigned)cnt > mp->m_hi)	/* NOTE: unsigned compare */
			cnt = mp->m_hi - addr + 1;
		fn = wr ? mp->m_write : mp->m_read;
		if ((*fn)(mp->m_which, mp->m_off + (addr - mp->m_lo), buf, cnt) == 0)
			return (0);
		buf += cnt;
		addr += (long)(unsigned)cnt;
	}
	dot += (long)(unsigned)n;
	return (1);
}

/* 0x4234 findmap(space, addr) — the map of space containing addr (m_lo <= addr < m_hi), or 0 */
struct map *
findmap(space, addr)
int	space;
long	addr;
{
	register struct map *mp;

	for (mp = maps[space]; mp != 0; mp = mp->m_next)
		if (mp->m_lo <= addr && addr < mp->m_hi)
			break;
	return (mp);
}

/* 0x4278 lookup(ep) — read an identifier from the input and resolve it (space letter, register, symbol table) into *ep; 1 ok, 0 error */
lookup(ep)
register struct opnd *ep;
{
	register int c, i;
	struct ldsym sym;

	i = 0;
	for (;;) {
		c = getch();
		if (isascii(c) && (isalnum(c) || c == '_')) {
			if (i < NCPLN)
				sym.ls_id[i++] = c;
			continue;
		}
		break;
	}
	ungetch(c);
	while (i < NCPLN)
		sym.ls_id[i++] = 0;
	if (spacesym(&sym) == 0 && regsym(&sym) == 0 && findsym(&sym) == 0) {
		synerr("Symbol not found");
		return (0);
	}
	switch (sym.ls_type & 0xf) {
	case L_SHRI:
	case L_PRVI:
	case L_BSSI:
		ep->e_space = 1;
		ep->e_flags = 2;
		break;
	case L_SHRD:
	case L_PRVD:
	case L_BSSD:
	case L_ABS:
		ep->e_space = 0;
		ep->e_flags = 2;
		break;
	case L_REF:
		synerr("Symbol not defined");
		return (0);
	case 11:
		ep->e_space = 2;
		ep->e_flags = 2;
		break;
	}
	ep->e_val = sym.ls_addr;
	return (1);
}

/* 0x43ca spacesym(sp) — the one-letter space symbols d, i, u: value 0 in data, instruction, u-area */
spacesym(sp)
register struct ldsym *sp;
{
	if (sp->ls_id[1] != 0)
		return (0);
	switch (sp->ls_id[0]) {
	case 'd':
		sp->ls_type = L_SHRD;
		break;
	case 'i':
		sp->ls_type = L_SHRI;
		break;
	case 'u':
		sp->ls_type = 11;
		break;
	default:
		return (0);
	}
	sp->ls_addr = 0;
	return (1);
}

/* 0x4454 symstr(buf, space, addr, maxoff) — format addr as symbol[+off] when a symbol lies within maxoff below it (maxoff < 0: never), else as a number; returns the end of the string */
char *
symstr(buf, space, addr, maxoff)
register char *buf;
int	space;
long	addr;
int	maxoff;
{
	int	off;
	struct ldsym sym;

	if (maxoff >= 0 && nearsym(space, addr, &sym)
	 && (unsigned)(off = (unsigned)addr - sym.ls_addr) <= maxoff) {
		sprintf(buf, "%.*s", NCPLN, sym.ls_id);
		while (*buf)
			buf++;
		if (off) {
			*buf++ = '+';
			sprintf(buf, "%u", off);
		}
	} else
		putnum(buf, addr);
	while (*buf)
		buf++;
	return (buf);
}

/* 0x450c findsym(sp) — find sp->ls_id in the symbol table (exact match, else the C name with a trailing '_'); fills *sp; 1 found */
findsym(sp)
struct ldsym *sp;
{
	register struct symidx *xp;
	register long n;
	register int r, h, found;
	long	off;
	struct ldsym cur;
	struct ldsym best;

	found = 0;
	h = symhash(sp);
	xp = symidx;
	off = symoff;
	for (n = nsyms; n-- != 0; xp++, off += sizeof(struct ldsym)) {
		if (symidx != 0 && (xp->x_hash & 0xff) != h)
			continue;
		fseek(symfp, off, 0);
		if (fread(&cur, sizeof(struct ldsym), 1, symfp) != 1)
			return (0);
		canint(cur.ls_type);
		canlong(cur.ls_addr);
		r = symcmp(sp, &cur);
		if (r > 0) {
			*sp = cur;
			return (1);
		}
		if (r < 0) {
			best = cur;
			found = 1;
		}
	}
	if (found == 0)
		return (0);
	*sp = best;
	return (1);
}

/* 0x4636 symcmp(a, b) — compare a typed name a against table name b: 1 equal (first 8 chars), -1 if b is a followed by '_' (and a does not end in '_'), 0 otherwise */
symcmp(a, b)
register char *a, *b;
{
	register int n, c;

	n = 8;
	for (;;) {
		c = *b++;
		if (*a != c) {
			if (*b != 0)
				return (0);
			b--;
			if (*b != '_')
				return (0);
			if (*a != 0)
				return (0);
			a--;
			if (*a != '_')
				return (-1);
			return (0);
		}
		if (*a++ == 0)
			return (1);
		if (--n == 0)
			return (1);	/* NOTE: only 8 characters are significant */
	}
}

/* 0x4698 nearsym(space, addr, sp) — the highest-valued symbol of space at or below addr (u-area: any segment); fills *sp; 1 found */
nearsym(space, addr, sp)
int	space;
long	addr;
struct ldsym *sp;
{
	register struct symidx *xp;
	register int i, best, type;
	register long value;
	long	bestval;
	struct ldsym cur;

	strncpy(sp->ls_id, nullstr, NCPLN);
	if (symfp == 0)
		return (0);
	bestval = 0;
	best = -1;
	if (symidx == 0)
		fseek(symfp, symoff, 0);
	for (i = 0, xp = symidx; (long)i < nsyms; i++, xp++) {
		if (symidx != 0) {
			value = xp->x_value;
			type = xp->x_type & 0xff;
		} else {
			if (fread(&cur, sizeof(struct ldsym), 1, symfp) != 1)
				return (0);
			canint(cur.ls_type);
			canlong(cur.ls_addr);
			value = cur.ls_addr;
			type = cur.ls_type;
		}
		switch (type & 0xf) {
		case L_SHRI:
		case L_PRVI:
		case L_BSSI:
			if (space == 0)
				continue;
			break;
		case L_SHRD:
		case L_PRVD:
		case L_BSSD:
			if (space == 1)
				continue;
			break;
		default:
			continue;
		}
		if ((unsigned long)value > (unsigned long)addr)
			continue;
		if (bestval != 0 && (unsigned long)value <= (unsigned long)bestval)
			continue;
		bestval = value;
		best = i;
	}
	if (best < 0)
		return (0);
	fseek(symfp, symoff + (long)(best * sizeof(struct ldsym)), 0);
	if (fread(sp, sizeof(struct ldsym), 1, symfp) != 1)
		return (0);
	canint(sp->ls_type);
	canlong(sp->ls_addr);
	return (1);
}

/* 0x4870 mapread(which, off, buf, n) — map read callback: n bytes at file offset off of auxfp (which) or objfp */
mapread(which, off, buf, n)
int	which;
long	off;
char	*buf;
int	n;
{
	register FILE *fp;

	fp = which ? auxfp : objfp;
	fseek(fp, off, 0);
	return (fread(buf, n, 1, fp));
}

/* 0x48be mapwrite(which, off, buf, n) — map write callback: fwrite + fflush; 1 ok */
mapwrite(which, off, buf, n)
int	which;
long	off;
char	*buf;
int	n;
{
	register FILE *fp;

	fp = which ? auxfp : objfp;
	fseek(fp, off, 0);
	if (fwrite(buf, n, 1, fp) != 0 && fflush(fp) != -1)
		return (1);
	return (0);
}

/* 0x4928 alloc(n) — malloc or die */
char *
alloc(n)
unsigned n;
{
	register char *p;

	p = malloc(n);
	if (p == 0)
		fatal("No memory");
	return (p);
}

/* 0x495c release(p) — free */
release(p)
char	*p;
{
	free(p);
}

/* 0x4974 fatal(fmt, ...) — fatal error: message on stderr, exit(1) */
fatal(fmt)
char	*fmt;
{
	fprintf(stderr, "%r", &fmt);
	putc('\n', stderr);
	exit(1);
}

/* 0x49ea bpterror(addr) — "Breakpoint error at <addr>" on stderr */
bpterror(addr)
long	addr;
{
	fprintf(stderr, "Breakpoint error at ");
	fprintf(stderr, "%08lx", addr);
	putc('\n', stderr);
}

/* 0x4a66 error(fmt, ...) — error message on stderr */
error(fmt)
char	*fmt;
{
	fprintf(stderr, "%r", &fmt);
	putc('\n', stderr);
}

/* 0x4ad0 synerr(msg) — remember msg as the last error (for the `?' command) and print "?" */
synerr(msg)
char	*msg;
{
	lasterr = msg;
	fprintf(stderr, "?\n");
}

/* 0x4afe print(fmt, ...) — printf */
print(fmt)
char	*fmt;
{
	printf("%r", &fmt);
}

/* 0x4b24 outch(c) — putchar */
outch(c)
int	c;
{
	putc(c, stdout);
}

/* 0x4b6e toseg(l) — linear (seg<<16|off) to Z8001 segmented (seg<<24|off) address */
long
toseg(l)
long	l;
{
	return (((l << 8) & 0xff000000L) | (l & 0xffffL));
}

/* 0x4ba4 fromseg(l) — Z8001 segmented to linear address */
long
fromseg(l)
long	l;
{
	return (((l >> 8) & 0x00ff0000L) | (l & 0xffffL));
}

/* 0x4bda mkfilemap(p, base, size, fileoff) — make a map for linear range [base, base+size) backed by file offset fileoff (callbacks mapread/mapwrite, which = 0) */
struct map *
mkfilemap(p, base, size, fileoff)
long	p;		/* NOTE: passed through to mkmap's first argument; type per part 1 */
long	base;
long	size;
long	fileoff;
{
	register long start;

	start = toseg(base);
	return (mkmap(p, start, toseg(base + size) - start, fileoff, mapread, mapwrite, 0));
}
