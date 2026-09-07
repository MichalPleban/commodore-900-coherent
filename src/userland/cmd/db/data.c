/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * db - global data.  Initialised items are given with the data-segment
 * offset they had in the shipped binary; the rest is bss.
 */
#include "db.h"

char	*libfile = "/lib/slibc.a";			/* 0x0006 */
char	*spacename[NSPACE] = {				/* 0x00dc */
	"Data space", "Instruction space", "User area",
};
int	radix = 16;					/* 0x04da */
char	nullstr[] = "";					/* 0x0589 */
int	bptinst = BPTINSTR;				/* 0x06c2 */

/* 0x0704  u-area offset of rN relative to UREGS (r14, r15 come first) */
int	regoff[16] = {
	0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e, 0x0010, 0x0012,
	0x0014, 0x0016, 0x0018, 0x001a, 0x001c, 0x001e, 0x0000, 0x0002,
};

/*
 * 0x0724  names of the trap system calls sc #0x80.. followed, at 0x0730,
 * by the fault names indexed by signal number - 1 (signame).
 */
char	*trapname[] = {
	"sgrow", "bpt", "halt",
	"Hangup",
	"Interrupt",
	"Quit",
	"Alarm clock",
	"Termination signal",
	"Restart",
	"Bad argument to system call",
	"Write on open pipe",
	"Kill",
	"Breakpoint",
	"Segmentation violation",
	"Unimplemented instruction",
	"Privileged instruction",
	"NVI/Step trap",
};

FILE	*libfp;			/* 0x1924  /lib/slibc.a */
struct dbhdr libhdr;		/* 0x1928  its header */
int	dotspace;		/* 0x196a  space of `.' */
int	intrflag;		/* 0x196c  SIGINT seen */
int	stepcnt;		/* 0x196e  remaining `:s' count */
struct map *maps[NSPACE];	/* 0x1970  map list heads: data, instruction, u-area */
char	linebuf[256];		/* 0x197c  command tail / display line */
long	dot;			/* 0x1a7c  memory access cursor */
int	runreq;			/* 0x1a80  run mode requested by `:s'/`:sc' */
int	runmode;		/* 0x1a82  run mode in effect */
long	lastaddr;		/* 0x1a84  address after the last display */
struct map *libmaps;		/* 0x1a88  shared-library maps (slibimap) */
char	dfmt[NSPACE][64];	/* 0x1a8c  default display format per space */
int	swapmode;		/* 0x1b4c  byte-swap words on target access */
struct insrc *insrc;		/* 0x1b4e  command input stack */
int	tmpbpt;			/* 0x1b52  temporary breakpoint planted */
FILE	*auxfp;			/* 0x1b54  core / dump / raw file */
int	slibflag;		/* 0x1b58  object uses the shared library */
char	*stepcmds;		/* 0x1b5a  commands run after each step */
long	nsyms;			/* 0x1b5e  symbols in the table (its low word was the int at 0x1b60) */
int	dsize;			/* 0x1b62  size of the datum last displayed */
struct symidx *symidx;		/* 0x1b64  in-core symbol index */
int	childpid;		/* 0x1b68 */
int	regs[16];		/* 0x1b6a  r0..r15 */
long	regpc;			/* 0x1b8a */
int	regfcw;			/* 0x1b8e */
char	*objname;		/* 0x1b90  program file name */
int	ptword;			/* 0x1b94  cached ptrace word */
long	ptaddr;			/* 0x1b96  its address */
FILE	*objfp;			/* 0x1b9a  object file */
int	kernmode;		/* 0x1b9e  -k */
struct bpt bpts[NBPT];		/* 0x1ba0 */
long	dotaddr;		/* 0x1d20  the user-visible `.' */
FILE	*symfp;			/* 0x1d24  symbol table file */
int	bptsave;		/* 0x1d28  word under the temporary breakpoint */
int	rflag;			/* 0x1d2a  -r */
int	sflag;			/* 0x1d2c  -s */
long	symoff;			/* 0x1d2e  symbol table file offset */
char	*faultname;		/* 0x1d32 */
int	ptspace;		/* 0x1d36  space of the cached ptrace word, -1 none */
char	*lasterr;		/* 0x1d38  last error message (`?') */
int	objloaded;		/* 0x1d3c */
int	ungotc;			/* 0x1d3e  one-character pushback */
int	regsok;			/* 0x1d40  register image valid */
int	stepmode;		/* 0x1d42  resume with ptrace 9 */
int	childalive;		/* 0x1d44 */
