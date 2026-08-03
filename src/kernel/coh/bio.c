/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Buffered I/O.
 */
#include <coherent.h>
#include <buf.h>
#include <drvcon.h>
#include <errno.h>
#include <io.h>
#include <proc.h>
#include <sched.h>
#include <seg.h>
#include <stat.h>
#include <uproc.h>

/*
 * Initialise buffer headers.
 */
bufinit()
{
	register BUF *bp;
	register paddr_t p;
	register int i;

	p = blockp;
	bufl = kalloc(NBUF * sizeof(BUF));
	if (bufl == NULL)
		panic("bufinit: no space for BUF's");
	for (i=0; i<NBUF; i++) {
		bp = &bufl[i];
		bp->b_dev = NODEV;
		bp->b_vaddr = bvirt(bconv(p));
		bp->b_paddr = p;
		p += BSIZE;
	}
}

/*
 * Synchronise the buffer cache.
 */
bsync()
{
	register BUF *bp;

	for (bp=bufl; bp<&bufl[NBUF]; bp++) {
		if ((bp->b_flag&BFMOD) == 0)
			continue;
		lock(bp->b_gate);
		if ((bp->b_flag&BFMOD) != 0)
			bwrite(bp, 1);
		unlock(bp->b_gate);
	}
}

/*
 * Synchronise all block for a particular device in the buffer cache
 * and invalidate all references.
 */
bflush(dev)
register dev_t dev;
{
	register BUF *bp;

	for (bp=bufl; bp<&bufl[NBUF]; bp++) {
		if (bp->b_dev != dev)
			continue;
		lock(bp->b_gate);
		if (bp->b_dev == dev) {
			if ((bp->b_flag&BFMOD) != 0)
				bwrite(bp, 1);
			bp->b_dev = NODEV;
		}
		unlock(bp->b_gate);
	}
}

/*
 * Return a buffer containing the given block from the given device.
 * If `f' is not set, the read is asynchronous and no buffer is returned.
 */
BUF *
bread(dev, bno, f)
dev_t dev;
daddr_t bno;
register int f;
{
	register BUF *bp;
	register int s;

	bp = bclaim(dev, bno);
	if ((bp->b_flag&BFNTP) != 0) {
		if (f != 0)
			bp->b_flag &= ~BFASY;
		else {
			bp->b_flag |= BFASY;
			bumap(bp);
		}
		bp->b_req = BREAD;
		bp->b_count = BSIZE;
		s = sphi();
		dblock(dev, bp);
		if (f == 0) {
			spl(s);
			return (NULL);
		}
#if READAHEAD > 0
		spl(s);
		bread(dev, bno+1, 0);
#endif
#if READAHEAD > 1
		bread(dev, bno+2, 0);
#endif
#if READAHEAD > 2
		bread(dev, bno+3, 0);
#endif
#if READAHEAD > 3
		bread(dev, bno+4, 0);
#endif
#if READAHEAD > 0
		sphi();
#endif
		while ((bp->b_flag&BFNTP) != 0)
			sleep((char *)bp, CVBLKIO, IVBLKIO, SVBLKIO);
		spl(s);
		if ((bp->b_flag&BFERR) != 0) {
			u.u_error = bp->b_err ? bp->b_err : EIO;
			brelease(bp);
			return (NULL);
		}
		if (bp->b_resid == BSIZE) {
			brelease(bp);
			return (NULL);
		}
	}
	if (f == 0) {
		brelease(bp);
		return (NULL);
	}
	u.u_block++;
	return (bp);
}

/*
 * If the requested buffer is in the buffer cache, return a pointer to
 * it.  If not, pick an empty buffer, set it up and return it.
 */
BUF *
bclaim(dev, bno)
dev_t dev;
daddr_t bno;
{
	register BUF *bp;
	register BUF *bp1;
	register unsigned seqn;
	register int s;

again:
	bp1 = NULL;
	seqn = 0;
	for (bp=bufl; bp<&bufl[NBUF]; bp++) {
		if (bp->b_dev==dev && bp->b_bno==bno) {
			lock(bp->b_gate);
			if (bp->b_dev!=dev || bp->b_bno!=bno) {
				unlock(bp->b_gate);
				goto again;
			}
			if ((bp->b_flag&BFERR) != 0)
				bp->b_flag |= BFNTP;
			bsmap(bp);
			return (bp);
		}
		if (locked(bp->b_gate) == 0) {
			if (bufseqn-bp->b_seqn >= seqn) {
				bp1 = bp;
				seqn = bufseqn - bp->b_seqn;
			}
		}
	}
	if (bp1 == NULL) {
		s = sphi();
		for (bp=bufl; bp<&bufl[NBUF]; bp++) {
			if (locked(bp->b_gate) == 0) {
				if (bufseqn-bp->b_seqn >= seqn) {
					bp1 = bp;
					seqn = bufseqn - bp->b_seqn;
				}
			}
		}
		if (bp1 == NULL) {
			bufneed = 1;
			sleep((char *)&bufneed, CVBLKIO, IVBLKIO, SVBLKIO);
			spl(s);
			goto again;
		}
		spl(s);
	}
	bp = bp1;
	lock(bp->b_gate);
	if ((bp->b_flag&BFMOD) != 0) {
		bwrite(bp, 0);
		goto again;
	}
	bp->b_flag = BFNTP;
	bp->b_dev = dev;
	bp->b_bno = bno;
	bsmap(bp);
	return (bp);
}

/*
 * Write the given buffer out.  If `f' is set, the write is synchronous,
 * otherwise asynchronous.  This routine must be called with the buffer
 * gate locked.
 */
bwrite(bp, f)
register BUF *bp;
{
	register int s;

	if (f != 0)
		bp->b_flag &= ~BFASY;
	else {
		bp->b_flag |= BFASY;
		bumap(bp);
	}
	bp->b_flag |= BFNTP;
	bp->b_req = BWRITE;
	bp->b_count = BSIZE;
	s = sphi();
	dblock(bp->b_dev, bp);
	if (f == 0) {
		spl(s);
		return;
	}
	while ((bp->b_flag&BFNTP) != 0)
		sleep((char *)bp, CVBLKIO, IVBLKIO, SVBLKIO);
	spl(s);
}

/*
 * This is called by the driver when I/O has completed on a buffer.
 */
bdone(bp)
register BUF *bp;
{
	if (bp->b_req == BWRITE)
		bp->b_flag &= ~BFMOD;
	if (bp->b_req == BREAD) {
		if ((bp->b_flag&BFERR) != 0)
			bp->b_dev = NODEV;
	}
	if ((bp->b_flag&BFASY) != 0) {
		bp->b_flag &= ~BFASY;
		brelease(bp);
	}
	bp->b_flag &= ~BFNTP;
	wakeup((char *)bp);
}

/*
 * Release the given buffer.
 */
brelease(bp)
register BUF *bp;
{
	if ((bp->b_flag&BFERR) == 0)
		bp->b_seqn = bufseqn++;
	else {
		bp->b_flag &= ~BFERR;
		bp->b_dev = NODEV;
	}
	bp->b_flag &= ~BFNTP;
	bumap(bp);
	unlock(bp->b_gate);
	if (bufneed != 0) {
		bufneed = 0;
		wakeup((char *)&bufneed);
	}
}

/*
 * Map the given buffer.
 */
bsmap(bp)
register BUF *bp;
{
	bsave(bp->b_map);
	bp->b_flag |= BFMAP;
	bmapv(bconv(bp->b_paddr));
}

/*
 * Unmap the given buffer.
 */
bumap(bp)
register BUF *bp;
{
	if ((bp->b_flag&BFMAP) == 0)
		return;
	bp->b_flag &= ~BFMAP;
	brest(bp->b_map);
}

/*
 * Read data from the I/O segment into kernel space.
 */
ioread(iop, v, n)
register IO *iop;
register char *v;
register unsigned n;
{
	switch (iop->io_seg) {
	case IOSYS:
		iop->io_base += kkcopy(iop->io_base, v, n);
		break;
	case IOUSR:
		iop->io_base += ukcopy(iop->io_base, v, n);
		break;
	case IOPHY:
		iop->io_phys += pkcopy(iop->io_phys, v, n);
		break;
	}
	iop->io_ioc -= n;
}

/*
 * Write data from kernel space to the I/O segment.
 */
iowrite(iop, v, n)
register IO *iop;
register char *v;
register unsigned n;
{
	switch (iop->io_seg) {
	case IOSYS:
		iop->io_base += kkcopy(v, iop->io_base, n);
		break;
	case IOUSR:
		iop->io_base += kucopy(v, iop->io_base, n);
		break;
	case IOPHY:
		iop->io_phys += kpcopy(v, iop->io_phys, n);
		break;
	}
	iop->io_ioc -= n;
}

/*
 * Get a character from the I/O segment.
 */
iogetc(iop)
register IO *iop;
{
	register int c;

	if (iop->io_ioc == 0)
		return (-1);
	--iop->io_ioc;
	if (iop->io_seg == IOSYS)
		c = *iop->io_base++ & 0377;
	else {
		c = getubd(iop->io_base++);
		if (u.u_error)
			return (-1);
	}
	return (c);
}

/*
 * Put a character using the I/O segment.
 */
ioputc(c, iop)
register IO *iop;
{
	if (iop->io_ioc == 0)
		return (-1);
	--iop->io_ioc;
	if (iop->io_seg == IOSYS)
		*iop->io_base++ = c;
	else {
		putubd(iop->io_base++, c);
		if (u.u_error)
			return (-1);
	}
	return (c);
}

/*
 * Given a buffer pointer, an I/O structure, a device, request type, and
 * a flags word, check the I/O structure and perform the I/O request.
 */
ioreq(bp, iop, dev, req, f)
register BUF *bp;
register IO *iop;
dev_t dev;
{
	register SEG *sp;
	register int n;
	register int s;
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	lock(bp->b_gate);
	n = cp->c_flag;	/* n should do something with that flag */
	drest(dold);
	sp = NULL;
	if (iop != NULL) {
		if ((f&BFBLK) != 0) {
			if (blocko(iop->io_seek) != 0) {
				u.u_error = EIO;
				goto out;
			}
		}
		if ((f&BFIOC) != 0) {
			if ((sp=iomapvp(iop, bp)) == NULL) {
				u.u_error = EIO;
				goto out;
			}
		}
	}
	bp->b_flag = f|BFNTP;
	bp->b_req = req;
	bp->b_dev = dev;
	if (iop != NULL) {
		bp->b_bno = blockn(iop->io_seek);
		bp->b_count = iop->io_ioc;
	}
	if (sp != NULL)
		sp->s_lrefc++;
	s = sphi();
	dblock(dev, bp);
	while ((bp->b_flag&BFNTP) != 0)
		sleep((char *)bp, CVBLKIO, IVBLKIO, SVBLKIO);
	spl(s);
	if (sp != NULL)
		--sp->s_lrefc;
	/*
	 * Only poke the swapper if its timer is actually armed; otherwise
	 * every completed block transfer costs a needless walk of the
	 * process list looking for a sleeper that cannot be there.
	 */
	if (stimer.t_func != NULL)
		wakeup((char *)&stimer);
	if ((bp->b_flag&BFERR) != 0) {
		u.u_error = bp->b_err ? bp->b_err : EIO;
		goto out;
	}
	if (iop != NULL) {
		n = iop->io_ioc - bp->b_resid;
		iop->io_seek += n;
		iop->io_ioc -= n;
	}
out:
	unlock(bp->b_gate);
}

/*
 * Given an I/O structure and a buffer header, see if the addresses
 * in the I/O structure are valid and set up the buffer header.
 */
SEG *
iomapvp(iop, bp)
register IO *iop;
register BUF *bp;
{
	register SR *srp;
	register SEG *sp;
	register vaddr_t b;

	if (iop->io_seg != IOUSR)
		panic("Raw I/O from non user");
	for (srp=u.u_segl; srp<&u.u_segl[NUSEG]; srp++) {
		if ((sp=srp->sr_segp) == NULL)
			continue;
		if ((srp->sr_flag&SRFDATA) == 0)
			continue;
/* Yet another bug in the 8000 C compiler
		if ((long)(b=iop->io_base) < (long)srp->sr_base)
*/
		if ((b=iop->io_base) < srp->sr_base)
			continue;
		if ((long)b+iop->io_ioc > (long)srp->sr_base+ctob(sp->s_size))
			continue;
		bp->b_paddr = ctob((paddr_t)sp->s_mbase) +
				vtop(b) - vtop(srp->sr_base);
		return (sp);
	}
	return (NULL);
}

/*
 * Initialise devices.
 */
devinit()
{
	register DRV *dp;
	register DRV *dpe;

	for (dp=drvl, dpe=&dp[drvn]; dp<dpe; dp++)
		if (dp->d_conp != NULL && dp->d_conp->c_load != NULL)
			(*dp->d_conp->c_load)();
}

/*
 * Open a device.
 */
dopen(dev, m, f)
register dev_t dev;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	if ((cp->c_flag&f) == 0) {
		u.u_error = ENXIO;
		return;
	}
	(*cp->c_open)(dev, m);
	drest(dold);
}

/*
 * Close a device.
 */
dclose(dev)
register dev_t dev;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_close)(dev);
	drest(dold);
}

/*
 * Call the block entry point of a device.
 */
dblock(dev, bp)
dev_t dev;
BUF *bp;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_block)(bp);
	drest(dold);
}

/*
 * Read from a device.
 */
dread(dev, iop)
register dev_t dev;
register IO *iop;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_read)(dev, iop);
	drest(dold);
}

/*
 * Write to a device.
 */
dwrite(dev, iop)
register dev_t dev;
register IO *iop;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_write)(dev, iop);
	drest(dold);
}

/*
 * Call the ioctl function for a device.
 */
dioctl(dev, com, vec)
register dev_t dev;
union ioctl *vec;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_ioctl)(dev, com, vec);
	drest(dold);
}

/*
 * Call the powerfail entry point of a device.
 */
dpower(dev)
register dev_t dev;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_power)(dev);
	drest(dold);
}

/*
 * Call the timeout entry point of a device.
 */
dtime(dev)
register dev_t dev;
{
	register CON *cp;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return;
	(*cp->c_timer)(dev);
	drest(dold);
}

/*
 * Given a device, return the flags word.
 */
dflag(dev)
dev_t dev;
{
	register CON *cp;
	register int f;
	dold_t dold;

	if ((cp=drvmap(dev, &dold)) == NULL)
		return (DFERR);
	f = cp->c_flag;
	drest(dold);
	return (f);
}

/*
 * Given a device, and a pointer to a driver map save area, save the
 * current map in the driver map save area and map in the new device,
 * returning a pointer to the configuration entry for that device.
 */
CON *
drvmap(dev, doldp)
dev_t dev;
dold_t *doldp;
{
	register DRV *dp;
	register unsigned m;

	if ((m=major(dev)) >= drvn) {
		u.u_error = ENXIO;
		return (NULL);
	}
	dp = &drvl[m];
	if (locked(dp->d_gate)) {
		u.u_error = ENXIO;
		return (NULL);
	}
	if (dp->d_conp == NULL) {
		u.u_error = ENXIO;
		return (NULL);
	}
	dsave(*doldp);
	if (dp->d_map != 0)
		dmapv(dp->d_map);
	return (dp->d_conp);
}

/*
 * Non existant device.
 */
nonedev()
{
	u.u_error = ENXIO;
}

/*
 * Null device.
 */
nulldev()
{
}
