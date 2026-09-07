/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - part 6: the built-in Z8000 disassembler (text 0x59b2..0x6385)
 * and the data tables it owns (data 0x0680..0x0704, 0x0724..0x0733,
 * 0x0768..0x0c68, 0x0c68..0x0f40).
 *
 * CLEAN-ROOM decompilation from _disasm/db.asm (db_annot.asm) and the
 * data segment of the binary.  No other db source was consulted.
 *
 * How it works: disasm reads the instruction word at `dot' and walks
 * optab[] for the first record whose (word & o_mask) == o_op; the record's
 * format string is then run through the interpreter dofmt, which copies
 * plain characters to the output buffer and expands `%' directives.  The
 * interpreter keeps four pieces of state: the instruction word `w', a
 * working operand word `v' (fetched from the target with %n and modified by
 * %e/%u/%d/%i), a 4-bit field `f' (selected from w with %0-%3, or from v
 * with %4-%7) and an operand size (b/w/l/q, defaulting per instruction via
 * %t and %m).  A directive may reject the whole record (%z, or an unreadable
 * operand) by returning NULL, in which case the driver tries the next
 * record; when nothing matches it prints ".word".
 *
 * Directive summary (see dofmt):
 *   %0..%3 f = nibble n of w        %4..%7 f = nibble n-4 of v
 *   %b %w %l %x %q size = 1/2/4/4/8   %m   make size the default size
 *   %t   'b' + size 1 for byte ops, size 2 for word ops (bit 8 of w)
 *   %n   fetch the next word into v   %e<h> v &= low h bits (hex digit h)
 *   %u<h> sign-extend v from h bits   %i+ v++   %i| v = |v|
 *   %d+ v = dot+2v   %d- v = dot-2v   %d. v = dot+v   (dot = next address)
 *   %a   v numeric (symbolic only when |v| >= 256)   %p  v symbolic (pc-rel)
 *   %o   immediate operand of `size' (byte imm in the high byte of a word)
 *   %r   register of `size' from f (rhN/rlN, rN, rrN, rqN)
 *   %c   condition code f      %f flags in f (czsv)     %k  control reg w&7
 *   %v   interrupt set w&3     %s system-call name w&0xff
 *   %S   segmented address (short: seg|off in one word, long: two words)
 *   %A   operand by mode, '$' immediate when mode 0 and f == 0
 *   %B   operand by mode: 0 (rrN), 1 addr[(rN)], 2 register, 3 '?'
 *   %y<bit><a><b>  emit a if bit of f set else b; '*' emits nothing
 *   %z<mode>  reject the record when mode matches and f == 0
 *   anything else after '%' is copied literally.
 */

#include "db.h"

/* ------------------------------------------------------------------ */
/* tables                                                             */
/* ------------------------------------------------------------------ */

/*
 * 0x08e0  optab[] -- opcode match table, 8-byte records, ended by a
 * record whose format pointer is 0.  Records with a mask that leaves the
 * addressing-mode bits (15:14) free are skipped when the word has both
 * bits set (mode 3 = the short-form instructions jr/ldk/djnz/calr/ldb #).
 */

struct optab optab[] = {
	{ 0xb400, 0xfe00, "adc%t\t%0r,%1r" },
	{ 0x0000, 0x3e00, "add%t\t%0r,%1A" },
	{ 0x1600, 0x3f00, "addl%lm\t%0r,%1A" },
	{ 0x0600, 0x3e00, "and%t\t%0r,%1A" },
	{ 0x2600, 0x3e00, "bit%t\t%1z0B,$%e4a" },
	{ 0x2600, 0xfef0, "bit%t\t%n6r,%w0r" },
	{ 0x1f00, 0xbf0f, "call\t%1B" },
	{ 0xd000, 0xf000, "calr\t%ecucd-a" },
	{ 0x0c08, 0x3e0f, "clr%t\t%1B" },
	{ 0x0c00, 0x3e0f, "com%t\t%1B" },
	{ 0x8d05, 0xff0f, "comflg %1f" },
	{ 0x0a00, 0x3e00, "cp%t\t%0r,%1A" },
	{ 0x1000, 0x3f00, "cpl%lm\t%l0r,%1A" },
	{ 0x0c01, 0xbe0f, "cp%t\t%1B,$%o" },
	{ 0xba00, 0xfe01, "cp%0y1s*%y3di%y2r*%t\t%n5r,(%x1r),%w6r,%4c" },
	{ 0xb000, 0xff00, "dab\t%bm%1r" },
	{ 0x2a00, 0x3e00, "dec%t\t%1B,$%e4i+a" },
	{ 0x7c00, 0xfffc, "di\t%v" },
	{ 0x1b00, 0x3f00, "div\t%l0r,%1A" },
	{ 0x1a00, 0x3f00, "divl%lm\t%q0r,%1A" },
	{ 0xf080, 0xf080, "djnz\t%2r,%e7d-a" },
	{ 0xf000, 0xf080, "dbjnz%bm %2r,%e7d-a" },
	{ 0x7c04, 0xfffc, "ei\t%v" },
	{ 0x2c00, 0x3e00, "ex%t\t%0r,%1B" },
	{ 0xb100, 0xff0f, "extsb %1r" },
	{ 0xb10a, 0xff0f, "exts%lm\t%1r" },
	{ 0xb107, 0xff0f, "extsl%qm %1r" },
	{ 0x7a00, 0xffff, "halt" },
	{ 0x3c00, 0xfe00, "in%t\t%0r,(%x1r)" },
	{ 0x3a04, 0xfe0f, "in%t\t%1r,%na" },
	{ 0x2800, 0x3e00, "inc%t\t%1B,$%e4i+a" },
	{ 0x3a00, 0xfe07, "in%0y3di%n4y3*r%t\t(%w5r),(%1r),%w6r" },
	{ 0x7b00, 0xffff, "iret" },
	{ 0x1e00, 0xbf00, "jp\t%0c,%1B" },
	{ 0xe000, 0xf000, "jr\t%2c,%e8u8d+a" },
	{ 0x2000, 0x3e00, "ld%t\t%0r,%1A" },
	{ 0x1400, 0x3f00, "ldl%lm\t%0r,%1A" },
	{ 0xc000, 0xf000, "ldb%bm\t%2r,$%e8a" },
	{ 0x3000, 0xfe00, "ld%t\t%0r,%1z0xr(%nwa)" },
	{ 0x3500, 0xff00, "ldl%lm\t%0r,%1z0xr(%nwa)" },
	{ 0x7000, 0xfe00, "ld%t\t%0r,%1z1xr(%nw6r)" },
	{ 0x7500, 0xff00, "ldl%lm\t%0r,%1z1xr(%nw6r)" },
	{ 0x2e00, 0xbe00, "ld%t\t%1B,%0r" },
	{ 0x1d00, 0xbf00, "ldl%lm\t%1B,%0r" },
	{ 0x0c05, 0xbe0f, "ld%t\t%1B,$%o" },
	{ 0x3200, 0xfe00, "ld%t\t%1z0xr(%nwa),%0r" },
	{ 0x3700, 0xff00, "ldl%lm\t%1z0xr(%nwa),%0r" },
	{ 0x7200, 0xfe00, "ld%t\t%1z1xr(%nw6),%0r" },
	{ 0x7700, 0xff00, "ldl%lm\t%1z1xr(%nw6),%0r" },
	{ 0x7600, 0xff00, "lda\tr%0r,%1B" },
	{ 0x3400, 0xff00, "lda\tr%0r,%1z0xr(%nwa)" },
	{ 0x7400, 0xff00, "lda\tr%0r,%1z1xr(%nw6r)" },
	{ 0x3400, 0xfff0, "ldar\tr%0r,%nd.p" },
	{ 0x8c09, 0xff0f, "ldctl %0k,%1r" },
	{ 0x8c01, 0xff0f, "ldctl %1r,%0k" },
	{ 0x7d08, 0xff08, "ldctl %0k,%1r" },
	{ 0x7d00, 0xff08, "ldctl %1r,%0k" },
	{ 0xba01, 0xfe07, "ld%0y3di%n4y3*r%t\t(%x5r),(%x1r),%w6r" },
	{ 0xbd00, 0xff00, "ldk\t%1r,$%e4a" },
	{ 0x1c01, 0xbf0f, "ldm\t%nw6r,%1B,$%e4i+a" },
	{ 0x1c09, 0xbf0f, "ldm\t%n1B,%w6r,$%e4i+a" },
	{ 0x3900, 0xbf0f, "ldps\t%1B" },
	{ 0x3000, 0xfef0, "ldr%t\t%0r,%nd.a" },
	{ 0x3500, 0xfff0, "ldrl%lm\t%0r,%nd.a" },
	{ 0x3200, 0xfef0, "ldr%t\t%nd.a,%0r" },
	{ 0x3700, 0xfff0, "ldrl%lm\t%nd.a,%0r" },
	{ 0x7b0a, 0xffff, "mbit" },
	{ 0x7b0d, 0xff0f, "mreq\t%1r" },
	{ 0x7b09, 0xffff, "mres" },
	{ 0x7b08, 0xffff, "mset" },
	{ 0x1900, 0x3f00, "mult\t%l0r,%w1A" },
	{ 0x1800, 0x3f00, "multl%lm %q0r,%1A" },
	{ 0x0c02, 0x3e0f, "neg\t%1B" },
	{ 0x8d07, 0xffff, "nop" },
	{ 0x0400, 0x3e00, "or%t\t%0r,%1A" },
	{ 0x3a08, 0xfe07, "ot%0y3dir%t\t(%xn5r),(%x1r),%w6r" },
	{ 0x3e00, 0xfe00, "out%t\t(%x1r),%0r" },
	{ 0x3a06, 0xfe0f, "out%t\t%na,%1r" },
	{ 0x3a02, 0xfe07, "out%0y3di%t\t(%xn5r),(%x1r),%w6r" },
	{ 0x1700, 0x3f00, "pop\t%0B,(%x1r)" },
	{ 0x1500, 0x3f00, "popl%lm\t%l0B,(%x1r)" },
	{ 0x1300, 0x3f00, "push\t(%x1r),%w0B" },
	{ 0x1100, 0x3f00, "pushl%lm (%x1r),%0B" },
	{ 0x0d09, 0xff0f, "push\t(%x1r),$%o" },
	{ 0x2200, 0x3e00, "res%t\t%1z0B,$%e4a" },
	{ 0x2200, 0xfef0, "res%t\t%n6r,%0r" },
	{ 0x8d03, 0xff0f, "resflg %1f" },
	{ 0x9e08, 0xffff, "ret" },
	{ 0x9e00, 0xfff0, "ret\t%0c" },
	{ 0xb200, 0xfe01, "r%0y2rl%y3c*%t\t%1r,$%0y121" },
	{ 0xbc00, 0xfd00, "r%2y1lrdb\t%b0r,%1r" },
	{ 0xb600, 0xbe00, "sbc%t\t%0r,%1r" },
	{ 0x7f00, 0xff00, "sys\t%s" },
	{ 0xb203, 0xfe07, "sd%0y3al%t\t%1r,%n6r" },
	{ 0xb30f, 0xff0f, "sd%0y3all\t%l1r,%n6r" },
	{ 0xb201, 0xfe07, "s%n7y3rl%0y3al%t\t%1r,$%i|a" },
	{ 0xb305, 0xff07, "s%n7y3rl%0y3all%lm\t%1r,$%i|a" },
	{ 0x2400, 0x3e00, "set%t\t%1z0B,$%e4a" },
	{ 0x2400, 0xfef0, "set%t\t%n6r,%w0r" },
	{ 0x8d01, 0xff0f, "setflg %1f" },
	{ 0x3a05, 0xfe0f, "sin%t\t%1r,%na" },
	{ 0x3a01, 0xfe07, "sin%0y3di%n4y3*r%t\t(%x5r),(%x1r),%w6r" },
	{ 0x3a07, 0xfe0f, "sout%t\t%na,%1r" },
	{ 0x3a03, 0xfe07, "so%n4y3u*t%0y3di%4y3*r%t\t(%x5r),(%x1r),%w6r" },
	{ 0x0200, 0x3e00, "sub%t\t%0r,%1A" },
	{ 0x1200, 0x3f00, "subl%lm\t%0r,%1A" },
	{ 0xae00, 0xfe00, "tcc%t\t%0c,%1r" },
	{ 0x0c04, 0x3e0f, "test%t\t%1B" },
	{ 0x1c08, 0x3f0f, "testl%lm %1B" },
	{ 0xb800, 0xff01, "tr%0y1t*%y3di%y2r*b\t(%x1r),(%nx5r),%w6r" },
	{ 0x0c06, 0x3e0f, "tset%t\t%1B" },
	{ 0x0800, 0x3e00, "xor%t\t%0r,%1A" },
	{ 0, 0, 0 }
};
/* 112 records + terminator, data 0x08e0..0x0c68 */

/*
 * 0x06c4  dispfmts[] -- printf formats for the display command, indexed
 * (size * 4 + radix): size b,w,l,v (v = 3-byte value) x radix d,u,o,x.
 * Owned by numfmt.
 */
char *dispfmts[] = {
	"%4d",   "%3u",   "%04o",   "%02x",
	"%6d",   "%5u",   "%07o",   "%04x",
	"%10ld", "%11lu", "%012lo", "%08lx",
	"%8ld",  "%8lu",  "%09lo",  "%06lx",
};


/* 0x0768  ccname[] -- condition-code names, indexed by the cc field (%c) */
char *ccname[] = {
	"nun", "lt", "le",  "ule", "ov",  "mi", "eq", "ult",
	"un",  "ge", "gt",  "ugt", "nov", "pl", "ne", "nc",
};

/* 0x07a8  ctlname[] -- control-register names for ldctl, index w & 7 (%k) */
char *ctlname[] = {
	"?", "FLAGS", "FCW", "REFRESH", "PSAPSEG", "PSAPOFF", "NSPREG", "NSPOFF",
};

/* 0x07c8  intname[] -- interrupt-mask names for ei/di, index w & 3 (%v) */
char *intname[] = {
	"VI, NVI", "VI", "NVI", "",
};

/*
 * 0x07d8  sysname[] -- system-call names by call number (%s), 0..65.
 * NOTE: the interpreter's range check admits index 66 (0x42) as well,
 * which in the binary reads the first word pair of optab[] as a pointer.
 */
char *sysname[] = {
	0,         "exit",    "fork",    "read",    "write",   "open",
	"close",   "wait",    "creat",   "link",    "unlink",  "exece",
	"chdir",   0,         "mknod",   "chmod",   "chown",   "brk",
	"stat",    "lseek",   "getpid",  "mount",   "umount",  "setuid",
	"getuid",  "stime",   "ptrace",  "alarm",   "fstat",   "pause",
	"utime",   0,         0,         "access",  "nice",    "ftime",
	"sync",    "kill",    0,         0,         0,         "dup",
	"pipe",    "times",   "profil",  "unique",  "setgid",  "getgid",
	"signal",  0,         0,         "acct",    0,         0,
	"ioctl",   0,         "getegid", "geteuid", 0,         0,
	"umask",   "chroot",  0,         0,         "sload",   "suload",
};

/* ------------------------------------------------------------------ */
/* code                                                               */
/* ------------------------------------------------------------------ */

/* 0x59b2  disasm(buf, space) -- disassemble the instruction at dot in
 * `space' into buf; returns a pointer to the terminating NUL, or NULL
 * when the opcode word cannot be read.  Unknown words print as .word. */
char *
disasm(buf, space)
register char *buf;
int space;
{
	register struct optab *op;
	register char *r;
	int w;

	if (rdmem(space, (char *)&w, 2) == 0)
		return (0);
	for (op = optab; op->o_fmt != 0; op++) {
		if ((op->o_mask & 0xc000) == 0 && (w & 0xc000) == 0xc000)
			continue;
		if ((w & op->o_mask) != op->o_op)
			continue;
		if ((r = dofmt(buf, space, op->o_fmt, w)) != 0)
			return (r);
	}
	sprintf(buf, ".word\t%04x", w);
	return (buf + 10);
}

/* 0x5a6c  dofmt(buf, space, fmt, w) -- expand disassembler format `fmt'
 * for instruction word `w' into buf (see the directive table at the top of
 * this file); returns the new end of buf, or NULL when the format does not
 * apply to this word (%z) or an operand word cannot be read (%n %o %S). */
char *
dofmt(buf, space, fmt, w)
register char *buf;
int space;
register char *fmt;
int w;
{
	long lval;			/* 0x10  4-byte immediate */
	int f;				/* 0x14  selected 4-bit field */
	int mode;			/* 0x16  addressing mode, bits 15:14 of w */
	int v;				/* 0x18  working operand word */
	int size;			/* 0x1a  operand size for this directive */
	int dsize;			/* 0x1c  default operand size */
	int c2;				/* 0x1e */
	int c1;				/* 0x20 */
	int n;				/* 0x22 */
	int nflag;			/* 0x24  NOTE: never initialised in the original */
	int c;				/* 0x26 */
	register int i;			/* r12 */
	register int m;			/* r12 (register-number mask) */
	register char *p;

	mode = ((unsigned)w >> 14) & 3;
	f = w & 0xf;
	v = w;
	dsize = 2;
	for (;;) {
		if ((c = *fmt++) == 0)
			return (buf);
		if (c != '%') {
			*buf++ = c;
			continue;
		}
		size = dsize;
	next:
		c = *fmt++;
		switch (c) {

		case '0': case '1': case '2': case '3':
			f = ((unsigned)w >> ((c - '0') << 2)) & 0xf;
			goto next;

		case '4': case '5': case '6': case '7':
			f = ((unsigned)v >> ((c - '4') << 2)) & 0xf;
			goto next;

		case 'A':
			if (f == 0 && mode == 0) {
				*buf++ = '$';
				goto immed;
			}
			/* fall through: operand by mode */
		case 'B':
			switch (mode) {
			case 0:
				p = "(%xr)";
				break;
			case 1:
				p = (f == 0) ? "%S" : "%S(%r)";
				break;
			case 2:
				goto reg;
			case 3:
				*buf++ = '?';
				continue;
			}
			buf = dofmt(buf, space, p, f);
			continue;

		case 'S':
			if (rdmem(space, (char *)&v, 2) == 0)
				return (0);
			if ((v & 0x8000) == 0) {
				buf = symstr(buf, 0, (long)((unsigned)v >> 8), 0);
				*buf++ = '|';
				buf = symstr(buf, 0, (long)(v & 0xff), 0);
			} else {
				buf = symstr(buf, 0, (long)(((unsigned)v >> 8) & 0x7f), 0);
				*buf++ = '|';
				if (rdmem(space, (char *)&v, 2) == 0)
					return (0);
				buf = symstr(buf, 0, (long)(unsigned)v, 0);
			}
			continue;

		case 'a':
			m = (v > -256 && v < 256) ? -1 : 0;
			buf = symstr(buf, 0, (long)(unsigned)v, m);
			continue;

		case 'b':
			size = 1;
			goto next;

		case 'c':
			p = ccname[f];
			goto cat;

		case 'd':
			switch (*fmt++) {
			case '+':
				v = (int)(dot + (unsigned)(v + v));
				break;
			case '-':
				v = (int)(dot - (unsigned)(v + v));
				break;
			case '.':
				v = (int)(dot + (unsigned)v);
				break;
			}
			goto next;

		case 'e':
			c1 = *fmt++;
			n = (c1 <= '9') ? c1 - '0' : c1 - 'a' + 10;
			v &= (unsigned)0xffff >> (16 - n);
			goto next;

		case 'f':
			for (i = 0; i < 4; i++) {
				if ((f & (1 << (3 - i))) == 0)
					continue;
				if (nflag++ != 0)
					*buf++ = ',';
				*buf++ = "czsv"[i];
			}
			continue;

		case 'i':
			c = *fmt++;
			if (c == '+')
				v++;
			else if (c == '|') {
				if (v < 0)
					v = -v;
			}
			goto next;

		case 'k':
			p = ctlname[w & 7];
			goto cat;

		case 'l':
		case 'x':
			size = 4;
			goto next;

		case 'm':
			dsize = size;
			continue;

		case 'n':
			if (rdmem(space, (char *)&v, 2) == 0)
				return (0);
			goto next;

		case 'o':
		immed:
			switch (size) {
			case 1:
				if (rdmem(space, (char *)&v, 2) == 0)
					return (0);
				buf = symstr(buf, 0, (long)((unsigned)v >> 8), -1);
				break;
			case 2:
				if (rdmem(space, (char *)&v, 2) == 0)
					return (0);
				buf = symstr(buf, 0, (long)(unsigned)v, -1);
				break;
			case 4:
				if (rdmem(space, (char *)&lval, 4) == 0)
					return (0);
				buf = symstr(buf, 0, lval, -1);
				break;
			}
			continue;

		case 'p':
			buf = symstr(buf, 0, (long)(unsigned)v, 0x7fff);
			continue;

		case 'q':
			size = 8;
			goto next;

		case 'r':
		reg:
			*buf++ = 'r';
			switch (size) {
			case 1:
				*buf++ = (f > 7) ? 'l' : 'h';
				m = 7;
				break;
			case 2:
				m = 0xf;
				break;
			case 4:
				*buf++ = 'r';
				m = 0xe;
				break;
			case 8:
				*buf++ = 'q';
				m = 0xc;
				break;
			/* NOTE: other sizes use whatever r12 held (unreachable) */
			}
			sprintf(buf, "%d", f & m);
			goto skip;

		case 's':
			i = w & 0xff;
			if (i <= 0x42 && (p = sysname[i]) != 0)
				goto cat;
			n = i - 0x80;
			if ((unsigned)n <= 3 && (p = trapname[n]) != 0)
				goto cat;
			sprintf(buf, "%02x", i);
			goto skip;

		case 't':
			if (w & 0x100) {
				size = 2;
				dsize = size;
				continue;
			}
			*buf++ = 'b';
			size = 1;
			dsize = size;
			continue;

		case 'u':
			c1 = *fmt++;
			n = (c1 <= '9') ? c1 - '0' : c1 - 'a' + 10;
			m = 1 << (n - 1);
			if (v & m)
				v -= m + m;
			goto next;

		case 'v':
			p = intname[w & 3];
			goto cat;

		case 'w':
			size = 2;
			goto next;

		case 'y':
			n = *fmt++ - '0';
			c1 = *fmt++;
			c2 = *fmt++;
			c = (f & (1 << n)) ? c1 : c2;
			if (c == '*')
				continue;
			*buf++ = c;
			continue;

		case 'z':
			n = *fmt++ - '0';
			if (mode != n)
				goto next;
			if (f != 0)
				goto next;
			return (0);

		default:
			*buf++ = c;
			continue;
		}
	cat:
		while (*p != 0)
			*buf++ = *p++;
		continue;
	skip:
		while (*buf != 0)
			buf++;
		continue;
	}
}

/* 0x6282  numfmt(fc, rc) -- printf format for the display command: size
 * letter fc (b w l v; h = w; f/F = float) and radix letter rc (d u o x).
 * Returns "?" for an unknown letter. */
char *
numfmt(fc, rc)
register int fc;
register int rc;
{
	register char *p;
	register int sz;
	register int rad;

	if (fc == 'f' || fc == 'F')
		return ("%g");
	if (fc == 'h')
		fc = 'w';
	if ((p = index("bwlv", fc)) == 0)
		return ("?");
	sz = p - "bwlv";
	if ((p = index("duox", rc)) == 0)
		return ("?");
	rad = p - "duox";
	return (dispfmts[sz * 4 + rad]);
}

/* 0x631e  putnum(buf, val) -- print a plain number: decimal when the low
 * word is below 16, else 0x-prefixed long hex.  (The comparison looks at
 * the low 16 bits of val only.) */
putnum(buf, val)
char *buf;
long val;
{
	if ((unsigned)(int)val < 16)
		sprintf(buf, "%d", (int)val);
	else
		sprintf(buf, "0x%lx", val);
}
