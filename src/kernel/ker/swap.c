/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Swapper.
 */
#include <coherent.h>
#include <proc.h>
#include <sched.h>
#include <seg.h>
#include <uproc.h>
#include <buf.h>

/*
 * Functions.
 */
SEG	*xmalloc();
SEG	*xdalloc();

swap()
{
	register SEG *sp;
	register PROC *pp1;
	register PROC *pp2;
	register PROC *pp3;
	register unsigned s;
	register unsigned n;
	register unsigned t;
	register unsigned v;
	register unsigned m;
	register int i;
	static unsigned ltimer;

#ifndef	NOMONITOR
	if (swmflag)
		printf("Swap starts %d\n", sexflag);
#endif
	if (sexflag != 0)
		uexit(1);
	sexflag++;
	while (SELF->p_ssig == 0) {
		lock(pnxgate);
		t = (utimer-ltimer)/NSUTICK;
		v = t*SVCLOCK;
		ltimer += t*NSUTICK;
		m = 0;
		pp2 = NULL;
		for (pp1=procq.p_nback; pp1!=&procq; pp1=pp1->p_nback) {
			if ((pp1->p_flags&PFCORE) != 0) {
				pp1->p_sval >>= t;
				pp1->p_ival -= t;
				if (pp1->p_ival < -30000)
					pp1->p_ival = -30000;
				continue;
			}
			addu(pp1->p_sval, v);
			if (pp1->p_state != PSRUN)
				continue;
			s = 0;
			for (i=0; i<NUSEG+1; i++)
				if ((sp=pp1->p_segp[i]) != NULL)
					if ((sp->s_flags&SFCORE) == 0)
						s += sp->s_size;
			if ((s=ctokrd(s)) == 0)
				s = 1;
			n = (pp1->p_sval+pp1->p_rval)/s;
			if (n > m) {
				m = n;
				pp2 = pp1;
			}
		}
		unlock(pnxgate);
		if (pp2 == NULL)
			goto con;
#ifndef	NOMONITOR
		if (swmflag)
			printf("Swapin(%p, %d)\n", pp2, pp2->p_pid);
#endif
	xxx:
		while (testcore(pp2)==0 || proccore(pp2)!=0) {
			if ((pp2->p_flags&PFAUXM) != 0) {
				auxmdisk(pp2);
				goto xxx;
			}
			procdisk(pp2);
			i = 32767;
			pp3 = NULL;
			lock(pnxgate);
			for (pp1=procq.p_nforw; pp1!=&procq; pp1=pp1->p_nforw){
				if (pp1->p_flags&(PFSWIO|PFLOCK|PFKERN))
					continue;
				if ((pp1->p_flags&PFAUXM) != 0) {
					auxmdisk(pp1);
					unlock(pnxgate);
					goto xxx;
				}
				if ((pp1->p_flags&PFCORE) == 0) {
					if (procdisk(pp1) != 0) {
						unlock(pnxgate);
						goto xxx;
					}
					continue;
				}
				if (pp1->p_ival>-64 && pp1->p_sval!=0)
					continue;
				if (pp1->p_ival < i) {
					i = pp1->p_ival;
					pp3 = pp1;
				}
			}
			unlock(pnxgate);
			if (pp3 == NULL) {
#ifndef NOMONITOR
				if (swmflag)
					printf("No one to swap out\n");
#endif
				break;
			}
			if (i > 0) {
#ifndef NOMONITOR
				if (swmflag)
					printf("Dispatch(%p, %d)\n",
						pp3, pp3->p_pid);
#endif
				pp3->p_flags |= PFDISP;
				break;
			}
#ifndef NOMONITOR
			if (swmflag)
				printf("Swapout(%p, %d)\n", pp3, pp3->p_pid);
#endif
			procdisk(pp3);
		}
#ifndef NOMONITOR
		if (swmflag)
			printf("Swapdone\n");
#endif
	con:
		timeout(&stimer, NSRTICK, wakeup, (char *)&stimer);
		sleep((char *)&stimer, CVSWAP, IVSWAP, SVSWAP);
	}
	--sexflag;
	uexit(1);
}

/*
 * See if the given process may fit in core.
 */
testcore(pp)
register PROC *pp;
{
	register SEG *sp;
	register saddr_t s;
	register saddr_t s1;
	register saddr_t s2;
	register int i;

	s = 0;
	for (i=0; i<NUSEG+1; i++) {
		if ((sp=pp->p_segp[i]) == NULL)
			continue;
		if ((sp->s_flags&SFCORE) != 0)
			continue;
		if (sp->s_size > s)
			s = sp->s_size;
	}
	s1 = corebot;
	sp = &segmq;
	do {
		sp = sp->s_forw;
		s2 = sp->s_mbase;
		if (s2-s1 >= s)
			return (1);
		s1 = sp->s_mbase + sp->s_size;
	} while (sp != &segmq);
	return (0);
}

/*
 * Swap all segments associated with a particular process into core.
 * The number of segments still swapped out is returned.
 */
proccore(pp)
register PROC *pp;
{
	register SEG *sp;
	register int i;
	register int n;
	register int f;

	n = 0;
	f = pp->p_flags&PFSWAP;
	for (i=0; i<NUSEG+1; i++) {
		if ((sp=pp->p_segp[i]) == NULL)
			continue;
		if (f != 0)
			sp->s_lrefc++;
		if ((sp->s_flags&SFCORE)==0 && segcore(sp)==0)
			n++;
	}
	if (n == 0)
		pp->p_flags |= PFCORE|PFSPROTO;
	pp->p_flags &= ~PFSWAP;
	return (n);
}

/*
 * Swap out all segments associated with a given process.
 */
procdisk(pp)
register PROC *pp;
{
	register SEG *sp;
	register int i;
	register int f;
	int n;

	n = 0;
	f = pp->p_flags&PFSWAP;
	pp->p_flags &= ~PFCORE;
	for (i=0; i<NUSEG+1; i++) {
		if ((sp=pp->p_segp[i]) == NULL)
			continue;
		if (f == 0)
			--sp->s_lrefc;
		if ((sp->s_flags&SFCORE) == 0)
			continue;
		if (sp->s_lrefc == 0)
			if (segdisk(sp) != 0)
				n++;
	}
	pp->p_flags |= PFSWAP;
	return (n);
}

/*
 * Swap out all auxiliary segments used by a process.
 */
auxmdisk(pp)
register PROC *pp;
{
	register SEG *sp;
	register int i;
	register int f;
	register int m;
	SEG *segl[NUSEG];

#ifndef NOMONITOR
	if (swmflag)
		printf("Auxiliary(%p, %d)\n", pp, pp->p_pid);
#endif
	sp = pp->p_segp[SIUSERP];
	if ((sp->s_flags&SFCORE) == 0) {
		panic("We may be in trouble");
		return;
	}
	m = pp->p_flags&PFCORE;
	f = pp->p_flags&PFAUXM;
	pp->p_flags &= ~(PFAUXM|PFCORE);
	skcopy(sp, offset(uproc, u_sege[0]), segl, sizeof(u.u_sege));
	for (i=0; i<NUSEG; i++) {
		if ((sp=segl[i]) == NULL)
			continue;
		if (f != 0)
			--sp->s_lrefc;
		if ((sp->s_flags&SFCORE) == 0)
			continue;
		if (sp->s_lrefc == 0)
			segdisk(sp);
	}
	pp->p_flags |= m;
}

/*
 * Swap the given segment into core.
 */
segcore(sp1)
register SEG *sp1;
{
	register SEG *sp2;

	lock(seglink);
	sp2 = xmalloc(sp1->s_size);
	unlock(seglink);
	if (sp2 == NULL)
		return (0);
	sp1->s_lrefc++;
	swapio(0, sp2->s_mbase, sp1->s_dbase, sp2->s_size);
	lock(seglink);
	sp1->s_back->s_forw = sp1->s_forw;
	sp1->s_forw->s_back = sp1->s_back;
	sp2->s_back->s_forw = sp1;
	sp1->s_back = sp2->s_back;
	sp2->s_forw->s_back = sp1;
	sp1->s_forw = sp2->s_forw;
	sp1->s_flags |= SFCORE;
	sp1->s_mbase = sp2->s_mbase;
	--sp1->s_lrefc;
	unlock(seglink);
	return (1);
}

/*
 * Swap the given segment out onto disk.
 */
segdisk(sp1)
register SEG *sp1;
{
	register SEG *sp2;

	lock(seglink);
	sp2 = xdalloc(sp1->s_size);
	unlock(seglink);
	if (sp2 == NULL)
		return (0);
	sp1->s_lrefc++;
	swapio(1, sp1->s_mbase, sp2->s_dbase, sp1->s_size);
	lock(seglink);
	sp1->s_back->s_forw = sp1->s_forw;
	sp1->s_forw->s_back = sp1->s_back;
	sp2->s_back->s_forw = sp1;
	sp1->s_back = sp2->s_back;
	sp2->s_forw->s_back = sp1;
	sp1->s_forw = sp2->s_forw;
	sp1->s_flags &= ~SFCORE;
	sp1->s_dbase = sp2->s_dbase;
	--sp1->s_lrefc;
	unlock(seglink);
	return (1);
}

/*
 * Allocate a segment on disk that is `n' clicks long.
 * The `seglink' gate should be locked before this routine is called.
 * This routine is the same as `sdalloc' except that we can't run out of
 * alloc space to allocate the segment and we allocate in high regions.
 */
SEG *
xdalloc(s)
saddr_t s;
{
	register SEG *sp1;
	register SEG *sp2;
	register daddr_t d;
	register daddr_t d1;
	register daddr_t d2;

	d = stod(s);
	d2 = swaptop;
	sp1 = &segdq;
	do {
		if ((sp1=sp1->s_back) != &segdq)
			d1 = sp1->s_dbase + stod(sp1->s_size);
		else
			d1 = swapbot;
		if (d2-d1 >= d) {
			sp2 = &segswap;
			kclear((char *)sp2, sizeof(SEG));
			sp1->s_forw->s_back = sp2;
			sp2->s_forw = sp1->s_forw;
			sp1->s_forw = sp2;
			sp2->s_back = sp1;
			sp2->s_urefc = 1;
			sp2->s_lrefc = 1;
			sp2->s_size = s;
			sp2->s_dbase = d2 - d;
			return (sp2);
		}
		d2 = sp1->s_dbase;
	} while (sp1 != &segdq);
	return (NULL);
}

/*
 * Allocate a segment in memory that is `n' clicks long.
 * The `seglink' gate should be locked before this routine is called.
 * This routine is the same as `smalloc' except that we can't run out of
 * alloc space to allocate the segment.
 */
SEG *
xmalloc(s)
register saddr_t s;
{
	register SEG *sp1;
	register SEG *sp2;
	register saddr_t s1;
	register saddr_t s2;

	s1 = corebot;
	sp1 = &segmq;
	do {
		if ((sp1=sp1->s_forw) != &segmq)
			s2 = sp1->s_mbase;
		else
			s2 = coretop;
		if (s2-s1 >= s) {
			sp2 = &segswap;
			kclear((char *)sp2, sizeof(SEG));
			sp1->s_back->s_forw = sp2;
			sp2->s_back = sp1->s_back;
			sp1->s_back = sp2;
			sp2->s_forw = sp1;
			sp2->s_urefc = 1;
			sp2->s_lrefc = 1;
			sp2->s_size = s;
			sp2->s_mbase = s1;
			return (sp2);
		}
		s1 = sp1->s_mbase + sp1->s_size;
	} while (sp1 != &segmq);
	return (NULL);
}
