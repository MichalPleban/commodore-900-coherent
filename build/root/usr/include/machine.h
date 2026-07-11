/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
/*
 * Machine-dependent header file for the Commodore
 * M-series and Z8001HR processors.
 * This system runs in segmented mode.
 */
#ifndef	 MACHINE_H
#define	 MACHINE_H
#ifndef	NULL			/* Move to coherent.h ? */
#define	NULL	((char *)0)
#endif

/* Machine dependent constants for Z8001 */
#define	VERSION		"0.7.3"		/* NLD, Shared libs, WD disk */
#define	MSASIZE		16		/* Space reserved for sys args */
#define	ISTSIZE		256		/* Counts on stack growth */
#define	ISTVIRT		ADDR(USTACK+1, 0)
#define	SOVSIZE		64		/* Only needed for old compatibility */
#define	UPASIZE		2048		/* Size of user area */
#define	MADSIZE		32768L		/* Maximum stack seg size */
#define	SCHUNK		16384		/* Amount copied in core dump write */
#define SMICALL		0		/* Start of independent system calls */
#define NMICALL		66		/* Machine independent system calls */
#define SMDCALL		128		/* Start of dependent system calls */
#define NMDCALL		3		/* Machine dependent system calls */
#define	BSIZE		512		/* Buffer size */
#define	HZ		100		/* Number of clock ticks per second */
#define	NCPCL		(256-sizeof(cmap_t))/* Number of characters in clist */
#define	NPID		30000		/* Maximum process id */
#define	MAXU		((unsigned)0xFFFF)
#define	ADDR(s,o)	(((long)(s)<<24)+(unsigned)(o))
#define	MUERR		ADDR(USTACK, 0xFFFE)	/* Location of `errno' */
#define	MSSIZE		64			/* Maximum seg. size clicks */
#define	CSH		2			/* Shift saddr_t to hardware */

/*
 * Segments for use in selecting various spaces.
 */
#define	IS	0x30		/* System code segment */
#define	DS	0x31		/* System data segment */
#define	CLS	0x32		/* Clist mapping segment */
#define	BFS	0x33		/* Buffer mapping segment */
#define	WDS	0x39		/* Western Digital hard disc segment */
#define	BMS	0x3A		/* Bitmap segment 0, segment 1 is 0x3B */
#define	DBS	0x3C		/* DDT segment - no one else touch */
#define	ES	0x3D		/* Extra segment */
#define	OS	0x3E		/* System overlay segment */
#define	SS	0x3F		/* System stack/U-area segment */
#define	USTACK	0x00		/* User stack */
#define	USEG	0x03		/* Start of user segments */
#define	BMPHYS	0x00FF0000L	/* Physical (paddr_t) of bitmap */

/*
 * Alloc definitions for the Z8000.
 */
#ifdef KERNEL
#define	align(p)	((ALL *) ((vaddr_t) (p) & ~1))
#define	link(p)		align((p)->a_link)
#define	tstfree(p)	(((int) (p)->a_link&1) == 0)
#define	setfree(p)	((p)->a_link = (vaddr_t) (p)->a_link & ~1)
#define	setused(p)	((p)->a_link = (vaddr_t) (p)->a_link | 1)
#endif

/*
 * Offsets for the registers, defined from the end of the
 * system stack in the `u' area.  `regl' provides the base
 * for this, offsets below are in words.
 */
#define	regl	((unsigned *)((char *)&u+UPASIZE))

#define	OPCOFF	(-1)			/* PC offset */
#define	OPCSEG	(-2)			/* PC segment # */
#define OFCW	(-3)			/* Flags and control word */
#define	OPC	OPCSEG			/* Programme counter (long) */
#define	OR0	(-21)			/* Register 0 */
#define	OR1	(-20)			/* Register 1 */
#define	OR2	(-19)			/* Register 2 */
#define	OR3	(-18)			/* Register 3 */
#define	OR4	(-17)			/* Register 4 */
#define	OR5	(-16)			/* Register 5 */
#define	OR6	(-15)			/* Register 6 */
#define	OR7	(-14)			/* Register 7 */
#define	OR8	(-13)			/* Register 8 */
#define	OR9	(-12)			/* Register 9 */
#define	OR10	(-11)			/* Register 10 */
#define	OR11	(-10)			/* Register 11 */
#define	OR12	(-9)			/* Register 12 */
#define	OR13	(-8)			/* Register 13 */
#define	OS14	(-7)			/* System mode register 14 */
#define	OS15	(-6)			/* System mode register 15 */
#define	OR14	(-23)			/* Register 14 */
#define	OR15	(-22)			/* Register 15 */
#define	OES	(-24)			/* ES map */
#define	OOS	(-25)			/* OS map */

/*
 * For accessing high and low words of a long.
 */
struct l {
	int	l_hi;
	int	l_lo;
};

/*
 * Machine dependent constants needed later on.
 * Register offsets from a fixed base pointer onto
 * the stack.
 */
#define	SFCW	(MFSYS|MFSEG)
#define	MFSEG	0x8000			/* Segmented mode */
#define MFSYS	0x4000			/* System mode */
#define MFVIE	0x1000			/* Enable vectored interrupts */
#define	MFNVE	0x0800			/* Non-vectored interrup enable */
#define	MFCCB	0x00FC			/* Condition code bits */

/*
 * Functions.
 */
#define	blockn(n)		((n)>>9)
#define	blocko(n)		((n)&(512-1))
#define nbnrem(b)		((int)(b)&(128-1))
#define nbndiv(b)		((b)>>7)
#define	btocru(n)		((saddr_t) (((n)+1023)>>10))
#define	btocrd(n)		((saddr_t) ((n)>>10))
#define	bruc(n)			((n+1023)&~1023)
#define	ctob(n)			((n)<<10)
#define	ctokrd(n)		(n)
#define	stod(n)			((daddr_t)((n)<<1))

/*
 * Simple functions.
 */
#define	sualloc(s)	smalloc(s)
#define	msetppc(v)	(*((vaddr_t*)(regl+OPC))=(v))
#define	sxalloc(s,f)	((f&SFHIGH) ? shalloc(s) : smalloc(s))
#define	msysgen(p)
#define	mfixcon(pp)
#define	usave()
#define	kdaddr(s)	((char *)(ADDR(DS, (unsigned)(s))))
#define	kucopy(k,u,n)	kkcopy((k), ufix(u), n)
#define	pkcopy(p,k,n)	kkcopy(pfix(ES, p), k, n)
#define	kpcopy(k,p,n)	kkcopy(k, pfix(ES, p), n)
#define	upcopy(u,p,n)	kkcopy(ufix(u), pfix(ES, p), n)
#define	pucopy(p,u,n)	kkcopy(pfix(ES, p), ufix(u), n)
#define	getuwi		getuwd
#define	putuwi		putuwd
#define	getuwd(a)	pgetw(ufix(a))
#define	getupd(a)	pgetp(ufix(a))
#define	getubd(a)	pgetb(ufix(a))
#define	putuwd(a,i)	pputw(ufix(a),i)
#define	putupd(a,p)	pputp(ufix(a),p)
#define	putubd(a,c)	pputb(ufix(a),c)

/*
 * For mapping auxiliary segment in exec.
 */
#define	asave(m)	((m)=omapget())
#define	arest(m)	omapset(m)
#define	abase(m)	(omapset((m)<<CSH), (char *)ADDR(OS,0))
#define	aputp(p,q)	(*(char **)(p) = (q))
#define	aputi(p,i)	(*(int *)(p) = (i))
#define	aputc(p,c)	(*(char *)(p) = (c))

/*
 * Buffers are mapped only in segmented system.
 */
#define	bsave(m)
#define	brest(m)
#define	bmapv(m)
#define	bconv(p)	(p)
#define	bvirt(p)	((BUF *)(_bvirt+(unsigned)((p)-blockp)))

/*
 * Clists are permanently mapped into their own segment.
 * Therefore 64K is the maximum number of clists allowed.
 */
#define	csave(m)
#define	crest(m)
#define	cmapv(m)
#define	cconv(p)	(p)
#define	cvirt(p)	((CLIST *)(p))

/*
 * Loadable driver support.
 */
#define	dsave(m)	((m)=omapget())
#define	drest(m)	omapset(m)
#define	dcopy(t,f)	((t)=(f))
#define	dmapv(m)	omapset((m)<<CSH)
#define	dvirt()	((char *)ADDR(OS,0))

/*
 * Set processor priority.
 * `sphi' is a real routine to make things
 * faster and smaller.
 */
#define splo()	spl(SFCW|MFVIE)

/*
 * Setjmp/longjmp version of the environment.
 */
typedef	struct menv {
	int	me_r6;
	int	me_r7;
	int	me_r8;
	int	me_r9;
	int	me_r10;
	int	me_r11;
	int	me_r12;
	int	me_r13;
	int	me_r14;
	int	me_sp;
	char	*me_pc;
	int	me_fcw;
	int	me_depth;		/* Stack depth */
	unsigned me_emap;		/* Extra map (hardware clicks) */
	unsigned me_omap;		/* Loadable map */
}	MENV;

/*
 * Context structure.
 */
typedef struct mcon {
	int	mc_r6;
	int	mc_r7;
	int	mc_r8;
	int	mc_r9;
	int	mc_r10;
	int	mc_r11;
	int	mc_r12;
	int	mc_r13;
	char	*mc_sp;
	char	*mc_pc;
	int	mc_fcw;
	int	mc_depth;		/* Stack depth (system/normal mode) */
	unsigned mc_emap;		/* Extra map (not saddr_t) */
	unsigned mc_omap;		/* Overlay map */
} MCON;

/*
 * Other things to save in the configuration.
 * Usually this is for floating point registers,
 * and other things like that.  Here, we have
 * nothing to save.
 */
typedef	struct	mgen {
}	MGEN;

/*
 * Segmenation prototype.
 */
#define	NHUSEG	32				/* Segments for user */

typedef	struct	mproto {
	int	mp_nseg;			/* Number of active segments */
	struct	hsegs	{
		unsigned	mp_base;	/* Base of segment */
		unsigned char	mp_len;		/* Length of segment */
		unsigned char	mp_attr;	/* Segment attributes */
	}	mp_hsegs[NHUSEG];
}	MPROTO;


#ifdef KERNEL
/*
 * Global variables.
 */
int	ukcopy();
char	*pgetp();
char	*pfix();
char	*ufix();
paddr_t	vtop();
vaddr_t	ptov();
char	*_bvirt;
#endif

#endif
