/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - object maps, registers and ptrace (binary 0x4c42..0x59b1).
 *
 * Object-file address maps, raw-file mapping, the child's register image,
 * register-name symbols, the temporary breakpoint that stands in for a
 * hardware single step over `sc' and block instructions, and the ptrace
 * byte access callbacks.
 *
 * Reconstructed from the shipped binary (_disasm/db.asm); the analysis
 * copy with the original addresses is _disasm/db_part5.c.
 */

#include "db.h"

/*
 * l.out header as `db' reads it (48 bytes, canonicalised by canhdr).
 * NOTE: offset 6 is used as a 16-bit int and is the file offset of the
 * first segment (48 for the binary examined); the 32-bit word at 0x2c is
 * canonicalised separately by canhdr.  This does not match the current
 * src/include/l.out.h (vaddr_t l_entry at +6); part 1 owns canhdr.
 */

/*
 * Address map record (26 bytes, allocated by mkmap).  A space is a
 * singly linked list of these; findmap walks them by address range.
 */

/*
 * Register image as it sits in the child's u-area at offset 0x7d2
 * (46 bytes, read by getregs).
 */

#define	UREGS	0x7d2L		/* u-area offset of struct sregs */
#define	UPC	0x7fcL		/* u-area offset of the saved pc */
#define	NREGSZ	0x2e		/* sizeof (struct sregs) */

/* Outside this range */

extern int	errno;

/* 0x4c42 maplout(hp) — build the data/instruction address maps[0] of an object file from its l.out header */
maplout(hp)
register struct dbhdr *hp;
{
	register long base;
	long off;
	long s0, s1, s2, s3, s4;

	if (hp->h_flag & 010)		/* NOTE: LF_KER */
		base = 0x3e0000L;
	else
		base = 0x30000L;
	if (kernmode)
		base = 0x300000L;
	if ((hp->h_flag & 04) == 0)	/* NOTE: LF_NRB */
		base = 0;
	slibflag = hp->h_flag & 040;
	off = hp->h_tbase;
	s0 = hp->h_ssize[0];
	s1 = hp->h_ssize[1];
	s2 = hp->h_ssize[2];
	s3 = hp->h_ssize[3];
	s4 = hp->h_ssize[4];
	switch (hp->h_flag & 03) {
	case 0:
		libmaps = slibimap();
		maps[0] = mkfilemap(slibdmap(libmaps), base, s1 + s0, off);
		maps[0] = mkfilemap(maps[0], base + s1 + s0 + s2, s4 + s3, off + s0 + s1);
		maps[1] = maps[0];
		break;
	case 1:
		maps[0] = mkfilemap(slibimap(), base, s0, off);
		maps[0] = mkfilemap(maps[0], base + s0, s3, off + s0 + s1);
		libmaps = maps[0];
		base = (base + s0 + s3 + 0xffffL) & ~0xffffL;
		maps[0] = mkfilemap(slibdmap(maps[0]), base, s1, off + s0);
		maps[0] = mkfilemap(maps[0], base + s1, s4, off + s1 + s0 + s3);
		maps[1] = maps[0];
		break;
	case 2:
		libmaps = slibimap();
		maps[1] = mkfilemap(libmaps, base, s1 + s0, off);
		base = (base + s0 + s1 + s2 + 0xffffL) & ~0xffffL;
		maps[0] = mkfilemap(slibdmap((struct map *)0), base, s4 + s3, off + s0 + s1);
		break;
	case 3:
		maps[1] = mkfilemap(slibdmap((struct map *)0), base, s0, off);
		maps[1] = mkfilemap(maps[1], (base + s0 + 0xffffL) & ~0xffffL, s1, off + s0);
		base = (base + s0 + s2 + s1 + 0xffffL) & ~0xffffL;
		maps[0] = mkfilemap((struct map *)0, base, s3, off + s0 + s1);
		libmaps = maps[0];
		maps[0] = mkfilemap(slibdmap(maps[0]), (base + s3 + 0xffffL) & ~0xffffL, s4, off + s1 + s0 + s3);
		break;
	}
}

/* 0x4f88 mapraw(name) — map a raw file: u-area and instruction space over the whole file, data space from etext_ (rounded up to 256) if the symbol exists */
mapraw(name)
char *name;
{
	struct ldsym sym;
	register int lo;

	auxfp = dbfopen(name, rflag);
	maps[2] = mkmap((struct map *)0, 0L, 0x7fffffffL, 0L, mapread, mapwrite, 1);
	maps[1] = mkmap((struct map *)0, 0L, 0x7fffffffL, 0L, mapread, mapwrite, 1);
	strncpy(sym.ls_id, "etext_", NCPLN);
	if (findsym(&sym) == 0)
		maps[0] = maps[1];
	else {
		lo = (int)(sym.ls_addr + 0xff) & 0xff00;
		maps[0] = mkmap((struct map *)0, (long)lo, 0x7fffffffL, 0L, mapread, mapwrite, 1);
	}
}

/* 0x50cc mapfile(name) — map a file as a straight array of bytes (-f) */
mapfile(name)
char *name;
{
	mapraw(name);
}

/* 0x50e4 getregs() — fetch the child's register image from its u-area; 1 if ok */
getregs()
{
	struct sregs sr;

	regsok = 0;
	dot = UREGS;
	if (rdmem(2, &sr, NREGSZ) == 0) {
		error("Cannot read registers");
		return (0);
	}
	setregs(&sr);
	regsok = 1;
	return (1);
}

/* 0x5154 setregs(sp) — copy a u-area register image into regs[], regpc and regfcw */
setregs(sp)
register struct sregs *sp;
{
	regs[0] = sp->sr_r[0];
	regs[1] = sp->sr_r[1];
	regs[2] = sp->sr_r[2];
	regs[3] = sp->sr_r[3];
	regs[4] = sp->sr_r[4];
	regs[5] = sp->sr_r[5];
	regs[6] = sp->sr_r[6];
	regs[7] = sp->sr_r[7];
	regs[8] = sp->sr_r[8];
	regs[9] = sp->sr_r[9];
	regs[10] = sp->sr_r[10];
	regs[11] = sp->sr_r[11];
	regs[12] = sp->sr_r[12];
	regs[13] = sp->sr_r[13];
	regs[14] = sp->sr_r14 & 0x7f00;
	regs[15] = sp->sr_r15;
	regpc = sp->sr_pc & 0x7f00ffffL;
	regfcw = sp->sr_fcw;
}

/* 0x5222 regsym(sp) — resolve a register name (pc, rN, rrN, rhN, rlN) into a u-area address symbol; 1 if it was one */
regsym(sp)
register struct ldsym *sp;
{
	register char *p;
	register int c, n;
	int lo, lim;

	if (regsok == 0)
		return (0);
	p = sp->ls_id;
	if (strncmp(p, "pc", NCPLN) == 0) {
		sp->ls_addr = UPC;
		sp->ls_type = 11;
		return (1);
	}
	if (*p++ != 'r')
		return (0);
	lo = 0;
	lim = 15;
	switch (*p++) {
	case 'h':
		lim = 7;
		break;
	case 'l':
		lo = 1;
		lim = 7;
		break;
	case 'r':
		lim = 14;
		break;
	default:
		p--;
		break;
	}
	c = *p++;
	if (c < '0' || c > '9')
		return (0);
	n = c - '0';
	if (n == 1) {
		c = *p++;
		if (c >= '0' && c <= '5')
			n = n * 10 + c - '0';
		else
			p--;
	}
	if (*p != 0)
		return (0);
	if ((n & ~lim) != 0)
		return (0);
	sp->ls_addr = (long)(regoff[n] + lo + (int)UREGS);
	sp->ls_type = 11;
	return (1);
}

/* 0x5342 getpc() — current pc of the traced child */
long
getpc()
{
	return (regpc);
}

/* 0x5354 setpc(pc) — set the child's pc, in the register image and in its u-area */
setpc(pc)
long pc;
{
	regpc = pc;
	dot = UPC;
	wrmem(2, &regpc, 4);
}

/* 0x538e getfp() — frame pointer (r14:r13) of the traced child as a long */
long
getfp()
{
	/* NOTE: the shift is done in 16-bit int arithmetic and yields 0, so
	 * only the offset word survives; kept as compiled. */
	return ((long)(unsigned)regs[13]);		/* was ((regs[14] & 0x7f00) << 16) | regs[13] */
}

/* 0x53b2 getsp() — stack pointer (r14:r15) of the traced child as a long */
long
getsp()
{
	/* NOTE: same 16-bit shift as getfp */
	return ((long)(unsigned)regs[15]);		/* was ((regs[14] & 0x7f00) << 16) | regs[15] */
}

/* 0x53d6 dumpregs() — print pc, fcw and r0..r15, four per line */
dumpregs()
{
	register int i;

	print("pc =%08X fcw=%04x\n", regpc, regfcw);
	for (i = 0; i < 16; i++) {
		if ((i & 3) == 0 && intrseen())
			break;
		print("r%02d=%04x%s", i, regs[i], (i & 3) == 3 ? "\n" : " ");
	}
}

/* 0x545a tmpundo() — after a stop: if a temporary breakpoint was planted, back the pc up over it and restore the original word; 1 if ok */
tmpundo()
{
	long pc;
	char *msg;

	ptspace = -1;
	if (tmpbpt) {
		dot = UPC;
		if (rdmem(2, &pc, 4) == 0) {
			msg = "Cannot get pc";
			goto bad;
		}
		pc -= 2;
		dot = UPC;
		if (wrmem(2, &pc, 4) == 0) {
			msg = "Cannot set pc";
			goto bad;
		}
		dot = pc & 0x7f00ffffL;
		if (wrmem(1, &bptsave, 2) == 0) {
			msg = "Cannot set sin";
			goto bad;
		}
		stepmode = 1;
	}
	return (1);
bad:
	error(msg);
	return (0);
}

/* 0x5530 stepprep() — before resuming in step mode: if the instruction at pc cannot be hardware-stepped, plant a temporary breakpoint after it and continue instead; 1 if ok */
stepprep()
{
	int w;
	register int n;
	char *msg;

	tmpbpt = 0;
	if (stepmode) {
		dot = regpc;
		if (rdmem(1, &w, 2) == 0) {
			msg = "Cannot get sin";
			goto bad;
		}
		if ((n = steplen(w)) != 0) {
			dot = regpc + n;
			if (rdmem(1, &bptsave, 2) == 0) {
				msg = "Cannot get breakpoint";
				goto bad;
			}
			dot -= 2;
			if (wrmem(1, &bptinst, 2) == 0) {
				msg = "Cannot set breakpoint";
				goto bad;
			}
			stepmode = 0;
			tmpbpt = 1;
		}
	}
	return (1);
bad:
	error(msg);
	return (0);
}

/* 0x561a chkcall() — if the instruction at pc is a segmented call (0x5f00), switch the step sub-mode to 3 (run over the call) */
chkcall()
{
	int w;

	dot = regpc;
	if (rdmem(1, &w, 2) && w == 0x5f00)
		runmode = 3;
}

/* 0x5660 retbpt() — plant a kind-4 (return) breakpoint at the return address on top of the child's stack, tagged with its frame pointer */
retbpt()
{
	int w;

	dot = (unsigned)regs[15];
	if (rdmem(0, &w, 2))
		bptenter(4, w, getfp(), 0L);
}

/* 0x56b8 isbpt(addr) — 1 if the instruction word at addr is the breakpoint instruction (sc #0x81) */
isbpt(addr)
long addr;
{
	int w;

	dot = addr;
	if (rdmem(1, &w, 2) && w == 0x7f81)
		return (1);
	return (0);
}

/* 0x56fa ptread(space, addr, buf, n) — map read callback for a traced child: n bytes via ptrace; 1 if ok */
ptread(space, addr, buf, n)
int space;
register long addr;
register char *buf;
int n;
{
	while (n-- != 0) {
		if (ptgetb(space, addr, buf) == 0)
			return (0);
		addr++;
		buf++;
	}
	return (1);
}

/* 0x574c ptwrite(space, addr, buf, n) — map write callback for a traced child: n bytes via ptrace, merging the odd end bytes with what is there; 1 if ok */
ptwrite(space, addr, buf, n)
int space;
register long addr;
register char *buf;
register int n;
{
	int w;			/* the word to write; its two bytes are filled separately */

	if (n != 0 && (addr & 1)) {
		addr--;
		if (ptgetb(space, addr, (char *)&w) == 0)
			return (0);
		((char *)&w)[1] = *buf++;
		ptrace(space + 4, childpid, addr, w);
		if (errno)
			return (0);
		addr += 2;
		n--;
	}
	while (n > 1) {
		((char *)&w)[0] = *buf++;
		((char *)&w)[1] = *buf++;
		ptrace(space + 4, childpid, addr, w);
		if (errno)
			return (0);
		addr += 2;
		n -= 2;
	}
	if (n != 0) {
		((char *)&w)[0] = *buf++;
		if (ptgetb(space, addr + 1, (char *)&w + 1) == 0)
			return (0);
		ptrace(space + 4, childpid, addr, w);
		if (errno)
			return (0);
	}
	return (1);
}

/* 0x5876 ptgetb(space, addr, buf) — read one byte of the child through a one-word ptrace cache; 1 if ok */
ptgetb(space, addr, buf)
int space;
long addr;
char *buf;
{
	register long a;

	a = addr & ~1L;
	if (a != ptaddr || space != ptspace) {
		errno = 0;
		ptword = ptrace(space + 1, childpid, a, 0);
		if (errno) {
			ptspace = -1;
			return (0);
		}
		ptspace = space;
		ptaddr = a;
	}
	if (addr & 1)
		*buf = ((char *)&ptword)[1];
	else
		*buf = ((char *)&ptword)[0];
	return (1);
}

/* 0x590c steplen(w) — length of an instruction that cannot be single-stepped by the hardware (sc = 2; block/repeat forms = 4), else 0 */
steplen(w)
register int w;
{
	register int t;

	if ((w & 0xff00) == 0x7f00)
		return (2);
	if ((w & 0xfe01) == 0xba00)
		return (4);
	if ((w & 0xff01) == 0x6800)	/* NOTE: opcode class not identified */
		return (4);
	t = w & 0xfe07;
	if ((t & 0xfffc) == 0x3a00 || t == 0x3a08 || t == 0xba01)
		return (4);
	return (0);
}

/* 0x596e xptrace(req, pid, addr, data) — debugging ptrace wrapper that traces its arguments; NOT referenced by any caller */
xptrace(req, pid, addr, data)
int req, pid;
long addr;
int data;
{
	printf("ptrace( %d, %d, %lx, %x)\n", req, pid, addr, data);
	return (ptrace(req, pid, addr, data));
}
