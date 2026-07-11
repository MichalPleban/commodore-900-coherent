/*
 * General Loader-Binder
 * 32-bit version
 */
/*
 * Knows about FILE struct to the extent it is revealed in putc
 * if BREADBOX is non-zero
 */
#include <stdio.h>
#include <canon.h>
#include "n.out.h"
#include <ar.h>
#include <stat.h>

#define	LADDR	LADDR
#ifdef	LADDR
typedef	unsigned long	uaddr_t;	/* universal address type */
#else
typedef	unsigned short	uaddr_t;	/* universal address type */
#endif

typedef	struct	{		/* segment descriptor */
	uaddr_t	vbase;		/* virtual address base */
	size_t	daddr,		/* seek addr of segment */
		size;		/* size */
} seg_t;

typedef	struct	sym_t	{	/* symbol descriptor */
	struct	sym_t	*next;	/* chained together */
	struct	ldsym	s;	/* id and value */
	struct	mod_t	*mod;	/* pass 1; defining module */
	unsigned int	symno;	/* pass 2; symbol number */
} sym_t;			/* above 2 items could be united */

typedef	struct	mod_t	{	/* descriptor for each input module */
	struct	mod_t	*next;	/* chained together */
	char	*fname;		/* file containing module */
	char	mname[DIRSIZ];	/* module name if in archive */
	seg_t	seg[NLSEG];	/* descriptor for each segment */
	unsigned int	nsym;	/* #symbols for this module */
	sym_t	*sym[];		/* array of nsym symbol ptrs */
} mod_t;

typedef	int	flag_t;		/* command line flag option */

typedef	struct	ar_hdr	arh_t;
typedef	struct	ar_sym	ars_t;
typedef	struct	ldheader ldh_t;
typedef	struct	ldsym	lds_t;

/* structures built in pass 1, used in pass 2 */

#define	NHASH	64
sym_t	*symtable[NHASH];	/* hashed symbol table */
mod_t	*modhead, *modtail;	/* module list head and tail */
seg_t	oseg[NLSEG];		/* output segment descriptors */
int	nundef;			/* number of undefined symbols */
uaddr_t	commons;		/* accumulate size of commons */
int	machine;			/* Machine type */
sym_t	*etext_s, *edata_s, *end_s;	/* special loader generated symbols */
char	etext_id[NCPLN] = "etext_";	/* and their names */
char	edata_id[NCPLN] = "edata_";
char	end_id[NCPLN] = "end_";
ldh_t	oldh			/* output load file header */
		 = { L_MAGIC, LF_32, 0, sizeof(oldh) };
char	*mchname[] = {		/* names of known machines */
		"Turing Machine",
		"PDP-11",
		"VAX-11",
		"3sickly",
		"Z8001",
		"Z8002",
		"iAPX 86",
		"i8080",
		"6800",
		"6809",
		"68000",
		"NS16000",
};
uaddr_t	userbase[] = {		/* Coherent user process reloc. to here */
		0L,
		0L,
		0L,
		0L,
		0x00030000L,		/* Paddr format */
		0L,
		0L,
		0L,
		0L,
		0L,
		0L,
		0L,
};
uaddr_t	segsize[] = {		/* size of segment on target machine */
		0,
		8192,
		512,
		2048,
		0x00010000L,
		512,
		16,
		0,
		0,
		0,
		0,
		0,
};
uaddr_t	segmax[] = {		/* Large model takes over above this */
		0,
		0,
		0,
		0,
		0x00010000L,
		0,
		0x00010000L,
		0,
		0,
		0,
		0xFFFFFFFFL,
		0xFFFFFFFFL,
};
uaddr_t	drvbase[] = {		/* base of loadable driver */
		0L,
		0120000L,
		0L,
		0L,
		0x003E0000L,		/* Overlay segment */
		0xD000L,
		0xD000L,
		0L,
		0L,
		0L,
		0L,
		0L,
};
uaddr_t	drvtop[] = {		/* address limit of loadable driver */
		0L,
		0140000L,
		0L,
		0L,
		0x003F0000L,
		0xF000L,
		0xF000L,
		0L,
		0L,
		0L,
		0L,
		0L,
};
flag_t	noilcl,			/* discard internal symbols `L...' */
	nolcl,			/* discard local symbols */
	watch,			/* watch everything happen */
	worder,			/* byte order in word; depends on machine */
	lorder;			/* Word order in long */
char	*outbuf;		/* buffer for in-memory load */
FILE	*outputf[NLSEG];	/* output ptrs (for each segment) */

/* seconds between ranlib update and archive modify times */
#define	SLOPTIME 150

/* values for worder, lorder */
#define	LOHI	0
#define	HILO	1

/* for pass 2; these will change if format of relocation changed */
#define	putaddr(addr,fp,sgp)	(putlohi((short)((addr)>>16),fp,sgp),\
				 putlohi((short)(addr),fp,sgp))
#define	getsymno getlohi
#define	putsymno putlohi

/* C requires this... */
void	baseall();
void	canldh();
void	endbind();
void	undef();
void	lfixup1(), lfixup2();
uaddr_t	setbase(), newpage(), lentry(), seppage();
uaddr_t ptov(), vtop();
size_t	segoffs();
void	symredef(), rdsymbol();
sym_t	*addsym(), *symref();
size_t	symoff();
void	loadmod(), putstruc(), putword(), putlohi(), puthilo(), putbyte();
void	putlong();
unsigned short	getword(), getlohi(), gethilo();
unsigned long getlong(), getaddr();
void	message(), fatal(), usage(), filemsg(), modmsg(), mpmsg(), spmsg();
