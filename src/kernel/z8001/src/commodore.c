/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Machine dependent routines for the Commodore
 * M-series Z8001 processor running in
 * Segmented mode.
 */
#include <coherent.h>
#include <clist.h>
#include <errno.h>
#include <inode.h>
#include <proc.h>
#include <seg.h>
#include <signal.h>
#include <uproc.h>
#include <romconf.h>
#include <alloc.h>

struct	romconf	romconf;
char	*_bvirt;

/*
 * General initialisation.
 * Setup pointers to most data structures.
 */
commodore()
{
	register vaddr_t p, p1;
	extern char end, etext;
	register unsigned e, et;
	register char *t;

	/*
	 * Segment 1 is ROM DS.
	 * It contains the romconf structure
	 * at offset of 0.
	 */
	t = (char *)ADDR(1, 0);
	t = *(int **)t;
	kkcopy(t, &romconf, sizeof romconf);
	corebot = romconf.rom_bram;
	coretop = romconf.rom_eram;
	/*
	 * Compute base of system (after Kernel Alloc Space).
	 * This base is in physical memory.
	 */
	t = &end;
	e = (unsigned)t;
	t = &etext;
	et = (unsigned)t;
	p1 = ctob((long)corebot);
	p1 += bruc(et);
	p1 += e;
	p = bruc(p1+ALLSIZE);
	allkp = setarena((char *)&end, asize = p-p1);
	inodep = (INODE *)(&end + asize);
	p += NINODE*sizeof(INODE);
	if (e + asize + NINODE*sizeof(INODE) < e)
		panic("System is too large");
	p = (p + (long)(BSIZE-1)) & ~((long)(BSIZE-1));/* DMA needs aligned */
	blockp = p;
	_bvirt = pfix(BFS, p);
	p += NBUF*BSIZE;
	clistp = (paddr_t)pfix(CLS, p);
	p += NCLIST*sizeof(CLIST);
	corebot = btocru(p);
	msize = ctokrd(coretop-corebot);
	/*
	 * Map the bit map
	 */
	pfix(BMS, BMPHYS);
	pfix(BMS+1, BMPHYS+0x00010000L);
}

/*
 * Calculate segmentation for a new process.
 * Segmented system has a separate (and only one)
 * stack segment.
 * Right now only separated (possibly) I/D (i.e.
 * 2 segments.
 */
mproto()
{
	register PROC *pp;
	register int sn;
	register int i;

	u.u_sproto.mp_nseg = 0;
	pp = SELF;
	if ((pp->p_flags & PFKERN) != 0) {
		if (pp->p_segp[SIPTEXT] != NULL
		 || pp->p_segp[SISTEXT] != NULL
		 || pp->p_segp[SISDATA] != NULL
		 || pp->p_segp[SIPDATA] != NULL
		 && pp->p_segp[SIPDATA]->s_size > MSSIZE) {
			u.u_error = ENOEXEC;
			return (0);
		}
		return (1);
	}
	sn = USTACK;
	for (i=SISTACK; i<=SIPDATA; i++) {
		if ((sn = uproto(pp->p_segp[i], sn, i)) < 0)
			return (0);
		if (i == SISTACK)
			sn = USTACK+1;
		else if (i == SIPSLIB)
			sn = USEG;
	}
	return (1);
}

/*
 * Load up the segmentation hardware.
 * Recalculate the prototype if necessary (after fork
 * and swaps)
 */
segload()
{
	if ((SELF->p_flags & (PFSPROTO|PFKERN)) != 0) {
		if ((SELF->p_flags & PFKERN) != 0)
			return;
		sproto();
	}
	loadmmu(u.u_sproto.mp_nseg, &u.u_sproto.mp_hsegs[0]);
}

/*
 * Set up segmentation for one segment.
 * Returns the next available segment (or negative on error).
 * `sp' is the segment pointer, `sn' is the hardware
 * segment number at which to start and `si' is
 * the index into Coherent's software segment table.
 * The structure of this routine leaves something
 * to be desired in clarity.
 */
static
uproto(sp, sn, si)
register SEG *sp;
register int sn;
int si;
{
	register struct hsegs *hp;
	register int f, l, hl;
	register saddr_t b;
	unsigned base;

	/*
	 * Check here for unused segments.
	 */
	if (sp == NULL)
		return (sn);
	hp = &u.u_sproto.mp_hsegs[sn];
	f = sp->s_flags&SFSHRX ? 0x01 : 0;	/* read only? */
	base = 0;
	b = sp->s_mbase;
	l = sp->s_size;
	if ((sp->s_flags & SFDOWN) != 0) {
		if (l >= MSSIZE-1)
			return (-1);
		f |= 0x20;		/* Downward growing segment */
		hl = MSSIZE-l;
		b -= hl;
		base = ctob(hl);
	}
	b <<= CSH;
	u.u_segl[si].sr_base = ADDR(sn, base);
	while (l > 0) {
		if (sn >= NHUSEG)
			return (-1);
		if (l >= MSSIZE) {
/*			printf("Large sproto, size=%u, sn=%d\n", l, sn); */
			hl = 0xFF;		/* Full segment */
		} else if ((sp->s_flags & SFDOWN) != 0)
			hl = (MSSIZE-l)<<CSH;
		else
			hl = (l<<CSH)-1;
		hp->mp_base = b;
		b += 256;
		hp->mp_len = hl;
		hp->mp_attr = f;
		sn++;
		hp++;
		l -= MSSIZE;
	}
	if (sn > u.u_sproto.mp_nseg)
		u.u_sproto.mp_nseg = sn;
	return (sn);
}

/*
 * Copy `n' clicks from the segment base `s1' to `s2' from right to left.
 */
srlcopy(s1, s2, n)
register saddr_t s1;
register saddr_t s2;
register unsigned n;
{
	if (n == 0)
		return;
	s1 += n;
	s2 += n;
	do {
		slrcopy(--s1, --s2, 1);
	} while (--n);
}

/*
 * Set up initial context for a process running in kernel mode.
 */
msetsys(mp, f, m)
register MCON *mp;
int (*f)();
saddr_t m;
{
	mp->mc_omap = m<<CSH;
	mp->mc_sp = (char *)&u + UPASIZE-sizeof(f);
	mp->mc_pc = f;
	mp->mc_fcw = MFSYS|MFVIE|MFSEG;
	mp->mc_depth = 1;
}

/*
 * Set up a new user process.
 * If this is a shared library, assume msetusr
 * is the last call in exec so that we can
 * suspend the process.
 */
msetusr(pc, sp)
vaddr_t pc;
vaddr_t sp;
{
	register PROC *pp;

	pp = SELF;
	if ((pp->p_flags & PFSLIB) != 0) {
		upause();
		/* NOTREACHED */
	}
	*((long *)(regl+OPC)) = pc;
	if ((SELF->p_flags & PFKERN) != 0) {
		regl[OFCW] |= MFSYS;
		regl[OOS] = SELF->p_segp[SIPDATA]->s_mbase << CSH;
	} else
		*((long *)(regl+OR14)) = sp;
}

/*
 * Set the given address in the user area to the given value if it is
 * okay to do so.
 */
msetuof(a, v)
register int a;
register unsigned v;
{

	a = -(UPASIZE - a)/2;
	switch (a) {
	case OR0:
	case OR1:
	case OR2:
	case OR3:
	case OR4:
	case OR5:
	case OR6:
	case OR7:
	case OR8:
	case OR9:
	case OR10:
	case OR11:
	case OR12:
	case OR13:
	case OR14:
	case OR15:
	case OPCSEG:
	case OPCOFF:
		regl[a] = v;
		break;

	case OFCW:
		if ((v&~MFCCB) != 0)
			return (0);
		regl[OFCW] &= ~MFCCB;
		regl[OFCW] |= v;
		break;

	default:
		u.u_error = EINVAL;
		return (0);
	}
	return (1);
}

/*
 * Cause a signal routine to be executed.
 */
msigint(n, f)
register int n;
register int (*f)();
{
	register int *usp;

	usp = *(int **)&regl[OR14];
	putupd(--(int **)usp, *(int **)&regl[OPC]);
	putuwd(--usp, regl[OFCW]);
	putuwd(--usp, n);
	regl[OFCW] &= ~MFNVE;
	*(int **)&regl[OPC] = f;
	*(int **)&regl[OR14] = usp;
	if (n!=SIGEPA && n!=SIGTRAP)
		u.u_sfunc[n-1] = SIG_DFL;
}

/*
 * Cause the next instruction to single step.
 * Non-vectored interrupt enable signals assembler
 * code to arm the single-step logic on return
 * from this trap.
 */
msigsin()
{
	regl[OFCW] |= MFNVE;
}

/*
 * Idle process.
 */
idle()
{
	for (;;) {
		disflag = 1;
		_idle();
	}
}

/*
 * Set an interrupt vector.
 * Make an entry in the "vecs" table, for
 * use by the interrupt dispatcher.  No interrupt
 * controller needs to be armed on this machine.
 * Levels must be even on the z8001.
 */
setivec(level, fun)
register int	level;
int		(*fun)();
{
	register vaddr_t t1, t2;
	extern	 int	(*vecs[])();
	extern	saddr_t	vmaps[];
	extern	 int	vret();

	level >>= 1;
	t1 = (vaddr_t)vecs[level];
	t2 = (vaddr_t)vret;
	if ((unsigned)t1 != (unsigned)t2) {
		u.u_error = EDBUSY;
		return;
	}
	vecs[level] = fun;
	vmaps[level] = omapget();
}

/*
 * Clear an interrupt vector.
 */
clrivec(level)
register int	level;
{
	extern	 int	(*vecs[])();
	extern	 int	vret();

	level >>= 1;
	vecs[level] = vret;
}
