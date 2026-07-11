/*
 * Nroff/Troff.
 * Traps and diversions.
 */
#include <stdio.h>
#include "roff.h"
#include "code.h"
#include "div.h"
#include "reg.h"
#include "str.h"

/*
 * Given a name, create and initialise a new diversion.
 */
newdivn(name)
char name[2];
{
	register DIV *dp;

	dp = (DIV *) nalloc(sizeof (DIV));
	dp->d_next = cdivp;
	dp->d_name[0] = name[0];
	dp->d_name[1] = name[1];
	dp->d_rpos = 0;
	dp->d_nspm = 0;
	dp->d_maxh = 0;
	dp->d_maxw = 0;
	dp->d_seek = tmpseek;
	dp->d_macp = NULL;
	dp->d_stpl = NULL;
	dp->d_trap = NULL;
	dp->d_ctpp = NULL;
	cdivp = dp;
}

/*
 * End a diversion.
 */
enddivn()
{
	register REG *rp;
	register DIV *dp;

	if ((dp=cdivp) == mdivp) {
		printe("Cannot end diversion");
		return;
	}
	if ((rp=findreg(dp->d_name)) != NULL) {
		rp->r_maxh = cdivp->d_maxh;
		rp->r_maxw = cdivp->d_maxw;
	}
	nrdnreg->r_nval = cdivp->d_maxh;
	nrdlreg->r_nval = cdivp->d_maxw;
	cdivp = cdivp->d_next;
	nfree(dp);
}

/*
 * Append the given buffer onto the end of the current diversion.
 */
flushd(buffer, bufend)
CODE *buffer, *bufend;
{
	register DIV *dp;
	register MAC *mp;
	register int n;

	dp = cdivp;
	mp = dp->d_macp;
	if (mp->m_type!=MDIVN || dp->d_seek!=tmpseek) {
		dp->d_macp = dp->d_macp->m_next = mp = nalloc(sizeof *mp);
		mp->m_next = NULL;
		mp->m_type = MDIVN;
		mp->m_size = 0;
		mp->m_core = NULL;
		mp->m_seek = tmpseek;
	}
	n = bufend - buffer;
	nwrite(buffer, sizeof (CODE), n);
	mp->m_size += n;
	dp->d_seek = tmpseek;
	if ((n=tmpseek%512) != 0)
		nwrite(miscbuf, 1, 512-n);
}

/*
 * Space the required distance after a line has been put out.
 * If we sprung a trap, execute it.
 */
lspace(n)
{
	register DIV *dp;
	register TPL *tp;

	dp = cdivp;
	tp = dp->d_ctpp;
	if (tp!=NULL && dp->d_rpos+n<tp->t_apos)
		tp = NULL;
	scroll(dp, n);
	if (tp != NULL)
		execute(tp->t_name);
}

/*
 * Space to the end of the current page.
 */
pspace()
{
	register DIV *dp;
	register TPL *tp;
	int lpct;
	register int npos;

	dp = mdivp;
	lpct = pct;
	while (lpct == pct) {
		npos = pgl;
		tp = dp->d_ctpp;
		if (tp!=NULL && npos>=tp->t_apos)
			npos = tp->t_apos;
		if (mdivp==dp && npos>=pgl) {
			scroll(dp, pgl-dp->d_rpos);
			npos = dp->d_rpos;
			tp = dp->d_trap;
		}
		scroll(dp, npos-dp->d_rpos);
		if (tp!=NULL && dp->d_rpos==tp->t_apos)
			execute(tp->t_name);
	}
}

/*
 * Space the distance `n'.  If a trap is encountered, spring the
 * trap and stop.
 */
sspace(n)
{
	register DIV *dp;
	register TPL *tp;
	register int npos;

	dp = cdivp;
	tp = dp->d_ctpp;
	npos = dp->d_rpos + n;
	if (npos < 0)
		npos = 0;
	if (tp!=NULL && npos>=tp->t_apos)
		npos = tp->t_apos;
	if (mdivp==dp && npos>=pgl) {
		scroll(dp, pgl-dp->d_rpos);
		npos = dp->d_rpos;
		tp = dp->d_trap;
	}
	scroll(dp, npos-dp->d_rpos);
	if (tp!=NULL && dp->d_rpos==tp->t_apos)
		execute(tp->t_name);
}

/*
 * Space down the given distance, resetting page information
 * if we pass a page boundary.  `dp' is a pointer to the
 * diversion we are using.
 */
scroll(dp, n)
register DIV *dp;
{
	CODE code[1];
	register TPL *tp;

	code[0].c_code = DSPAR;
	code[0].c_iarg = n;
	if (dp == mdivp)
		flushl(code, &code[1]);
	else
		flushd(code, &code[1]);
	if ((cdivp->d_rpos+=n) > cdivp->d_maxh)
		cdivp->d_maxh = cdivp->d_rpos;
	while (cdivp->d_rpos >= pgl) {
		if (byeflag)
			exit(0);
		pct++;
		pno = npn++;
		cdivp->d_rpos -= pgl;
		cdivp->d_maxh = cdivp->d_rpos;
		cdivp->d_ctpp = cdivp->d_trap;
	}
	tp = dp->d_ctpp;
	while (tp!=NULL && tp->t_apos<=dp->d_rpos)
		tp = tp->t_next;
	dp->d_ctpp = tp;
}

/*
 * Execute a macro, given the name.
 */
execute(name)
char name[2];
{
	register REG *rp;

	if ((rp=findreg(name, RTEXT)) != NULL) {
		adstreg(rp);
		strp->s_eoff = 1;
		process();
	}
}
