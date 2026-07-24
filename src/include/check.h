/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * check.h -- exit-status bit flags shared by icheck, dcheck and the check
 * driver that runs them.  icheck and dcheck accumulate these bits in their
 * `exstat' and return them as their exit status; `check' reads that status
 * (via (wait-status >> 8) & 0377) and, in -s mode, decides whether the
 * damage is repairable.
 *
 * The IC_* space (icheck) and the DC_* space (dcheck) are independent: each
 * program only ever sets and returns bits from its own space, and check
 * masks icheck's result with IC_* and dcheck's with DC_*.
 *
 * IC_FIX / DC_FIX are the subset of problems that a `-s' pass can repair
 * (free-list / superblock reconstruction for icheck; inode-clear and
 * link-count correction for dcheck).  IC_HARD/IC_MISC and DC_HARD/DC_MISC
 * are unrecoverable and are deliberately excluded from the *_FIX masks, so
 * check's test `(err & ~*_FIX) == 0' is true only when every set bit is
 * repairable.
 *
 * NOTE: this header was lost from the source tree and is reconstructed from
 * its three consumers (check.c, icheck.c, dcheck.c).  The set of flags, and
 * which of them belong to the repairable *_FIX masks, are fixed by that use;
 * the specific bit *values* are not (they are private to these three tools,
 * which are rebuilt together), so a distinct-bit layout is chosen here.
 */

#ifndef	_CHECK_H_
#define	_CHECK_H_

/*
 * icheck (i-list / block) status bits.
 */
#define	IC_MISC	0x01		/* Miscellaneous error (usage, open, args) */
#define	IC_HARD	0x02		/* Hard I/O error -- not repairable */
#define	IC_MISS	0x04		/* Blocks missing from the free list */
#define	IC_DUPF	0x08		/* Duplicate block in the free list */
#define	IC_BFB	0x10		/* Bad free-block / i-free list */
#define	IC_BADF	0x20		/* Bad block number in the free list */

/* Problems icheck -s can repair (all free-list / superblock related). */
#define	IC_FIX	(IC_MISS | IC_DUPF | IC_BFB | IC_BADF)

/*
 * dcheck (directory / link-count) status bits.
 */
#define	DC_MISC	0x01		/* Miscellaneous error (usage, open, args) */
#define	DC_HARD	0x02		/* Hard I/O error -- not repairable */
#define	DC_CLRI	0x04		/* Unreferenced allocated inode to clear */
#define	DC_LCE	0x08		/* Link-count error to correct */

/* Problems dcheck -s can repair. */
#define	DC_FIX	(DC_CLRI | DC_LCE)

#endif	/* _CHECK_H_ */
