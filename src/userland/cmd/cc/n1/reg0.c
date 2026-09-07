/*
 * This file contains the
 * machine independent parts of register
 * allocation. Most has to do with
 * extracting information from the register
 * tables.
 */
#ifdef   vax
#include "INC$LIB:cc1.h"
#else
#include "cc1.h"
#endif

/*
 * Get the name of the
 * high order half of a register pair.
 */
hihalf(r)
{
	return (reg[r].r_hihalf);
}

/*
 * Get the name of the
 * low half of a register pair.
 */
lohalf(r)
{
	return (reg[r].r_lohalf);
}

/*
 * Get the name of the register
 * pair that encloses the specified
 * register.
 */
enpair(r)
{
	return (reg[r].r_enpair);
}

/*
 * Check if a register is busy by
 * looking to see if any bits are set in
 * the busy mask.
 */
isbusy(r)
{
	if ((reg[r].r_phys&curbusy) != 0)
		return (1);
	return (0);
}

/*
 * Turn on the busy bits for register
 * `r' in the busy mask.
 */
setbusy(r)
{
	curbusy |= reg[r].r_phys;
}

/*
 * Turn off the busy bits for register
 * `r' in the busy mask.
 */
clrbusy(r)
{
	curbusy &= ~reg[r].r_phys;
}

/*
 * Allocate a register. The
 * register will be able to hold the
 * result type of tree `tp'.
 * Registers in the `used' set will
 * be avoided.
 * Flag = 0 means rvalue temp.
 * Flag = 1 means offset temp.
 * Flag = 2 means index  temp.
 * The register name is returned. A
 * -1 is returned if no register is
 * available.
 */
rallo(tp, used, flag)
TREE *tp;
register PREGSET used;
{
	register REGDESC *rp;
	register KIND kind;
	REGDESC *first, *hold;
	int naddr;

	kind  = pertype[tp->t_type].p_kind;
	used |= curbusy;
	if (flag != 2)
		used |= curxreg;
	/* first: the register this has always returned -- the highest that will
	 * serve.  hold: one that can HOLD this kind but can never ADDRESS it
	 * (r_lvalue empty), so spending it costs no addressing capability.
	 * naddr: how many registers that can address would still be free if
	 * `first' were taken. */
	first = hold = NULL;
	naddr = 0;
	rp = &reg[NRREG];
	while (--rp >= &reg[FRREG]) {
		if ((rp->r_phys&used) != 0)
			continue;
		if (flag == 0) {
			if ((rp->r_rvalue&kind) == 0)
				continue;
		} else {
			if ((rp->r_lvalue&kind) == 0)
				continue;
		}
		if (rp->r_lvalue == 0) {
			if (hold == NULL)
				hold = rp;
		} else if (first == NULL)
			first = rp;
		else
			naddr += 1;
	}
	/* Keep one register that can address.  A value temp does not care which
	 * register holds it, but an expression can need an address at the same
	 * time -- a subscript, an indirection, the base of a far pointer -- and
	 * when the last addressing register has been spent on a value there is
	 * nothing left to address through and no way to recover: the selector
	 * spills, and the spilled assignment wants the same address it could not
	 * form.  So when this allocation would take the last one, a register that
	 * could never have addressed anything takes the value instead.  A POINTER
	 * value is exempt: it is an address in waiting, and putting it where it
	 * cannot be dereferenced only moves the problem. */
	if (first == NULL)
		return (hold == NULL ? -1 : hold - &reg[0]);
	if (hold != NULL && naddr == 0 && flag == 0 && !ispoint(tp->t_type))
		return (hold - &reg[0]);
	return (first - &reg[0]);
}

/*
 * This routine figures out the
 * register name that gets passed down to
 * a subordinate. The first argument `s'
 * is a selector. The second is the current
 * temp. 
 */
reguse(selreg, tmpreg)
register selreg;
register tmpreg;
{
	if (selreg == TEMP)
		selreg = tmpreg;
	else if (selreg == LOTEMP)
		selreg = lohalf(tmpreg);
	else if (selreg == HITEMP)
		selreg = hihalf(tmpreg);
	return (selreg);
}
