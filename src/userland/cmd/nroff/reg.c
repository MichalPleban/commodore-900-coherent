/*
 * Nroff/Troff.
 * Register manipulation.
 */
#include <stdio.h>
#include "roff.h"
#include "reg.h"

/*
 * Return a pointer to a number register of the given name.
 */
char	*
getnreg(name)
char name[2];
{
	register REG *rp;

	if ((rp=findreg(name, RNUMR)) == NULL) {
		rp = makereg(name, RNUMR);
		rp->r_nval = 0;
		rp->r_form = '1';
		rp->r_incr = 1;
	}
	return (rp);
}

/*
 * Create a register of the given name and type.  If one
 * already exists, remove it.
 */
char	*
makereg(name, type)
char name[2];
{
	REG **rpp;
	register REG *rp;
	register MAC *mp, *lmp;

	if (rp=findreg(name, type)) {
		if (rp->r_type == RTEXT) {
			rp->r_maxh = 0;
			rp->r_maxw = 0;
			mp = rp->r_macd.m_next;
			while (mp) {
				if (mp->m_type==MTEXT && mp->m_core!=NULL)
					nfree(mp->m_core);
				lmp = mp;
				mp = mp->m_next;
				nfree(lmp);
			}
		}
	} else {
		rpp = &regt[hash(name)];
		rp = (REG *) nalloc(sizeof *rp);
		rp->r_type = type;
		rp->r_name[0] = name[0];
		rp->r_name[1] = name[1];
		rp->r_maxh = 0;
		rp->r_maxw = 0;
		rp->r_next = *rpp;
		*rpp = rp;
	}
	return (rp);
}

/*
 * Remove the given text register or request.  Return 1 if
 * the register is found, else 0.
 */
reltreg(name)
char name[2];
{
	MAC *lmp;
	register MAC *mp;
	register REG *rp, **lrp;

	for (lrp=&regt[hash(name)]; rp=*lrp; lrp=&rp->r_next) {
		if (rp->r_name[0]==name[0] && rp->r_name[1]==name[1]) {
			if (rp->r_type != RTEXT)
				continue;
			mp = rp->r_macd.m_next;
			while (mp) {
				if (mp->m_type==MTEXT && mp->m_core!=NULL)
					nfree(mp->m_core);
				lmp = mp;
				mp = mp->m_next;
				nfree(lmp);
			}
			*lrp = rp->r_next;
			nfree(rp);
			return (1);
		}
	}
	return (0);
}

/*
 * Remove the given number register.  Return 1 if we found it,
 * else 0.
 */
relnreg(name)
char name[2];
{
	register REG *rp, **lrp;

	for (lrp=&regt[hash(name)]; rp=*lrp; lrp=&rp->r_next) {
		if (rp->r_name[0]==name[0] && rp->r_name[1]==name[1]) {
			if (rp->r_type == RNUMR) {
				*lrp = rp->r_next;
				nfree(rp);
				return (1);
			}
		}
	}
	return (0);
}

/*
 * Given a register name, and a register type, return a pointer
 * to the register if it exists.  If not, NULL is returned.
 */
char	*
findreg(name, type)
char name[2];
{
	register REG *rp;

	for (rp=regt[hash(name)]; rp; rp=rp->r_next)
		if (rp->r_name[0]==name[0] && rp->r_name[1]==name[1])
			if (rp->r_type == type)
				return (rp);
	return (NULL);
}

/*
 * Convert a string to a two character register name.
 */
argname(str, name)
register char *str;
char name[2];
{
	if ((name[0]=str[0]) == '\0')
		name[1] = '\0';
	else
		name[1] = str[1];
}
