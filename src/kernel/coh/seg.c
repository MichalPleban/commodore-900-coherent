/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Segment manipulation.
 */
#include <coherent.h>
#include <buf.h>
#include <errno.h>
#include <ino.h>
#include <inode.h>
#include <proc.h>
#include <sched.h>
#include <seg.h>
#include <uproc.h>

/*
 * Initialisation code.
 */
seginit()
{
	segmq.s_forw = &segmq;
	segmq.s_back = &segmq;
	segdq.s_forw = &segdq;
	segdq.s_back = &segdq;
}

/*
 * Given an inode, `ip', and flags, `ff', describing a segment associated
 * with the inode, see if the segment already exists and if so, return a
 * copy.  If the segment does not exists, allocate the segment having size
 * `ss', and read the segment using the inode at seek offset `dq' with a
 * size of `ds'.
 */
SEG *
ssalloc(rp, ip, ff, ss, dq, ds)
int *rp;
register INODE *ip;
size_t ss;
size_t dq;
size_t ds;
{
	register SEG *sp;
	register int f;

	*rp = -1;
	if (ss == 0) {
		*rp = 1;
		return (NULL);
	}
	lock(seglink);
	f = ff & (SFSHRX|SFTEXT);

	/*
	 * Look for the segment in the memory queue.
	 */
	for (sp=segmq.s_forw; sp!=&segmq; sp=sp->s_forw) {
		if (sp->s_ip==ip && (sp->s_flags&(SFSHRX|SFTEXT))==f) {
			unlock(seglink);
			if ((sp = segdupl(sp)) == NULL)
				return (NULL);
			segfinm(sp);
			*rp = 1;
			return (sp);
		}
	}

	/*
	 * Look for the segment on the disk queue.
	 */
	for (sp=segdq.s_forw; sp!=&segdq; sp=sp->s_forw) {
		if (sp->s_ip==ip && (sp->s_flags&(SFSHRX|SFTEXT))==f) {
			unlock(seglink);
			if ((sp = segdupl(sp)) == NULL)
				return (NULL);
			segfinm(sp);
			*rp = 1;
			return (sp);
		}
	}
	unlock(seglink);

	/*
	 * Allocate and create the segment.
	 */
	if ((sp=salloc(ss, ff)) == NULL)
		return (NULL);
	if (exsread(sp, ip, ds, dq, (size_t)0) == 0) {
		sfree(sp);
		return (NULL);
	}
	if ((ff&SFSHRX) != 0) {
		sp->s_ip = ip;
		ip->i_refc++;
	}
	*rp = 0;
	return (sp);
}

/*
 * Given a pointer to a newly created process, copy all of our segments
 * into the given process.
 */
segadup(cpp)
register PROC *cpp;
{
	register SEG *sp;
	register int n;
	register PROC *pp;

	pp = SELF;
	cpp->p_flags |= PFSWIO;
	for (n=0; n<NUSEG; n++) {
		if ((sp=pp->p_segp[n]) == NULL)
			continue;
		if ((sp=segdupl(sp)) == NULL)
			break;
		cpp->p_segp[n] = sp;
		if ((sp->s_flags&SFCORE) == 0)
			cpp->p_flags &= ~PFCORE;
	}
	if (n < NUSEG) {
		while (n > 0) {
			if ((sp=cpp->p_segp[--n]) != NULL) {
				sfree(sp);
				cpp->p_segp[n] = NULL;
			}
		}
	}
	cpp->p_flags &= ~PFSWIO;
	return (n);
}

/*
 * Duplicate a segment.
 */
SEG *
segdupl(sp)
register SEG *sp;
{
	register SEG *sp1;
	register size_t ss;

	if ((sp->s_flags&SFSHRX) != 0) {
		/*
		 * `s_urefc' is a char; refuse rather than wrap at the
		 * 128th sharer (the caller fails the exec or fork).
		 */
		if (sp->s_urefc >= 127)
			return (NULL);
		sp->s_urefc++;
		sp->s_lrefc++;
		return (sp);
	}
	ss = ctob((size_t)sp->s_size);
	if ((sp->s_flags&SFCORE) == 0)
		panic("Cannot duplicate non shared swapped segment");
	if ((sp1=salloc(ss, sp->s_flags|SFNSWP|SFNCLR)) == NULL)
		sp1 = segdupd(sp);
	else {
		sp1->s_flags = sp->s_flags;
		slrcopy(sp->s_mbase, sp1->s_mbase, sp->s_size);
	}
	return (sp1);
}

/*
 * Allocate a segment `n' bytes long.  `f' contains some pseudo flags.
 */
SEG *
salloc(n, f)
size_t n;
{
	register SEG *sp;
	register saddr_t s;
	register int r;

	r = (f&(SFSYST|SFHIGH|SFTEXT|SFSHRX|SFDOWN)) | SFCORE;
	s = btocru(n);
	lock(seglink);
	sp = sxalloc(s, f);
	unlock(seglink);

	/*
	 * Nothing large enough is free, but the arena may merely be in
	 * pieces.  Compact it and try once more before falling back on
	 * the swapper below: sliding a segment costs one memory to
	 * memory copy, where `segsext' costs two disk transfers.
	 */
	if (sp==NULL && segpack(s)!=0) {
		lock(seglink);
		sp = sxalloc(s, f);
		unlock(seglink);
	}
	if (sp != NULL)
		sp->s_flags = r;
	else {
		if ((f&SFNSWP) != 0)
			return (NULL);
		if ((sp=(SEG *)kalloc(sizeof(SEG))) == NULL)
			return (NULL);
		sp->s_forw = sp;
		sp->s_back = sp;
		sp->s_flags = r;
		sp->s_urefc = 1;
		sp->s_lrefc = 1;
		if (segsext(sp, s) == NULL) {
			kfree(sp);
			return (NULL);
		}
	}
	if ((f&SFNCLR) == 0)
		sclear(sp->s_mbase, s);
	return (sp);
}

/*
 * Free the given segment pointer.
 */
sfree(sp)
register SEG *sp;
{
	register INODE *ip;

	--sp->s_lrefc;
	if (--sp->s_urefc == 0) {
		if (sp->s_lrefc != 0)
			panic("Bad segment count");
		if ((ip=sp->s_ip) != NULL)
			ldetach(ip);
		lock(seglink);
		sp->s_back->s_forw = sp->s_forw;
		sp->s_forw->s_back = sp->s_back;
		unlock(seglink);
		kfree(sp);
	}
}

/*
 * Grow or shrink the segment `sp' so that it has size `n'.
 */
seggrow(sp, n)
register SEG *sp;
size_t n;
{
	register SEG *sp1;
	register saddr_t s;
	register saddr_t d;
	register saddr_t pb;
	register saddr_t nb;
	register int dowflag;

	dowflag = sp->s_flags&SFDOWN;
	s = btocru(n);

	/*
	 * Size of new segment is smaller or the same size as the old
	 * segment.
	 */
	lock(seglink);
	d = s - sp->s_size;
	if (s <= sp->s_size) {
		if (dowflag)
			sp->s_mbase -= d;
		sp->s_size = s;
		unlock(seglink);
		return (1);
	}
	if ((sp1=sp->s_back) == &segmq)
		pb = corebot;
	else
		pb = sp1->s_mbase + sp1->s_size;
	if ((sp1=sp->s_forw) == &segmq)
		nb = coretop;
	else
		nb = sp1->s_mbase;

	/*
	 * If the segment does not grow down, see if there is enough
	 * space after the segment.
	 */
	if (dowflag==0 && nb-sp->s_mbase>=s) {
		sclear(sp->s_mbase+sp->s_size, d);
		sp->s_size = s;
		unlock(seglink);
		return (1);
	}

	/*
	 * If the segment grows down, see if there is enough space
	 * before the segment.
	 */
	if (dowflag!=0 && sp->s_mbase+sp->s_size-pb>=s) {
		sp->s_mbase -= d;
		sp->s_size = s;
		sclear(sp->s_mbase, d);
		unlock(seglink);
		return (1);
	}

	/*
	 * Is there enough space in total counting the gaps on either
	 * side of us?
	 */
	if (nb-pb >= s) {
		if (dowflag == 0) {
			slrcopy(sp->s_mbase, pb, sp->s_size);
			sclear(pb+sp->s_size, d);
			sp->s_mbase = pb;
		} else {
			srlcopy(sp->s_mbase, nb-sp->s_size, sp->s_size);
			sclear(nb-s, d);
			sp->s_mbase = nb-s;
		}
		sp->s_size = s;
		unlock(seglink);
		return (1);
	}

	/*
	 * Try to allocate a segment somewhere else on the segment queue
	 * and copy ourselves there.
	 */
	unlock(seglink);
	if ((sp1=salloc(ctob((size_t)s), sp->s_flags|SFNSWP|SFNCLR)) != NULL) {
		if (dowflag == 0) {
			slrcopy(sp->s_mbase, sp1->s_mbase, sp->s_size);
			sclear(sp1->s_mbase+sp->s_size, d);
		} else {
			slrcopy(sp->s_mbase, sp1->s_mbase+d, sp->s_size);
			sclear(sp1->s_mbase, d);
		}
		lock(seglink);
		satcopy(sp, sp1);
		unlock(seglink);
		return (1);
	}

	/*
	 * Last chance.  Extend the segment by swapping it.
	 */
	if (segsext(sp, s) != NULL) {
		if (dowflag == 0)
			sclear(sp->s_mbase+s-d, d);
		else {
			srlcopy(sp->s_mbase, sp->s_mbase+d, s-d);
			sclear(sp->s_mbase, d);
		}
		return (1);
	}

	/*
	 * At least we tried.
	 */
	return (0);
}

/*
 * Given a segment pointer, `sp' and a segment size, grow the given segment
 * to the given size.  Returns non zero if the segment now has the requested
 * size, zero if it could not be grown (the old size is then still in force,
 * unless we could not even put that back, in which case we take a SIGSEGV).
 */
segsize(sp, s2)
register SEG *sp;
vaddr_t s2;
{
	register vaddr_t s1;
	register int r;

	s1 = ctob((vaddr_t)sp->s_size);
	if (seggrow(sp, (size_t)s2) == 0) {
		u.u_error = ENOMEM;
		return (0);
	}
	r = 1;
	if (sproto() == 0) {
		if (seggrow(sp, (size_t)s1)==0 || sproto()==0)
			sendsig(SIGSEGV, SELF);
		r = 0;
	}
	segload();
	return (r);
}

/*
 * Grow the segment `sp1' to the size `s' by swapping it out
 * and back in.  The segment may not be locked.
 */
SEG *
segsext(sp1, s)
register SEG *sp1;
register saddr_t s;
{
	register SEG *sp2;

#ifndef NOMONITOR
	if (swmflag)
		printf("Segsext(%p, %u)\n", SELF, SELF->p_pid);
#endif
	if (sexflag == 0) {
		u.u_error = ENOMEM;
		return (NULL);
	}
	lock(seglink);
	if ((sp2=sdalloc(s)) == NULL) {
		unlock(seglink);
		return (NULL);
	}
	unlock(seglink);
	sp1->s_lrefc++;
	if (sp1->s_size != 0)
		swapio(1, sp1->s_mbase, sp2->s_dbase, sp1->s_size);
	lock(seglink);
	satcopy(sp1, sp2);
	unlock(seglink);
	sp1->s_flags &= ~SFCORE;
	--sp1->s_lrefc;
	segfinm(sp1);
	return (sp1);
}

/*
 * Force the given segment to be in memory.  One can only force
 * one segment to be in memory at a time.
 */
segfinm(sp)
register SEG *sp;
{
	register PROC *pp;
	register int s;

	if ((sp->s_flags&SFCORE) != 0)
		return;
	pp = SELF;
	pp->p_segp[SIAUXIL] = sp;
	pp->p_flags &= ~PFCORE;
	s = sphi();
	setrun(pp);
	dispatch();
	spl(s);
	pp->p_segp[SIAUXIL] = NULL;
}

/*
 * Make a copy of the segment `sp1' which is in memory by writing
 * it out to disk.
 */
SEG *
segdupd(sp1)
register SEG *sp1;
{
	register SEG *sp2;

	if (sexflag == 0)
		return (NULL);
	lock(seglink);
	if ((sp2=sdalloc(sp1->s_size)) == NULL) {
		unlock(seglink);
		return (NULL);
	}
	sp1->s_lrefc++;
	unlock(seglink);
	swapio(1, sp1->s_mbase, sp2->s_dbase, sp1->s_size);
	--sp1->s_lrefc;
	sp2->s_ip = sp1->s_ip;
	sp2->s_flags = sp1->s_flags & ~SFCORE;
	sp2->s_size = sp1->s_size;
	return (sp2);
}

/*
 * Given a flag, a core address in clicks, a disk address and a count in
 * clicks, perform an I/O operation between core and disk.  If `flag' is
 * set, the transfer is to the disk otherwise it is to memory.  As you may
 * have guessed, this is used by the swapper.
 */
swapio(f, c, d, n)
saddr_t c;
daddr_t d;
saddr_t n;
{
	register BUF *bp;
	register int s;
	register int nb;

#ifndef NOMONITOR
	if (swmflag > 1)
		printf("swapio(%s,%x,%x,%x)\n",(f?"out":"in"),c,(int)d,n);
#endif
	if (d < swapbot || d+stod(n) > swaptop
	 || c < corebot || c+n > coretop)
		panic("Swapio bad parameter");
	bp = &swapbuf;
	lock(bp->b_gate);
	SELF->p_flags |= PFSWIO;
	while (n != 0) {
		nb = (n > btocrd(SCHUNK)) ? SCHUNK : ctob(n);
		bp->b_flag = BFNTP;
		bp->b_req = f ? BWRITE : BREAD;
		bp->b_dev = swapdev;
		bp->b_bno = d;
		bp->b_paddr = ctob((paddr_t)c);
		bp->b_count = nb;
		s = sphi();
		dblock(swapdev, bp);
		while ((bp->b_flag&BFNTP) != 0)
			sleep((char *)bp, CVBLKIO, IVBLKIO, SVBLKIO);
		spl(s);
		if ((bp->b_flag&BFERR) != 0)
			panic("Swapio error");
		c += btocrd(SCHUNK);
		d += stod(btocrd(SCHUNK));
		n -= btocrd(nb);
	}
	unlock(bp->b_gate);
	SELF->p_flags &= ~PFSWIO;
}

/*
 * Make the segment descriptor pointed to by `sp1' have the attributes
 * of `sp2' including it's position in the segment queue and release
 * `sp2'.  `seglink' must be locked when this routine is called.
 */
satcopy(sp1, sp2)
register SEG *sp1;
register SEG *sp2;
{
	sp1->s_back->s_forw = sp1->s_forw;
	sp1->s_forw->s_back = sp1->s_back;
	sp2->s_back->s_forw = sp1;
	sp1->s_back = sp2->s_back;
	sp2->s_forw->s_back = sp1;
	sp1->s_forw = sp2->s_forw;
	sp1->s_size = sp2->s_size;
	sp1->s_mbase = sp2->s_mbase;
	sp1->s_dbase = sp2->s_dbase;
	kfree(sp2);
}

/*
 * Allocate a segment on disk that is `n' clicks long.
 * The `seglink' gate should be locked before this routine is called.
 */
SEG *
sdalloc(s)
saddr_t s;
{
	register SEG *sp1;
	register SEG *sp2;
	register daddr_t d;
	register daddr_t d1;
	register daddr_t d2;

	d = stod(s);
	d1 = swapbot;
	sp1 = &segdq;
	do {
		if (d1 >= swaptop)
			return (NULL);
		if ((sp1=sp1->s_forw) != &segdq)
			d2 = sp1->s_dbase;
		else
			d2 = swaptop;
		if (d2-d1 >= d) {
			if ((sp2=(SEG *)kalloc(sizeof(SEG))) == NULL)
				return (NULL);
			sp1->s_back->s_forw = sp2;
			sp2->s_back = sp1->s_back;
			sp1->s_back = sp2;
			sp2->s_forw = sp1;
			sp2->s_urefc = 1;
			sp2->s_lrefc = 1;
			sp2->s_size = s;
			sp2->s_dbase = d1;
			return (sp2);
		}
		d1 = sp1->s_dbase + stod(sp1->s_size);
	} while (sp1 != &segdq);
	return (NULL);
}

/*
 * Allocate a segment in memory that is `n' clicks long.
 * The `seglink' gate should be locked before this routine is called.
 */
SEG *
smalloc(s)
saddr_t s;
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
			if ((sp2=(SEG *)kalloc(sizeof (SEG))) == NULL)
				return (NULL);
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

/*
 * Allocate a segment from the high end of memory that is `n' clicks long.
 * The `seglink' gate should be locked before this routine is called.
 */
SEG *
shalloc(s)
saddr_t s;
{
	register SEG *sp1;
	register SEG *sp2;
	register saddr_t s1;
	register saddr_t s2;

	sp1 = &segmq;
	s2 = coretop;
	do {
		if ((sp1=sp1->s_back) != &segmq)
			s1 = sp1->s_mbase + sp1->s_size;
		else
			s1 = corebot;
		if (s2-s1 >= s) {
			if ((sp2=(SEG *)kalloc(sizeof (SEG))) == NULL)
				return (NULL);
			sp1->s_forw->s_back = sp2;
			sp2->s_forw = sp1->s_forw;
			sp1->s_forw = sp2;
			sp2->s_back = sp1;
			sp2->s_urefc = 1;
			sp2->s_lrefc = 1;
			sp2->s_size = s;
			sp2->s_mbase = s2-s;
			return (sp2);
		}
		s2 = sp1->s_mbase;
	} while (sp1 != &segmq);
	return (NULL);
}

/*
 * Memory compaction.
 *
 * There is no paging here: a segment must occupy one contiguous run of
 * physical memory, so an arena that is merely in pieces defeats
 * `smalloc' even when the free total is ample.  `segpack' slides the
 * movable segments of `segmq' down against their predecessors, which
 * gathers the holes into a single one at the top.
 *
 * Relocation itself is nothing new - `seggrow' already copies a segment
 * to a roomier hole - because the hardware segmentation of a process is
 * only a cache: `mp_hsegs' in its u area, rebuilt by `sproto' from
 * `s_mbase' whenever PFSPROTO is set and reloaded by `segload' on the
 * way out to user mode.  A move is therefore a copy, a new `s_mbase'
 * and PFSPROTO on every owner.  The u area segment needs even less than
 * that: the context switch maps it straight from `p_u->s_mbase'.
 *
 * `pnxgate' is taken before `seglink' because the swapper already fixes
 * that order - it calls `procdisk' with `pnxgate' held, and that locks
 * `seglink'.
 */

/*
 * Move the segment `sp' down to `nb' if it is safe to do so, marking
 * each process that holds it as needing a fresh segmentation prototype
 * along the way.  Marking a segment we then decline to move is harmless
 * - it costs that process one `sproto' - so this is one walk of the
 * process table and not two.  Returns non zero if the segment moved.
 * Both gates must be held.
 *
 * Nothing outside `segmq' and the process table may be holding the old
 * address, which comes down to four things:
 *
 *	- The segment is resident.  Nothing else in `s_flags' disqualifies
 *	  it, and SFSYST in particular must NOT: `segdupl' copies
 *	  `s_flags' wholesale, so the SFSYST that `main' gave the very
 *	  first u area is inherited by every u area in the system.
 *	  Refusing it pinned one 2 Kb wall per process - measured at 7 of
 *	  them, stranding 300 Kb of the 411 Kb free - while the u areas
 *	  that really are the system's belong to PFKERN processes and are
 *	  refused below anyway.
 *
 *	- No owner is wired down.  PFSLIB is the one that bites: a shared
 *	  library's text is mapped for every process at once by a global
 *	  MMU descriptor that `slmap' built from `s_mbase' (exec.c), and
 *	  no amount of PFSPROTO would rebuild that.  Its holder carries
 *	  PFLOCK as well.  We also decline our own u area, since we are
 *	  running on the kernel stack inside it.
 *
 *	- Every reference is accounted for.  `s_urefc' must equal the
 *	  number of `p_segp' slots found.  This is what closes the windows
 *	  in which a segment exists but is not yet installed: `segdupl'
 *	  has bumped `s_urefc' but `segadup' has not stored the pointer
 *	  yet, or `exec' holds the segment in `u_sege' of a process asleep
 *	  in `exaread'.  Either way the count comes up short and we leave
 *	  the segment alone.
 *
 *	- No transient lock is outstanding.  Each owner that has not been
 *	  swapped out contributes one to `s_lrefc' (see `procdisk' and
 *	  `proccore'), so an `s_lrefc' above that count means somebody
 *	  holds the segment for I/O: `bio.c' across a raw transfer, whose
 *	  driver already has the physical address in `b_paddr', or
 *	  `segdisk' across the swap write that has not yet unlinked it
 *	  from `segmq'.  The swapper's static `segswap' placeholder is
 *	  permanently locked and has no owner at all, so both counts
 *	  refuse it.
 */
static
segmove(sp, nb)
register SEG *sp;
register saddr_t nb;
{
	register PROC *pp;
	register int i;
	register int n;
	register int c;

	if ((sp->s_flags&SFCORE) == 0)
		return (0);

	/*
	 * A downward growing segment is mapped with its top against the
	 * top of a hardware segment, so `uproto' hands the MMU a base of
	 * `s_mbase-(MSSIZE-s_size)'.  `saddr_t' is unsigned: refuse a
	 * destination that would make that subtraction wrap.
	 */
	if ((sp->s_flags&SFDOWN)!=0 && nb+sp->s_size<MSSIZE)
		return (0);
	n = 0;
	c = 0;
	for (pp=procq.p_nforw; pp!=&procq; pp=pp->p_nforw) {
		for (i=0; i<NUSEG+1; i++) {
			if (pp->p_segp[i] != sp)
				continue;
			if ((pp->p_flags&(PFLOCK|PFKERN|PFSWIO|PFSLIB)) != 0)
				return (0);
			if (i==SIUSERP && pp==SELF)
				return (0);
			pp->p_flags |= PFSPROTO;
			n++;
			if ((pp->p_flags&PFSWAP) == 0)
				c++;
		}
	}
	if (n==0 || n!=sp->s_urefc || c!=sp->s_lrefc)
		return (0);
	slrcopy(sp->s_mbase, nb, sp->s_size);
	sp->s_mbase = nb;
	return (1);
}

/*
 * Compact core so that a segment of `s' clicks may be allocated.
 * Returns the number of segments moved, which tells the caller whether
 * retrying its allocation is worth while.  Neither gate may be held.
 */
segpack(s)
register saddr_t s;
{
	register SEG *sp;
	register saddr_t b;
	saddr_t big;
	saddr_t tot;
	saddr_t gap;
	int moved;

	lock(pnxgate);
	lock(seglink);

	/*
	 * Walk the holes first - the same walk `mem' reports.  If the
	 * largest is already big enough the caller failed for some other
	 * reason (no `kalloc' space for the descriptor, say), and if the
	 * total is short then no amount of sliding will produce the room.
	 */
	tot = 0;
	big = 0;
	b = corebot;
	sp = &segmq;
	for (;;) {
		sp = sp->s_forw;
		gap = (sp==&segmq ? coretop : sp->s_mbase) - b;
		tot += gap;
		if (gap > big)
			big = gap;
		if (sp == &segmq)
			break;
		b = sp->s_mbase + sp->s_size;
	}
	moved = 0;
	if (big<s && tot>=s) {
		/*
		 * Slide everything movable down against whatever precedes
		 * it.  A segment we may not touch simply becomes the new
		 * floor, so one wired segment in the middle costs us just
		 * the hole in front of it.
		 */
		b = corebot;
		for (sp=segmq.s_forw; sp!=&segmq; sp=sp->s_forw) {
			if (sp->s_mbase>b && segmove(sp, b)!=0) {
				segpackc += sp->s_size;
				moved++;
			}
			b = sp->s_mbase + sp->s_size;
		}
		segpacks++;
	}
	unlock(seglink);
	unlock(pnxgate);
#ifndef NOMONITOR
	if (swmflag)
		printf("Segpack(%u) free %u largest %u moved %d\n",
			s, tot, big, moved);
#endif

	/*
	 * Our own mapping may be among the ones just invalidated;
	 * `segload' rebuilds it if `segmove' marked us.
	 */
	if (moved != 0)
		segload();
	return (moved);
}

/*
 * Set up `SR' structure in user area from segments descriptors in
 * process structure.  Also set up the user segmentation registers.
 */
sproto()
{
	register int n;
	register SEG *sp;
	register SR *srp;
	register PROC *pp;

	pp = SELF;
	kclear(u.u_segl, sizeof(u.u_segl));
	for (n=0; n<NUSEG; n++) {
		if ((sp=pp->p_segp[n]) == NULL)
			continue;
		srp = &u.u_segl[n];
		if (n == SIUSERP)
			srp->sr_base = (vaddr_t)&u;
		else
			srp->sr_flag |= SRFPMAP;
		if (n!=SISTEXT && n!=SISDATA)
			srp->sr_flag |= SRFDUMP;
		if (n!=SIUSERP && n!=SISTEXT && n!=SIPTEXT)
			srp->sr_flag |= SRFDATA;
		srp->sr_size = ctob(sp->s_size);
		srp->sr_segp = sp;
	}
	pp->p_flags &= ~PFSPROTO;
	return (mproto());
}

/*
 * Segment consistency checks for the paranoid.
segchk()
{
	register SEG *sp;
	register saddr_t s;
	register daddr_t d;
	register int nbad;

	nbad = 0;
	sp = &segmq;
	s = corebot;
	while ((sp=sp->s_forw) != &segmq) {
		if (sp->s_mbase < s)
			nbad += badseg("mem", sp->s_mbase, 0);
		s = sp->s_mbase + sp->s_size;
	}
	if (coretop < s)
		nbad += badseg("mem", sp->s_back->s_mbase, sp->s_back->s_size);
	sp = &segdq;
	d = swapbot;
	while ((sp=sp->s_forw) != &segdq) {
		if (sp->s_dbase < d)
			nbad += badseg("disk", (int)sp->s_dbase, 0);
		d = sp->s_dbase + stod(sp->s_size);
	}
	if (swaptop < d)
		nbad += badseg("disk", (int)sp->s_back->s_dbase, sp->s_back->s_size);
}

badseg(t, b, s)
char *t;
int b, s;
{
	printf("Bad %s segment %x for %x clicks\n", t, b, s);
	return (1);
}
*/
