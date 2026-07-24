/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * libfs -- logical-to-physical block map (imap).
 *
 * For inode `ip', map logical block `lb' onto its physical disc block, walking
 * the single/double/triple indirect blocks as needed.  This is the copy that
 * dcheck and ncheck each used to carry verbatim.  The Coherent inode has ND
 * direct addresses followed by one single, one double and one triple indirect
 * (NI = NII = NIII = 1); the ranges/offsets/coeff tables encode that access
 * polynomial.  Indirect blocks are read through the FS's private scratch buffer
 * so the caller's own block buffer is never disturbed.
 */
#include <stdio.h>
#include <fs.h>
#include <canon.h>

#undef	NI
#undef	NII
#undef	NIII
#define	NI	1
#define	NII	1
#define	NIII	1

static	daddr_t	ranges[] = {
	ND,
	ND + (daddr_t)NI*NBN,
	ND + (daddr_t)NI*NBN + (daddr_t)NII*NBN*NBN,
	ND + (daddr_t)NI*NBN + (daddr_t)NII*NBN*NBN + (daddr_t)NIII*NBN*NBN*NBN,
};

static	char	offsets[] = {
	0,
	ND,
	ND+NI,
	ND+NI+NII,
};

static	daddr_t	coeff[] = {
	1, (daddr_t)NBN, (daddr_t)NBN*NBN, (daddr_t)NBN*NBN*NBN
};

daddr_t
fsimap(fsp, ip, lb)
register FS *fsp;
register struct dinode *ip;
daddr_t lb;
{
	register il;
	daddr_t bpos, pb;
	register daddr_t *bp;
	register daddr_t addrs[NADDR];

	l3tol(addrs, ip->di_addr, NADDR);
	for (il = 0; il < 4; il++)
		if (lb < ranges[il]) {
			if (il != 0)
				lb -= ranges[il-1];
			bpos = lb/coeff[il];
			lb %= coeff[il];
			bp = &addrs[(int)bpos + offsets[il]];
			if ((pb = *bp) != 0) {
				/*
				 * Map through indirect blocks here.
				 */
				while (il-- > 0) {
					fsbread(fsp, pb, fsp->dbuf);
					bpos = lb/coeff[il];
					lb %= coeff[il];
					bp = (daddr_t *)fsp->dbuf + bpos;
					if ((pb = *bp) == 0)
						break;
					pb = *bp;
					candaddr(pb);
				}
			}
			return (pb);
		}
	return (0);
}
