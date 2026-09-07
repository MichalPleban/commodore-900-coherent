/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - start-up and file mapping (binary 0x001c..0x1575).
 *
 * main(), option parsing, the object / core / dump / raw-file mapping
 * modes, the shared-library (/lib/slibc.a) maps, the l.out header and
 * symbol-index readers, the stack traceback (`:t') and the memory-map
 * records every space is described by.
 *
 * Reconstructed from the shipped binary (_disasm/db.asm); the analysis
 * copy with the original addresses is _disasm/db_part1.c.
 */

#include "db.h"

/*
 * The l.out header as this db reads it (48 bytes — see canhdr):
 * NOTE: differs from <l.out.h>'s struct ldheader: a 16-bit field at
 * offset 6 (the file offset of the first segment = header size), the
 * nine segment sizes at offset 8, and the entry point last at 0x2c.
 */

/*
 * One address-space map record (26 bytes, built by mkmap): the
 * addresses [m_lo, m_hi) of a space live at file offset m_off of
 * the file the callbacks m_read/m_write operate on; m_which is handed
 * to those callbacks as their first argument.
 */

/*
 * Segment descriptor as found in a core file's u-area copy
 * (8 of them, 14 bytes each, at core file offset 0x64).
 * NOTE: only the fields db looks at are understood.
 */

/* externs — other parts */

/* externs — globals (canonical; see db_part1.names) */


/* 0x001c slibread(arg, off, buf, n) — map read callback for the shared library file: seek and read */
slibread(arg, off, buf, n)
int arg;
long off;
char *buf;
int n;
{
	fseek(objfp, off, 0);
	return (fread(buf, n, 1, objfp));
}

/* 0x0058 slibwrite(arg, off, buf, n) — map write callback for the shared library file: seek, write, flush */
slibwrite(arg, off, buf, n)
int arg;
long off;
char *buf;
int n;
{
	register int r;

	fseek(objfp, off, 0);
	if (fwrite(buf, n, 1, objfp) == 0)
		return (0);
	if ((r = fflush(objfp)) == -1)
		return (0);
	return (1);
}

/* 0x00b4 slibimap() — open the shared library and map its two instruction-space segments; returns the map list */
struct map *
slibimap()
{
	register struct map *m;
	register long off;
	register long base;

	if (slibflag == 0)
		return ((struct map *)0);
	objfp = dbfopen(libfile, rflag);
	if (fread((char *)&libhdr, sizeof(libhdr), 1, objfp) != 1)
		fatal("cannot read %s", libfile);
	canhdr(&libhdr);
	off = libhdr.h_tbase;
	base = 0x1000000L;
	m = mkmap((struct map *)0, base, libhdr.h_ssize[L_SHRI], off,
		slibread, slibwrite, 3);
	off += libhdr.h_ssize[L_PRVI] + libhdr.h_ssize[L_SHRI];
	base += libhdr.h_ssize[L_SHRI];
	m = mkmap(m, base, libhdr.h_ssize[L_SHRD], off, slibread, slibwrite, 3);
	return (m);
}

/* 0x01b6 slibdmap(next) — map the shared library's two data-space segments after `next'; returns the map list */
struct map *
slibdmap(next)
struct map *next;
{
	register struct map *m;
	register long off;
	register long base;

	if (slibflag == 0)
		return ((struct map *)0);
	off = libhdr.h_tbase + libhdr.h_ssize[L_SHRI];
	base = 0x2000000L;
	m = mkmap(next, base, libhdr.h_ssize[L_PRVI], off, slibread, slibwrite, 3);
	off += libhdr.h_ssize[L_SHRD] + libhdr.h_ssize[L_PRVI];
	base += libhdr.h_ssize[L_PRVI];
	m = mkmap(m, base, libhdr.h_ssize[L_PRVD], off, slibread, slibwrite, 3);
	return (m);
}

/* 0x0256 unwind1(retpcp, fpp) — unwind one frame from the child's current pc/fp/sp: caller's return pc and fp */
unwind1(retpcp, fpp)
long *retpcp;
long *fpp;
{
	struct ldsym sym;
	long sp;

	/* NOTE: the original evaluated getsp() (sp) before getfp() (fp) */
	return (unwind(retpcp, fpp, &sp, regpc, getfp(), getsp(), &sym));
}

/* 0x02a2 traceback(depth) — the `:t' command: print a stack traceback of `depth' frames (0 = all) */
traceback(depth)
int depth;
{
	register long pc;
	struct ldsym sym;
	int w2, w1, w0;
	long retpc;
	long sp;
	long fp;

	strcpy(sym.ls_id, "main_");
	if (findsym(&sym) == 0) {
		error("no symbols");
		return;
	}
	pc = regpc;
	fp = getfp();
	sp = getsp();
	for (;;) {
		printf("sp = %08lx fp = %08lx  ", sp, fp);
		if (unwind(&retpc, &fp, &sp, pc, fp, sp, &sym) == 0)
			return;
		printf("pc = %.*s + %04x   ", NCPLN, sym.ls_id, (int)(pc-sym.ls_addr));
		pc = retpc;
		dot = pc - 6;
		rdmem(1, &w0, 2);
		rdmem(1, &w1, 2);
		rdmem(1, &w2, 2);
		if (w0 == 0x5f00 && (w1 & 0x8000))	/* call seg, long form */
			;
		else if (w1 == 0x5f00 && (w2 & 0x8000) == 0)	/* call seg, short form */
			;
		else if ((w2 & 0xf000) == 0xd000)	/* calr */
			;
		else {
			error("improper entry");
			return;
		}
		rdmem(1, &w0, 2);
		rdmem(1, &w1, 2);
		if ((w0 & 0xfff0) == 0xa9f0)		/* inc r15,#k */
			w1 = (w0 & 0xf) + 1;
		else if (w0 != 0x010f)			/* add r15,#n leaves n in w1 */
			w1 = 0;
		if (w1 == 0)
			printf("no args\n");
		else {
			w1 /= 2;
			w2 = 0;
			dot = sp + 4;
			printf("\n  args =");
			while (w1 != 0) {
				w2++;
				w1--;
				rdmem(0, &w0, 2);
				printf(" %4x", w0);
				if (w2 % 10 == 0 && w1 != 0)
					printf("\n         ");
			}
			printf("\n");
		}
		if (intrseen())
			return;
		if (--depth == 0)
			return;
		if (strcmp(sym.ls_id, "main_") == 0)
			return;
	}
}

/* 0x0572 unwind(retpcp, fpp, spp, pc, fp, sp, sym) — unwind one frame: find the function holding pc, decode its prolog, return the caller's pc/fp/sp */
unwind(retpcp, fpp, spp, pc, fp, sp, sym)
long *retpcp;
long *fpp;
register long *spp;
register long pc;
long fp;
long sp;
register struct ldsym *sym;
{
	int w2, w1, w0;
	int save;		/* bytes of registers saved above the frame pointer */
	int frame;		/* bytes of locals */
	long eprolog;		/* address just past the prolog */
	char *msg;

	if (nearsym(1, pc, sym) == 0) {
		msg = "can't locate pc";
		goto bad;
	}
	sym->ls_addr &= 0x7f00ffffL;
	dot = sym->ls_addr;
	if (rdmem(1, &frame, 2) == 0) {
		msg = "can't read prolog";
		goto bad;
	}
	if (frame == 0x030f)			/* sub r15,#n */
		rdmem(1, &frame, 2);
	else if ((frame & 0xfff0) == 0xabf0)	/* dec r15,#k */
		frame = (frame & 0xf) + 1;
	else {
		msg = "bad prolog";
		goto bad;
	}
	rdmem(1, &save, 2);
	if (save == 0x2fed)			/* ld @rr14,r13 */
		save = 0;
	else if (save == 0x1dec)		/* ldl @rr14,rr12 */
		save = 4;	/* NOTE: as coded; the saved r13 is really 2 bytes up */
	else if (save == 0x1ce9) {		/* ldm @rr14,rn,#c */
		rdmem(1, &save, 2);
		save = (save & 0xf) * 2;
	} else {
		msg = "bad prolog";
		goto bad;
	}
	eprolog = dot;
	dot = pc;
	if (rdmem(1, &w0, 2) == 0) {
		msg = "can't read at pc";
		goto bad;
	}
	*fpp = fp;
	if (pc == sym->ls_addr || w0 == 0x9e08)		/* at entry or at `ret' */
		*spp = sp;
	else if (pc <= eprolog)				/* still in the prolog */
		*spp = sp + frame;
	else {
		rdmem(1, &w1, 2);
		rdmem(1, &w2, 2);
		if ((w0 == 0x010f && w2 == 0x9e08)		/* add r15,#n; ret */
		 || ((w0 & 0xfff0) == 0xa9f0 && w1 == 0x9e08))	/* inc r15,#k; ret */
			*spp = sp + frame;
		else {
			*spp = fp + frame;
			dot = fp + save;
			rdmem(0, (char *)fpp + 2, 2);	/* saved r13 -> low word of *fpp */
		}
	}
	dot = *spp;
	rdmem(0, retpcp, 4);
	*retpcp &= 0x7f00ffffL;
	*fpp &= 0x7f00ffffL;
	return (1);
bad:
	error(msg);
	return (0);
}

/* 0x07f8 main(argc, argv) — set up, parse the options, catch SIGINT, ignore SIGQUIT, run the command loop */
main(argc, argv)
int argc;
char *argv[];
{
	dbinit();
	setup(argc, argv);
	signal(SIGINT, intr);
	signal(SIGQUIT, SIG_IGN);
	runchild();
}

/* 0x0848 quit() — quit: kill the child and exit */
quit()
{
	killchild();
	exit(0);
}

/* 0x0866 dbinit() — initialise: default display formats (`w' for data and u-area, `i' for instructions), clear breakpoints */
dbinit()
{
	register int i;

	for (i = 0; i < 3; i++)
		strcpy(dfmt[i], "w");
	strcpy(dfmt[1], "i");
	bptclear();
}

/* 0x08c4 setup(argc, argv) — parse the command line, open/map the files for the chosen mode, -t redirects to /dev/tty */
setup(argc, argv)
int argc;
char *argv[];
{
	register char *p;
	register int c;
	register int fd;
	register int mode;
	int tflag;

	mode = 0;
	tflag = 0;
	for (; argc > 1; argc--, argv++) {
		p = argv[1];
		if (*p++ != '-')
			break;
		while ((c = *p++) != 0) {
			switch (c) {
			case 'c':
			case 'd':
			case 'e':
			case 'f':
			case 'k':
			case 'o':
				if (mode != 0)
					usage();
				mode = c;
				break;
			case 'h':
				kernmode = 1;
				break;
			case 'r':
				rflag = 1;
				break;
			case 's':
				sflag = 1;
				break;
			case 't':
				tflag = 1;
				break;
			default:
				usage();
			}
		}
	}
	switch (mode) {
	case 0:
		switch (argc) {
		case 1:
			objopen("l.out", 3);
			coreopen("core");
			break;
		case 2:
			objopen(argv[1], 3);
			break;
		case 3:
			objopen(argv[1], 3);
			coreopen(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'c':
		switch (argc) {
		case 1:
			objopen("l.out", 3);
			coreopen("core");
			break;
		case 2:
			coreopen(argv[1]);
			break;
		case 3:
			objopen(argv[1], 3);
			coreopen(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'd':
		if (argc == 1) {
			objopen("/coherent", 3);
			mapfile("/dev/dump");
		} else if (argc == 3) {
			objopen(argv[1], 3);
			mapfile(argv[2]);
		} else
			usage();
		break;
	case 'e':
		if (argc < 2)
			usage();
		objopen(argv[1], 3);
		if (startchild(&argv[1], (char *)0, (char *)0, 0) == 0
		 || stopstate() == 0)
			quit();
		break;
	case 'f':
		if (argc == 2)
			fileopen(argv[1]);
		else if (argc == 3) {
			objopen(argv[1], 1);
			fileopen(argv[2]);
		} else
			usage();
		break;
	case 'k':
		switch (argc) {
		case 1:
			objopen("/coherent", 1);
			mapraw("/dev/mem");
			break;
		case 2:
			mapraw(argv[1]);
			break;
		case 3:
			objopen(argv[1], 1);
			mapraw(argv[2]);
			break;
		default:
			usage();
		}
		break;
	case 'o':
		switch (argc) {
		case 1:
			objopen("l.out", 3);
			break;
		case 2:
			objopen(argv[1], 3);
			break;
		case 3:
			objopen(argv[1], 1);
			objopen(argv[2], 2);
			break;
		default:
			usage();
		}
		break;
	}
	if (tflag) {
		if ((fd = open("/dev/tty", 2)) < 0)
			fatal("Cannot open /dev/tty");
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
	}
}

/* 0x0cf8 usage() — usage message and exit */
usage()
{
	fatal("Usage: db [-cdefort] [object file] [auxiliary file]");
}

/* 0x0d14 intr() — SIGINT handler: re-arm and note the interrupt */
intr()
{
	signal(SIGINT, intr);
	intrflag++;
}

/* 0x0d3a intrseen() — was there an interrupt?  Report it, clear it, return the old count */
intrseen()
{
	register int r;

	r = intrflag;
	if (r != 0)
		synerr("Interrupt");
	intrflag = 0;
	return (r);
}

/* 0x0d68 objopen(name, flags) — open an object file and read its header; flags&1: use its symbol table, flags&2: it is the program (map it, honour -r) */
objopen(name, flags)
char *name;
int flags;
{
	struct dbhdr hdr;

	objfp = dbfopen(name, (flags & 2) ? rflag : 1);
	if (fread((char *)&hdr, sizeof(hdr), 1, objfp) != 1)
		fatal("Cannot read object file");
	canhdr(&hdr);
	if (hdr.h_magic != L_MAGIC)
		fatal("Bad object file");
	objloaded = 1;
	if (flags & 1) {
		symoff = hdr.h_tbase;
		symoff += hdr.h_ssize[L_SHRI] + hdr.h_ssize[L_PRVI];
		symoff += hdr.h_ssize[L_SHRD] + hdr.h_ssize[L_PRVD];
		nsyms = hdr.h_ssize[L_SYM] / (long)sizeof(struct ldsym);
		if (nsyms != 0) {
			symfp = objfp;
			if (sflag == 0)
				symindex(symfp);
		}
	}
	if (flags & 2) {
		objname = name;
		maplout(&hdr);
	}
}

/* 0x0ea6 canhdr(hp) — byte-swap an l.out header read from disk into host order */
canhdr(hp)
register struct dbhdr *hp;
{
	register int i;

	canint(hp->h_tbase);
	canint(hp->h_magic);
	canint(hp->h_flag);
	canint(hp->h_machine);
	canlong(hp->h_entry);
	for (i = 0; i < NLSEG; i++)
		canlong(hp->h_ssize[i]);
}

/* 0x0f64 symindex(fp) — read the symbol table into the compact 6-byte-per-symbol index (hash, type, address) */
symindex(fp)
FILE *fp;
{
	register char *p;
	register int n;
	register int r;
	struct ldsym sym;

	symidx = (struct symidx *)alloc((int)nsyms * 6);
	if (symidx == 0)
		return;
	fseek(fp, symoff, 0);
	n = (int)nsyms;
	for (p = (char *)symidx; n-- != 0; p += 6) {
		if ((r = fread((char *)&sym, sizeof(sym), 1, fp)) != 1) {
			release(symidx);
			symidx = 0;
			return;
		}
		canint(sym.ls_type);
		canlong(sym.ls_addr);
		p[0] = symhash(sym.ls_id);
		p[1] = sym.ls_type;
		*(long *)(p+2) = sym.ls_addr;
	}
}

/* 0x104a symhash(name) — symbol name hash: sum of the first 8 characters, `_' excluded, low 8 bits */
symhash(name)
char *name;
{
	register char *p;
	register int n;
	register int h;

	h = 0;
	n = 8;
	p = name;
	do {
		if (*p != '_')
			h += *p;
	} while (*p++ != 0 && --n != 0);
	return (h & 0xff);
}

/* 0x1082 coremap1(next, base, len, foff) — add a core map whose end is computed in linear addressing; NOTE: no caller found (dead code?) */
struct map *
coremap1(next, base, len, foff)
struct map *next;
long base;
long len;
long foff;
{
	register struct map *m;

	m = mkmap(next, base, toseg(fromseg(base) + len) - base, foff,
		mapread, mapwrite, 1);
	return (m);
}

/* 0x10de coreopen(name) — open a core file: check the command name, map the u-area and every dumped segment, fetch fault and registers */
coreopen(name)
char *name;
{
	register struct map *m;
	register int i;
	register char *p;
	register int r;
	struct coreseg segs[8];
	char cmd[10];
	long off;
	long size;

	auxfp = dbfopen(name, rflag);
	fseek(auxfp, 0x230L, 0);		/* NOTE: u_comm offset in the u-area */
	if ((p = objname) != 0) {
		if ((r = fread(cmd, 10, 1, auxfp)) == 1) {
			register char *q;

			for (q = p; *p != 0; )		/* basename of the program */
				if (*p++ == '/')
					q = p;
			if (strncmp(q, cmd, 10) != 0)
				error("Core different from object");
		}
	}
	fseek(auxfp, 0x64L, 0);		/* NOTE: segment table offset in the u-area */
	if ((r = fread((char *)segs, sizeof(segs), 1, auxfp)) != 1)
		fatal("Bad core file");
	maps[2] = mkmap((struct map *)0, 0L, 0x800L, 0L, mapread, mapwrite, 1);
	m = freemaps(maps[0], (struct map *)libmaps);
	off = segs[0].cs_size;
	for (i = 1; i < 8; i++) {
		if (segs[i].cs_x == 0)
			continue;
		if ((~segs[i].cs_flags & 3) != 0)
			continue;
		size = segs[i].cs_size;
		m = mkmap(m, segs[i].cs_base, size, off, mapread, mapwrite, 1);
		off += size;
	}
	if (maps[1] == maps[0])
		maps[1] = m;
	maps[0] = m;
	getfault();
	getregs();
}

/* 0x1300 getfault() — read the fault (signal) number from the child's u-area; sets the fault name; returns the number or 0 */
getfault()
{
	register int i;
	int sig;

	faultname = "???";
	dot = 0x25cL;				/* NOTE: u_signo offset in the u-area */
	if (rdmem(2, &sig, 2) == 0) {
		error("Cannot read fault type");
		return (0);
	}
	if (sig <= 0 || sig > 16) {
		error("Bad fault type");
		return (0);
	}
	i = sig - 1;
	faultname = signame[i];
	return (sig);
}

/* 0x1388 fileopen(name) — -f: map a plain file as one flat space for both data and instructions */
fileopen(name)
char *name;
{
	objfp = dbfopen(name, rflag);
	maps[0] = mkmap((struct map *)0, 0L, 0x7fffffffL, 0L, mapread, mapwrite, 0);
	maps[1] = maps[0];
}

/* 0x13fc dbfopen(name, rdonly) — fopen a file, read/write unless rdonly or not permitted; fatal if it cannot be opened */
FILE *
dbfopen(name, rdonly)
char *name;
int rdonly;
{
	register FILE *fp;

	if (rdonly) {
		if ((fp = fopen(name, "r")) == NULL)
			goto bad;
	} else {
		if ((fp = fopen(name, "r+w")) == NULL) {
			if ((fp = fopen(name, "r")) == NULL)
				goto bad;
			error("%s: opened read only", name);
		}
	}
	return (fp);
bad:
	fatal("Cannot open %s", name);
}

/* 0x148e mkmap(next, base, size, foff, rdfn, wrfn, arg) — allocate and fill one map record */
struct map *
mkmap(next, base, size, foff, rdfn, wrfn, arg)
struct map *next;
long base;
long size;
long foff;
int (*rdfn)();
int (*wrfn)();
int arg;
{
	register struct map *m;

	m = (struct map *)alloc(sizeof(struct map));
	m->m_next = next;
	m->m_lo = base;
	m->m_hi = size + base;
	m->m_off = foff;
	m->m_read = rdfn;
	m->m_write = wrfn;
	m->m_which = arg;
	return (m);
}

/* 0x14e8 freemaps(m, stop) — free the map records from m up to (not including) stop; returns stop */
struct map *
freemaps(m, stop)
register struct map *m;
register struct map *stop;
{
	while (m != stop) {
		release(m);
		m = m->m_next;	/* NOTE: read after free, as in the original */
	}
	return (m);
}

/* 0x1514 freeallmaps() — free all three map lists (a shared data/instruction list is freed once) */
freeallmaps()
{
	register int i;

	i = 0;
	if (maps[0] == maps[1])
		maps[i++] = 0;
	for (; i < 3; i++)
		maps[i] = freemaps(maps[i], (struct map *)0);
}
