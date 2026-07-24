/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Pipes.
 */
#include <coherent.h>
#include <errno.h>
#include <filsys.h>
#include <ino.h>
#include <inode.h>
#include <io.h>
#include <proc.h>
#include <sched.h>
#include <signal.h>
#include <uproc.h>

/*
 * Create and return a locked pipe inode.  This is called from the
 * pipe system call.
 */
INODE *
pmake(mode)
{
	register INODE *ip;

	if ((ip=ialloc(pipedev, IFPIPE|mode)) != NULL) {
		iclear(ip);
		ip->i_a.i_pnc = 0;
		ip->i_a.i_prx = 0;
		ip->i_a.i_pwx = 0;
	}
	return (ip);
}

/*
 * Open a pipe given the inode pointer.
 */
popen(ip, mode)
{
}

/*
 * Close a pipe inode.
 */
pclose(ip)
register INODE *ip;
{
	if (ip->i_refc == 2) {
		pevent(ip);
		ip->i_flag |= IFEOF;
	}
}

/*
 * Only one end of the pipe is going to be left.
 */
pevent(ip)
register INODE *ip;
{
	if ((ip->i_flag&IFWFR) != 0) {
		ip->i_flag &= ~IFWFR;
		wakeup((char *)&ip->i_a.i_pwx);
	}
	if ((ip->i_flag&IFWFW) != 0) {
		ip->i_flag &= ~IFWFW;
		wakeup((char *)&ip->i_a.i_prx);
	}
}

/*
 * Read from a pipe.  The given inode is locked.
 */
pread(ip, iop)
register INODE *ip;
register IO *iop;
{
	register unsigned n;
	register unsigned ioc;

	while (ip->i_a.i_pnc == 0) {
		if ((ip->i_flag&IFEOF) != 0) {
			ip->i_flag &= ~IFEOF;
			break;
		}
		if (ip->i_nlink==0 && ip->i_refc<2)
			break;
		ip->i_flag |= IFWFW;
		iunlock(ip);
		sleep((char *)&ip->i_a.i_prx, CVPIPE, IVPIPE, SVPIPE);
		ilock(ip);
	}
	if ((ip->i_flag&IFEOF)!=0 && ip->i_a.i_pnc==0)
		ip->i_flag &= ~IFEOF;
	ioc = iop->io_ioc;
	while (u.u_error==0 && ioc>0 && ip->i_a.i_pnc>0) {
		if ((n=PIPSIZE-ip->i_a.i_prx) > ioc)
			n = ioc;
		if (n > ip->i_a.i_pnc)
			n = ip->i_a.i_pnc;
		iop->io_ioc = n;
		iop->io_seek = ip->i_a.i_prx;
		fread(ip, iop);
		n -= iop->io_ioc;
		if ((ip->i_a.i_prx+=n) == PIPSIZE)
			ip->i_a.i_prx = 0;
		ip->i_a.i_pnc -= n;
		ioc -= n;
	}
	iop->io_ioc = ioc;
	if ((ip->i_flag&IFWFR)!=0 && ip->i_a.i_pnc<PIPSIZE) {
		ip->i_flag &= ~IFWFR;
		wakeup((char *)&ip->i_a.i_pwx);
	}
}

/*
 * Write to a pipe.  The given inode is locked.
 */
pwrite(ip, iop)
register INODE *ip;
register IO *iop;
{
	register unsigned n;
	register unsigned ioc;

	ioc = iop->io_ioc;
	while (u.u_error==0 && ioc>0) {
		if (ip->i_refc<2 && ip->i_nlink==0) {
			u.u_error = EPIPE;
			sendsig(SIGPIPE, SELF);
			return;
		}
		if ((n=PIPSIZE-ip->i_a.i_pwx) > ioc)
			n = ioc;
		if (n > PIPSIZE-ip->i_a.i_pnc)
			n = PIPSIZE - ip->i_a.i_pnc;
		if (n==0 || (ip->i_flag&IFEOF)!=0) {
			ip->i_flag |= IFWFR;
			iunlock(ip);
			sleep((char *)&ip->i_a.i_pwx, CVPIPE, IVPIPE, SVPIPE);
			ilock(ip);
			continue;
		}
		iop->io_ioc = n;
		iop->io_seek = ip->i_a.i_pwx;
		fwrite(ip, iop);
		n -= iop->io_ioc;
		if ((ip->i_a.i_pwx+=n) == PIPSIZE)
			ip->i_a.i_pwx = 0;
		ip->i_a.i_pnc += n;
		ioc -= n;
		if ((ip->i_flag&IFWFW) && ip->i_a.i_pnc>0) {
			ip->i_flag &= ~IFWFW;
			wakeup((char *)&ip->i_a.i_prx);
		}
	}
	iop->io_ioc = ioc;
}
