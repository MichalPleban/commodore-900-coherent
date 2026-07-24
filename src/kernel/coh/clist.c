/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Character list management.
 */
#include <coherent.h>
#include <clist.h>
#include <sched.h>

/*
 * Initialise character list queues.
 */
cltinit()
{
	register cmap_t cm;
	register cmap_t lm;
	register paddr_t p;
	register int s;
	cold_t om;

	s = sphi();
	csave(om);
	lm = 0;
	for (p = clistp+NCLIST*sizeof(CLIST); (p-=sizeof(CLIST)) >= clistp; ) {
		cm = cconv(p);
		cmapv(cm);
		cvirt(cm)->cl_fp = lm;
		lm = cm;
	}
	cltfree = lm;
	crest(om);
	spl(s);
}

/*
 * Get a character from the given queue.
 */
getq(cqp)
register CQUEUE *cqp;
{
	register cmap_t op;
	register cmap_t np;
	register int ox;
	register int c;
	register int s;
	cold_t om;

	if (cqp->cq_cc == 0)
		return (-1);
	s = sphi();
	op = cqp->cq_op;
	ox = cqp->cq_ox;
	csave(om);
	cmapv(op);
	c = cvirt(op)->cl_ch[ox]&0377;
	crest(om);
	if (--cqp->cq_cc==0 || ++cqp->cq_ox==NCPCL) {
		cqp->cq_ox = 0;
		cmapv(op);
		np = cvirt(op)->cl_fp;
		cvirt(op)->cl_fp = cltfree;
		crest(om);
		cqp->cq_op = np;
		cltfree = op;
		if (np == 0) {
			cqp->cq_ip = 0;
			cqp->cq_ix = 0;
		}
		if (cltwant) {
			cltwant = 0;
			wakeup((char *)&cltwant);
		}
	}
	spl(s);
	return (c);
}

/*
 * Put a character on the given queue.
 */
putq(cqp, c)
register CQUEUE *cqp;
{
	register cmap_t ip;
	register int ix;
	register int s;
	register cmap_t np;
	cold_t om;

	s = sphi();
	ip = cqp->cq_ip;
	csave(om);
	if ((ix=cqp->cq_ix) == 0) {
		if ((ip=cltfree) == 0) {
			spl(s);
			return (-1);
		}
		cmapv(ip);
		cltfree = cvirt(ip)->cl_fp;
		cvirt(ip)->cl_fp = 0;
		crest(om);
		if ((np=cqp->cq_ip) == 0)
			cqp->cq_op = ip;
		else {
			cmapv(np);
			cvirt(np)->cl_fp = ip;
			crest(om);
		}
		cqp->cq_ip = ip;
	}
	cmapv(ip);
	cvirt(ip)->cl_ch[ix] = c;
	crest(om);
	if (++cqp->cq_ix == NCPCL)
		cqp->cq_ix = 0;
	cqp->cq_cc++;
	spl(s);
	return (c);
}

/*
 * Clear a character queue.
 */
clrq(cqp)
register CQUEUE *cqp;
{
	register int s;

	s = sphi();
	while (getq(cqp) >= 0)
		;
	spl(s);
}

/*
 * Wait for more character queues to become available.
 */
waitq()
{
	while (cltfree == 0) {
		cltwant = 1;
		sleep((char *)&cltwant, CVCLIST, IVCLIST, SVCLIST);
	}
}
