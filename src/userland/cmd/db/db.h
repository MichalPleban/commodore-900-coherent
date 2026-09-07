/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - the Coherent assembler-level symbolic debugger (Z8001).
 *
 * Reconstructed from the shipped binary (see _disasm/db.asm and the
 * per-range analyses _disasm/db_part[1-6].c); no original source was
 * consulted.  Every function keeps the address it had in the binary
 * in its header comment so the reconstruction can be diffed against
 * the disassembly.
 *
 * Address spaces: 0 = data, 1 = instruction, 2 = u-area.  Each space is a
 * linked list of `struct map' records (maps[space]) describing which
 * address ranges are backed by which file region or ptrace access.
 */

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <canon.h>
#include <l.out.h>
#include <string.h>

#define	NBPT	16		/* breakpoint slots */
#define	NEXPR	10		/* expression list length (`=' patch values) */
#define	NARG	19		/* argv slots built for the child */
#define	NSPACE	3		/* address spaces */
#define	BPTINSTR 0x7f81		/* `sc #0x81' -- the breakpoint instruction */

#define	LF_SLIBC 020		/* l_flag: object references /lib/slibc.a */
#define	LF_SYMS	040		/* l_flag: symbol table present */

#define	UREGS	0x7d2L		/* u-area offset of struct sregs */
#define	UPC	0x7fcL		/* u-area offset of the saved pc */
#define	USIZE	0x800L		/* size of the u-area mapping */
#define	UFAULT	0x25cL		/* u-area offset of the fault number */
#define	UCSEG	0x64L		/* u-area offset of the core segment table */
#define	NREGSZ	0x2e		/* sizeof (struct sregs) */

/*
 * The l.out header as it sits in the file (48 bytes, little-endian on
 * disk, byte-swapped by canhdr()).  <l.out.h>'s struct ldheader has the
 * same first four words but no trailing entry point.
 */
struct dbhdr {
	int	h_magic;	/* 0x00  L_MAGIC (0407) */
	int	h_flag;		/* 0x02  LF_* */
	int	h_machine;	/* 0x04 */
	int	h_tbase;	/* 0x06  file offset of the first segment */
	long	h_ssize[NLSEG];	/* 0x08  segment sizes */
	long	h_entry;	/* 0x2c  entry point */
};

/*
 * One address-space map record: target addresses [m_lo, m_hi) live at
 * offset m_off of whatever the callbacks m_read/m_write operate on;
 * m_which is handed to them as their first argument.
 */
struct map {
	struct map *m_next;	/* 0x00 */
	long	m_lo;		/* 0x04 */
	long	m_hi;		/* 0x08  lo + size */
	long	m_off;		/* 0x0c */
	int	(*m_read)();	/* 0x10  (which, off, buf, n) */
	int	(*m_write)();	/* 0x14  (which, off, buf, n) */
	int	m_which;	/* 0x18 */
};

/*
 * A breakpoint slot (24 bytes).  b_flags: 1 = user breakpoint (`:b',
 * commands in b_cmd), 2 = return breakpoint matched on frame b_frame
 * (commands in b_rcmd), 4 = "step over call" return breakpoint on
 * frame b_frame4 (planted by retbpt()).
 */
struct bpt {
	int	b_flags;	/* 0x00 */
	long	b_addr;		/* 0x02 */
	long	b_frame;	/* 0x06 */
	long	b_frame4;	/* 0x0a */
	char	*b_cmd;		/* 0x0e  malloc'd, 128 bytes */
	char	*b_rcmd;	/* 0x12  malloc'd, 128 bytes */
	int	b_save;		/* 0x16  instruction word under the breakpoint */
};

/*
 * A parsed expression (6 bytes).  e_flags: 1 = empty (no expression
 * given), 2 = e_space valid.
 */
struct opnd {
	char	e_flags;
	char	e_space;	/* 0 data, 1 instruction, 2 u-area */
	long	e_val;
};

/*
 * Command-input source stack element (10 bytes).
 */
struct insrc {
	struct insrc *i_next;
	int	i_kind;		/* 1 = FILE, 2 = string */
	char	*i_ptr;		/* FILE * or string pointer */
};

/*
 * In-core symbol index entry (6 bytes per symbol).
 */
struct symidx {
	char	x_hash;		/* symhash() of the name */
	char	x_type;
	long	x_value;
};

/*
 * Register image as it sits in the child's u-area at UREGS (46 bytes).
 */
struct sregs {
	int	sr_r14;		/* +0    r14 (stack segment) */
	int	sr_r15;		/* +2    r15 (stack offset) */
	int	sr_r[14];	/* +4    r0..r13 */
	int	sr_x[4];	/* +0x20 not used by db */
	int	sr_fcw;		/* +0x28 */
	long	sr_pc;		/* +0x2a (u-area UPC) */
};

/*
 * Segment descriptor as found in a core file's u-area copy
 * (8 of them, 14 bytes each, at UCSEG).
 */
struct coreseg {
	int	cs_flags;	/* 0x00  bits 0 and 1 both set => dumped */
	long	cs_base;	/* 0x02  virtual address */
	long	cs_size;	/* 0x06  bytes in the core file */
	long	cs_x;		/* 0x0a  non-zero when the segment exists */
};

/*
 * Disassembler opcode match record.
 */
struct optab {
	unsigned o_op;		/* opcode bits */
	unsigned o_mask;	/* significant bits */
	char	*o_fmt;		/* format string */
};

/* main.c */
extern struct map *slibimap(), *slibdmap(), *coremap1(), *mkmap(), *freemaps();
extern FILE	*dbfopen();
/* run.c */
/* cmd.c */
extern char	*fmtitem(), *escchar(), *disptime(), *savestr();
/* expr.c */
extern long	evalue(), evalue2(), toseg(), fromseg();
extern struct map *findmap(), *mkfilemap();
extern char	*symstr(), *alloc();
/* sym.c */
extern long	getpc(), getfp(), getsp();
/* dis.c */
extern char	*disasm(), *dofmt(), *numfmt();
/* every int-valued function, so any of them can be used as a pointer */
extern int	bptclear(), bptclr(), bptdel(), bptenter(), bpterror(), bptlist(), 
	bptprint(), bptset(), canhdr(), chkcall(), cmdloop(), colon(), coreopen(), 
	dbinit(), display(), docmds(), dumpregs(), eempty(), eol(), error(), 
	espace(), execcmd(), expr(), exprlist(), fatal(), fileopen(), findsym(), 
	freeallmaps(), getch(), getfault(), getregs(), getrest(), intr(), intrseen(), 
	isbpt(), killchild(), lookup(), mapbound(), mapfile(), maplist(), maplout(), 
	mapraw(), mapread(), mapwrite(), mapxfer(), nearsym(), number(), 
	objopen(), outch(), patch(), print(), prsym(), ptgetb(), ptread(), 
	ptwrite(), pushfile(), pushstr(), putnum(), quit(), rdmem(), regsym(), 
	release(), reloadobj(), retbpt(), runchild(), setpc(), setregs(), setup(), 
	shellcmd(), showline(), slibread(), slibwrite(), spacesym(), startchild(), steplen(), 
	stepprep(), stopstate(), swapwords(), symcmp(), symhash(), symindex(), symopnd(), 
	synerr(), tmpundo(), token(), traceback(), ungetch(), untoken(), unwind(), 
	unwind1(), usage(), waitchild(), wrmem(), xptrace();

/* data.c */
extern char	*libfile;
extern char	*spacename[NSPACE];
extern int	radix;
extern char	nullstr[];
extern int	bptinst;
extern char	*dispfmts[];
extern int	regoff[16];
extern char	*trapname[];
#define	signame	(trapname + 3)
extern char	*ccname[];
extern char	*ctlname[];
extern char	*intname[];
extern char	*sysname[];
extern struct optab optab[];

extern FILE	*libfp;
extern struct dbhdr libhdr;
extern int	dotspace;
extern int	intrflag;
extern int	stepcnt;
extern struct map *maps[NSPACE];
extern char	linebuf[256];
extern long	dot;
extern int	runreq;
extern int	runmode;
extern long	lastaddr;
extern struct map *libmaps;
extern char	dfmt[NSPACE][64];
extern int	swapmode;
extern struct insrc *insrc;
extern int	tmpbpt;
extern FILE	*auxfp;
extern int	slibflag;
extern char	*stepcmds;
extern long	nsyms;
extern int	dsize;
extern struct symidx *symidx;
extern int	childpid;
extern int	regs[16];
extern long	regpc;
extern int	regfcw;
extern char	*objname;
extern int	ptword;
extern long	ptaddr;
extern FILE	*objfp;
extern int	kernmode;
extern struct bpt bpts[NBPT];
extern long	dotaddr;
extern FILE	*symfp;
extern int	bptsave;
extern int	rflag;
extern int	sflag;
extern long	symoff;
extern char	*faultname;
extern int	ptspace;
extern char	*lasterr;
extern int	objloaded;
extern int	ungotc;
extern int	regsok;
extern int	stepmode;
extern int	childalive;

extern char	*ctime();
extern char	*malloc();
extern long	lseek();
extern int	(*signal())();
