	.shri
	.globl _lfalse_
	.globl _land_
	.globl _landn_
	.globl _lsrc_
	.globl _lnand_
	.globl _ldst_
	.globl _lxor_
	.globl _lor_
	.globl _lnandn_
	.globl _lnxor_
	.globl _lndst_
	.globl _lorn_
	.globl _lnsrc_
	.globl _lnor_
	.globl _lnorn_
	.globl _ltrue_


/  logop( src, dst, mask )
/
/	Do the logical operation between a source and destination word,
/	changing only those bits in the destination which are set in mask.
/
/	SS|4(r15)	-> src
/	SS|6(r15)	-> dst
/	SS|8(r15)	-> mask
/

_lfalse_:
	ld	r1, SS|8(r15)
	com	r1
	and	r1, SS|6(r15)		/ r1 = ~mask & dst
	ret	un


_land_:
	ld	r1, SS|8(r15)
	ld	r2, r1
	and	r1, SS|4(r15)
	and	r1, SS|6(r15)		/ r1 = mask & src & dst
	com	r2
	and	r2, SS|6(r15)		/ r2 = ~mask & dst
	or	r1, r2
	ret	un


_landn_:
	ld	r1, SS|8(r15)
	ld	r3, SS|6(r15)
	ld	r2, r1
	com	r2
	and	r2, r3			/ r2 = ~mask & dst
	com	r3
	and	r1, SS|4(r15)
	and	r1, r3			/ r1 = mask & src & ~dst
	or	r1, r2
	ret	un


_lsrc_:
	ld	r1, SS|8(r15)
	ld	r2, r1
	com	r2
	and	r2, SS|6(r15)		/ r2 = ~mask & dst
	and	r1, SS|4(r15)		/ r1 = mask & src
	or	r1, r2
	ret	un


_lnand_:
	ld	r2, SS|8(r15)
	ld	r1, SS|4(r15)
	com	r1
	and	r1, r2
	and	r1, SS|6(r15)		/ r1 = ~src & mask & dst
	com	r2
	and	r2, SS|6(r15)		/ r2 = ~mask & dst
	or	r1, r2
	ret	un


_ldst_:
	ld	r1, SS|6(r15)
	ret	un


_lxor_:
	ld	r2, SS|8(r15)
	ld	r1, SS|6(r15)
	ld	r3, r1
	xor	r1, SS|4(r15)
	and	r1, r2			/ r1 = (dst ^ src) & mask
	com	r2
	and	r2, r3			/ r2 = ~mask & dst
	or	r1, r2
	ret	un


_lor_:
	ld	r1, SS|4(r15)
	and	r1, SS|8(r15)
	or	r1, SS|6(r15)		/ r1 = src & mask | dst
	ret	un


_lnandn_:
	ld	r2, SS|8(r15)
	ld	r1, SS|4(r15)
	com	r1
	and	r1, r2			/ r1 = ~src & mask
	ld	r3, SS|6(r15)
	com	r2
	and	r2, r3			/ r2 = ~mask & dst
	com	r3
	and	r1, r3			/ r1 ^= ~dst
	or	r1, r2
	ret	un


_lnxor_:
	ld	r1, SS|4(r15)
	com	r1
	xor	r1, SS|6(r15)
	ld	r2, SS|8(r15)
	and	r1, r2			/ r1 = (~src ^ dst) & mask
	com	r2
	and	r2, SS|6(r15)		/ r2 = ~mask & dst
	or	r1, r2
	ret	un


_lndst_:
	ld	r1, SS|6(r15)
	xor	r1, SS|8(r15)		/ r1 = dst ^ mask
	ret	un


_lorn_:
	ld	r2, SS|8(r15)
	ld	r1, SS|4(r15)
	and	r1, r2			/ r1 = src & mask
	xor	r2, SS|6(r15)		/ r2 = mask ^ dst
	or	r1, r2
	ret	un


_lnsrc_:
	ld	r2, SS|8(r15)
	ld	r1, SS|4(r15)
	com	r1
	and	r1, r2			/ r1 = ~src & mask
	com	r2
	and	r2, SS|6(r15)		/ r2 = ~mask & dst
	or	r1, r2
	ret	un


_lnor_:
	ld	r1, SS|4(r15)
	com	r1
	and	r1, SS|8(r15)
	or	r1, SS|6(r15)		/ r1 = ~src & mask | dst
	ret	un


_lnorn_:
	ld	r2, SS|8(r15)
	ld	r1, SS|4(r15)
	com	r1
	and	r1, r2			/ r1 = ~src & mask
	xor	r2, SS|6(r15)		/ r2 = mask ^ dst
	or	r1, r2
	ret	un


_ltrue_:
	ld	r1, SS|8(r15)
	or	r1, SS|6(r15)		/ r1 = mask | dst
	ret	un
