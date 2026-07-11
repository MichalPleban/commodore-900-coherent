/* (-lgl
 * 	The information contained herein is a trade secret of Mark Williams
 * 	Company, and  is confidential information.  It is provided  under a
 * 	license agreement,  and may be  copied or disclosed  only under the
 * 	terms of  that agreement.  Any  reproduction or disclosure  of this
 * 	material without the express written authorization of Mark Williams
 * 	Company or persuant to the license agreement is unlawful.
 * 
 * 	COHERENT Version 0.7.3
 * 	Copyright (c) 1982, 1983, 1984.
 * 	An unpublished work by Mark Williams Company, Chicago.
 * 	All rights reserved.
 -lgl) */
/*
 * Coherent.
 * Variables.
 */
#include <coherent.h>
#include <buf.h>
#include <drvcon.h>
#include <inode.h>
#include <mount.h>
#include <proc.h>
#include <ptrace.h>
#include <seg.h>
#include <timeout.h>

int	 debflag = 0;			/* coherent.h */

int	 alcflag;			/* coherent.h */
int	 batflag;			/* coherent.h */
int	 outflag;			/* coherent.h */
int	 ttyflag;			/* coherent.h */
unsigned utimer;			/* coherent.h */
TIM	stimer;			/* coherent.h */
unsigned msize;				/* coherent.h */
unsigned asize;				/* coherent.h */
char	 *icodep;			/* coherent.h */
int	 icodes;			/* coherent.h */
saddr_t	 corebot;			/* coherent.h */
saddr_t	 coretop;			/* coherent.h */
paddr_t	 blockp;			/* coherent.h */
paddr_t	 clistp;			/* coherent.h */
struct	 all *allkp;			/* coherent.h */

unsigned bufseqn;			/* buf.h */
int	 bufneed;			/* buf.h */
BUF	 swapbuf;			/* buf.h */
BUF	*bufl;				/* buf.h */

int	cltwant;			/* clist.h */
cmap_t	cltfree;			/* clist.h */

INODE	*inodep;			/* inode.h */
INODE	*acctip;			/* inode.h */

MOUNT	*mountp;			/* mount.h */

int	quantum;			/* proc.h */
int	disflag;			/* proc.h */
int	intflag;			/* proc.h */
int	cpid;				/* proc.h */
GATE	pnxgate;			/* proc.h */
PROC	procq;				/* proc.h */
PROC	*iprocp;			/* proc.h */
PROC	*eprocp;			/* proc.h */
PROC	*cprocp;			/* proc.h */
PROC	*slprocp;			/* proc.h */
PLINK	linkq[NHPLINK];			/* proc.h */

struct	ptrace pts;			/* ptrace.h */

int	sexflag;			/* seg.h */
GATE	seglink;			/* seg.h */
#ifndef NOMONITOR
int	swmflag;			/* seg.h */
#endif
SEG	segswap;			/* seg.h */
SEG	segmq;				/* seg.h */
SEG	segdq;				/* seg.h */

TIM	timl;				/* timeout.h */
