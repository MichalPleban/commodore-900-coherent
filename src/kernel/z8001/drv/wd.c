/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Driver for Western Digital hard disk and Commodore floppy disk
 * controller for the Z8000HR.
 * 
 * Uses Commodore's SASI-like command block structure.
 *
 * Last Revision: April 12, 1985 (nrb)
 * wdioctl to format floppies added 6-10-85, MG (needs work)
 */

#define FMTCMD  3			/* for wdioctl */
#include	<coherent.h>
#include	<buf.h>
#include	<drvcon.h>
#include	<sched.h>
#include	<stat.h>
#include	<uproc.h>
#include	<errno.h>
//#include	<fdioctl.h>

int	wdload();
int	wduload();
int	wdopen();
int	wdread();
int	wdwrite();
int	wdblock();
int	wdintr();
int	wdioctl();
int	nulldev();
int	nonedev();
char	*pfix();			/* map phys addr to a char * */
daddr_t	wdbno();			/* returns actual block offset */

CON	wdcon	= {
	DFBLK|DFCHR,			/* Flags */
	2,				/* Major index */
	wdopen,				/* Open */
	nulldev,			/* Close */
	wdblock,			/* Block */
	wdread,				/* Read */
	wdwrite,			/* Write */
	wdioctl,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	wdload,				/* Load */
	wduload				/* Unload */
};

/*
 * Western Digital Controller port addresses
 */
#define	WDIO	0x0500			/* Data Register (word mode) */

#define	WDTDR	0x00			/* test drive ready */
#define	WDREST	0x01			/* restore to cyl 0 */
#define	WDRSS	0x03			/* Request status */
#define	WDCTF	0x05			/* check track format */
#define	WDFMTT	0x06			/* format track */
#define	WDREAD	0x08			/* Read */
#define	WDWRITE	0x0A			/* Write */
#define	WDSDP	0x0C			/* set drive parameters */
#define	WDCCBA	0x0F			/* change command block address */
#define	WDDDIAG	0xE3			/* run drive diagnostics */
#define	WDCDIAG	0xE4			/* run controller diagnostics */
#define	CFFMT	0x04			/* format floppy disk */
#define	CFCCB	0x0F			/* change command block address */

#define	FWPERROR 0x77			/* write protect error */
#define	FNOSENSE 0x70			/* no sense error */
#define	FMERROR	 0x73			/* non recoverable media error */
#define FCMERROR  0x76			/* changed-media error */

#define	NSEC	17			/* number of sectors per track */
#define	WDVEC	0x80			/* base of wd interrupt vector */
#define	NDRIVES	4			/* max drives 2 floppy + 2 hard */
#define	NHDISKS	2			/* first 2 drives are hard */
#define	NFBLK	2392			/* number of blocks per floppy */
#define	MAXBLKCNT 255			/* c_blockcnt is a single byte */

typedef	struct	wdcmd {			/* command block layout */
 	union {
		struct {
			unsigned char	c_opcode; /* command class & opcode */
			unsigned char	c_lunhiaddr; /* lun & addr [4:0] */
			unsigned char	c_midaddr; /* middle sector address */
			unsigned char	c_lowaddr; /* low sector address */
		} wdcmdop;
		unsigned long	c_lunaddr;	/* lun and whole address */
	} wdcmdu;
	unsigned char	c_blockcnt;	/* number of blocks in I/O */
	unsigned char	c_control;	/* reserved control byte */
	unsigned char	c_highdma;	/* high DMA addr (phys segment) */
	unsigned char	c_middma;	/* middle DMA address */
	unsigned char	c_lowdma;	/* low DMA address */
	unsigned char	c_rsvd1;	/* reserved */
	unsigned char	c_rsvd2;	/* reserved */
	unsigned char	c_rsvd3;	/* reserved */
	unsigned char	c_errorbits;	/* error infowdation */
	unsigned char	c_lunladd2;	/* error lun and high sector addr */
	unsigned char	c_ladd1;	/* error middle address */
	unsigned char	c_ladd0;	/* error low address */
} WDCMD;
#define	c_op	wdcmdu.wdcmdop.c_opcode

#define	bool	int
#define	TRUE	(0 == 0)
#define	FALSE	(0 != 0)


/*
 * minor device layout
 *	bits 7-4: physical drive number
 *	bits 3-0: pseudo-drive number (partition) within the physical drive
 */
#define	drive(mdev)	((mdev) >> 4)		/* get drive # */
#define	pseudo(mdev)	((mdev) & 0xF)		/* get partition # */

#define	NPSEUDO	(sizeof wdbtab/sizeof wdbtab[0])

struct {
	unsigned long	bstart;		/* starting block # */
	unsigned long	bcount;		/* size in blocks */
} wdbtab[] = {
	0L,	10336L,
	10336L,	10336L,
	20672L,	10336L,
	31008L,	10336L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	0L,
	0L,	41344L
};

/*
 * drive & controller parameters used during initialization. 
 * note that the parameters are used for both drives by the controller
 * (sorry, no mix and match on drive types).
 */
typedef	struct	wdp {
	unsigned char	p_options;	/* options <7:4>, step rate <3:0> */
	unsigned char	p_head_cyl;	/* # heads <6:4>, MSB # cyl <3:0> */
	unsigned char	p_cyl;		/* LSB # cylinders <7:0> */
	unsigned char	p_precomp;	/* precomp cyl/16  <7:0> */
	unsigned char	p_reduced;	/* reduced write cur. cyl/16 <7:0> */
	unsigned char	p_nsectors;	/* sectors per track <7:0> */
} WDP;
WDP	wdparam = {			/* patch these values as needed: */
	0xF,				/* no options, 16 uS fast step */
	(4 << 4) | (612/256),		/* 4 heads, MSB of cyl */
	612 % 256,			/* # cyl LSB */
	128 / 16,			/* precomp starts at cyl 128. */
	128 / 16,			/* reduced write cur. at cyl 128. */
	17				/* number of sectors per track */
};

/*
 * housekeeping
 */
typedef	struct wdunit {
	bool		wdu_active;	/* drive/controller busy */
	bool		wdu_dma_aligned;/* dma address is 512 aligned */
	struct wdunit	*wdu_forw;	/* next drive in controller chain */
	struct wdunit	*wdu_back;	/* last drive */
	BUF		*wdu_actf;	/* next BUF in drive chain */
	paddr_t		wdu_paddr;	/* physical address of buffer */
} WDU;

WDU	wdtab;				/* controller header */
WDU	wdutab[NDRIVES];		/* drive headers */
BUF	wdbuf;				/* BUF structure */
/*
 * cmdblk is a 3*512 byte block aligned on a 512 byte boundary in physical
 * memory. It is declared in md.s to use here. WDBUFPADDR must be a paddr_t
 * pointing to cmdblk+(2*512) for the code to work correctly !!!
 * the CF entries are for the floppy command block.
 */
extern	char	cmdblk[];
/* NORM compiled this with non-default values which aren't recorded
 * anywhere I can find. Inserted the values determined from disassem-
 * bling the system.  rec 6.V.85 */
#define WDCMDBLKPADDR	0x80000L
#define WDBUFPADDR	0x80400L
#ifndef	WDCMDBLKPADDR
#define	WDCMDBLKPADDR	0x20000L	/* default to 2|0000 based RAM */
#endif
#ifndef	WDBUFPADDR
#define	WDBUFPADDR	0x20400L	/* above cmd blk and ECC buffer */
#endif
#define	CFCMDBLKPADDR	(WDCMDBLKPADDR+0x10L)

/*
 * set up the driver and the actual controller. this involves setting the
 * drive parameters for the two drives on the hard disk. the floppy uses
 * the default internal parameters, so no need to set them here.
 */
wdload()
{
	register int i;

	for (i = 0; i < NDRIVES; ++i)
		wdutab[i].wdu_active = 0;
	wdtab.wdu_active = 0;
	setivec(WDVEC, wdintr);
	wdsetparam();
}


wduload()
{

	clrivec(WDVEC);
}

/*
 * Open routine. Check that the minor # is in range. Note that the
 * floppy minor # controls formatting. 0 == normal operation, 1 == format.
 */
wdopen(dev, mode)
dev_t	dev;
{
	register unsigned int d, p;		/* drive & partition #'s */

	d = drive(minor(dev));
	p = pseudo(minor(dev));
	if (d >= NDRIVES
	    || (p >= NPSEUDO && d < NHDISKS)
	    || (p > 1 && d >= NHDISKS))
		u.u_error = ENXIO;
	/* if (d >= NHDISKS && p != 0) */
		/* wdfformat(dev); */			/* format floppy */
}

/*
 * perform a RAW read operation
 */
wdread(dev, iop)
dev_t	dev;
IO	*iop;
{
	ioreq(&wdbuf, iop, dev, BREAD, BFIOC|BFBLK|BFRAW);
}


/*
 * perform a RAW read operation
 */
wdwrite(dev, iop)
dev_t	dev;
IO	*iop;
{
	ioreq(&wdbuf, iop, dev, BWRITE, BFIOC|BFBLK|BFRAW);
}

/*
 * block I/O routine.
 */
wdblock(bp)
register BUF	*bp;
{
	register unsigned int	d;		/* drive # */
	register unsigned int	pd;		/* partition # */
	register daddr_t  bno;			/* block # */
	register int	s;			/* saved machine status */
	daddr_t	bsize;				/* size of the medium in blocks */
	daddr_t	nblk;				/* # blocks we can really do */
	bool	wdstart();

	d = drive(minor(bp->b_dev));
	pd = pseudo(minor(bp->b_dev));
	bno = bp->b_bno;
#if	VERBOSE
	printf("wdblock: d=%d, pd=%d, bno=%d enter\n", d, pd, (int)bno);
	wdbufdump(bp);
#endif
	if (d >= NDRIVES
	    || (d < NHDISKS && pd >= NPSEUDO)
	    || (d >= NHDISKS && pd != 0)) {
		bp->b_flag |= BFERR;
		bdone(bp);
		return;
	}
	if (d < NHDISKS)
		bsize = wdbtab[pd].bcount;
	else
		bsize = NFBLK;
	/*
	 * Clip the transfer to the end of the medium and to what a single
	 * controller command can express.  RAW I/O arrives here as ONE BUF
	 * covering the whole user request, so this is the only place where a
	 * transfer running off the end of the partition can be caught: the
	 * controller would happily DMA over the start of the next one.
	 * Whatever we do not transfer must be reported in b_resid, since
	 * ioreq() credits the user with b_count - b_resid bytes and nothing
	 * else in this driver ever touches it.
	 */
	nblk = bp->b_count >> 9;
	if (nblk > (daddr_t)MAXBLKCNT)
		nblk = MAXBLKCNT;
	if (bno < 0 || bno >= bsize)
		nblk = 0;
	else if (bno + nblk > bsize)
		nblk = bsize - bno;
	if ((bp->b_flag&BFRAW) == 0) {
		/*
		 * Buffered I/O is always exactly one block: anything that
		 * would have to be clipped is a real error here, and a short
		 * transfer would leave the cache holding garbage.
		 */
		if (nblk != (daddr_t)(bp->b_count >> 9)) {
			bp->b_flag |= BFERR;
			bdone(bp);
			return;
		}
		bp->b_resid = 0;
	} else {
		bp->b_resid = bp->b_count - ((vaddr_t)nblk << 9);
		bp->b_count = (vaddr_t)nblk << 9;
		if (nblk == 0) {		/* nothing of it is on the disk */
			bdone(bp);
			return;
		}
	}
	if (d < NHDISKS)			/* only for hard disks */
		bno += wdbtab[pd].bstart;
	s = sphi();
	disksort(&wdutab[d], bp, bno);
	wdustart(d);
	while (!wdstart())
		;
	spl(s);
}

/*
 * start up a disk I/O request to the controller if one is not already
 * in progress.
 */
wdustart(d)
register int	d;					/* drive # */
{
	register BUF	*bp;

	if (wdutab[d].wdu_active)		/* active already ? */
		return;
	bp = wdutab[d].wdu_actf;
	if (bp == (BUF *)0)			/* any work to do ? */
		return;
	wdutab[d].wdu_active = TRUE;

	if (wdtab.wdu_forw != (WDU *)0)
		wdtab.wdu_back->wdu_forw = &wdutab[d];
	else
		wdtab.wdu_forw = &wdutab[d];
	wdtab.wdu_back = &wdutab[d];
	wdutab[d].wdu_forw = (WDU *)0;
	return;
}

/*
 * actually set up the command block for the WD controller to perform 
 * the I/O.  Check to see if DMA address is aligned on a 512 byte
 * boundary as required by the Commodore (MOS Technology) DMA controller.
 * All RAW I/O must be aligned !!! All other I/O must be aligned if greater
 * than 512 bytes (1 block) of I/O is performed. If a non-RAW non-aligned
 * write, copy the user's data to our wdiobuf which is aligned. If a read,
 * do the copy in the interrupt routine after the data is in the wdiobuf
 * buffer.
 * This is the price we have to pay for a poorly designed DMA controller.
 */
bool
wdstart()
{
	register BUF	*bp;
	register WDCMD	*cbp;
	register unsigned int	d, pd;
	int	dma_error = FALSE;

	if (wdtab.wdu_active)
		return (TRUE);
	if (wdtab.wdu_forw == (WDU *)0)
		return (TRUE);
	bp = wdtab.wdu_forw->wdu_actf;
	wdtab.wdu_paddr = bp->b_paddr;
	d = drive(minor(bp->b_dev));
	pd = pseudo(minor(bp->b_dev));
	wdtab.wdu_dma_aligned = TRUE;
	if ((wdtab.wdu_paddr&0x1FFL) != 0) {	/* make sure aligned 512 */
		wdtab.wdu_dma_aligned = FALSE;
		if (bp->b_flag&BFRAW) {
			printf("wd: non-aligned dma in RAW mode\n");
			++dma_error;
		} else if (bp->b_count > 512) {
			printf("wd: non-aligned multi-block dma error\n");
			++dma_error;
		}
	}
	if (dma_error) {
		wdtab.wdu_forw->wdu_active = 0;
		wdtab.wdu_forw->wdu_actf = bp->b_actf;
		wdtab.wdu_forw = wdtab.wdu_forw->wdu_forw;
		bp->b_flag |= BFERR;
		bdone(bp);
		wdustart(d);
		return (FALSE);
	}
	wdtab.wdu_active = TRUE;
	if (!wdtab.wdu_dma_aligned && bp->b_req == BWRITE) /* one block only! */
		kkcopy(pfix(WDS, wdtab.wdu_paddr), cmdblk+0x400, 512);
	if (d >= NHDISKS)			/* map appropriate cmd block */
		cbp = (WDCMD *) pfix(WDS, CFCMDBLKPADDR);	/* floppy */
	else
		cbp = (WDCMD *) pfix(WDS, WDCMDBLKPADDR);	/* hard */
	wdcmdclr(cbp);				/* clear out block */
	if (bp->b_req == FMTCMD)
		return wdformat(bp, cbp);
	cbp->wdcmdu.c_lunaddr = bp->b_bno + wdbtab[pd].bstart;
	if (d == 1 || d == NHDISKS+1)		/* second hard or floppy disk */
		cbp->wdcmdu.wdcmdop.c_lunhiaddr |= 1 << 5;
	cbp->c_blockcnt = bp->b_count >> 9;
	if (wdtab.wdu_dma_aligned) {
		cbp->c_highdma = wdtab.wdu_paddr >> 16;
		cbp->c_middma = wdtab.wdu_paddr >> 8;
		cbp->c_lowdma = wdtab.wdu_paddr;
	} else {
		cbp->c_highdma = (WDBUFPADDR >> 16) & 0xFF;
		cbp->c_middma =  (WDBUFPADDR >> 8) & 0xFF;
		cbp->c_lowdma =  (WDBUFPADDR) & 0xFF;
	}
	cbp->c_errorbits = 0xFF;		/* indicate ready */
	cbp->c_op = ((bp->b_req==BREAD) ? WDREAD : WDWRITE);
#if	VERBOSE
	wdcbdump(cbp);
#endif
	out(WDIO, 1);				/* wake up controller */
	return (TRUE);
}

/*
 * got an interrupt from the controller - see if it was a valid I/O
 * completion or an error.
 */
wdintr()
{
	register BUF	*bp;
	register WDCMD	*cbp;
	register unsigned int	d;

	if (wdtab.wdu_active) {
		bp = wdtab.wdu_forw->wdu_actf;
		d = drive(minor(bp->b_dev));
		if (d >= NHDISKS)
			cbp = (WDCMD *) pfix(WDS, CFCMDBLKPADDR); /* floppy */
		else
			cbp = (WDCMD *) pfix(WDS, WDCMDBLKPADDR); /* hard */
		if ((cbp->c_errorbits & 0x7F) == 0) {
			wdtab.wdu_forw->wdu_actf = bp->b_actf;
			if (!wdtab.wdu_dma_aligned)	/* one block only !! */
			    if (bp->b_req == BREAD)
			      kkcopy(cmdblk+0x400,pfix(WDS,wdtab.wdu_paddr),512);
			bdone(bp);
		} else
			wdharderr(bp, cbp);
		wdtab.wdu_forw->wdu_active = 0;
		wdtab.wdu_forw = wdtab.wdu_forw->wdu_forw;
		wdtab.wdu_active = 0;
		wdustart(d);
	}
	out(WDIO, 0);				/* reset IEO on DMA chip */
	while (!wdstart())
		;
}

/*
 * got an error - inform the user and try to recover.
 */
wdharderr(bp, cbp)
register BUF	*bp;
register WDCMD	*cbp;
{
	register unsigned int	d;
	register unsigned char *ucp;
	register unsigned int pd;
	register unsigned int bno;	/* only used for display !! */

	pd = pseudo(minor(bp->b_dev));
	bno = bp->b_bno;
	pd = cbp->c_errorbits & 0x7F;
	if ((d = drive(minor(bp->b_dev))) >= NHDISKS)
		printf("fd: floppy disk #%d: ", d-NHDISKS);
	else
		printf("hd: hard disk #%d", d);
	if (pd == FWPERROR && d >= NHDISKS)		/* write protected ?? */
		printf("write protect error. ");
	else if (pd == FNOSENSE && d >= NHDISKS)	/* no floppy ? */
		printf("no floppy in drive. ");
	else if (pd == FMERROR && d >= NHDISKS)
		printf("irrecoverable media error. ");
	else if (pd == FCMERROR && d >= NHDISKS)
		/*
		 * The diskette was swapped under us.  The queued transfer
		 * belongs to the OLD diskette, so it must NOT be re-issued:
		 * fall through and fail it like any other hard error.
		 */
		printf("diskette changed. ");
	else {
		printf(" error=%x [ ", pd);
		for (d = 0, ucp = (unsigned char *)cbp; d < sizeof(WDCMD); ++d)
			printf("%x ", *ucp++);
		printf("] ");
	}
	devmsg(bp->b_dev, " b=%u\n", bno);
	d = drive(minor(bp->b_dev));
	wdutab[d].wdu_actf = bp->b_actf;
	bp->b_flag |= BFERR;
	bdone(bp);
}

/*
 * clear out the command block pointed to by cbp.
 */
wdcmdclr(cbp)
WDCMD	*cbp;
{
	register unsigned char *ucp;
	register int i;

	ucp = (unsigned char *)cbp;
	for (i = 0; i < sizeof(WDCMD); ++i)
		*ucp++ = 0;
}

/*
 * return the actual device block offset given a buffer. this makes
 * the disk sorting process so much easier.
 */
daddr_t
wdbno(bp)
register BUF *bp;
{
	register unsigned int pd;

	pd = pseudo(minor(bp->b_dev));
	if (drive(minor(bp->b_dev)) >= NHDISKS)		/* floppies ??? */
		return (bp->b_bno);
	return (bp->b_bno + wdbtab[pd].bstart);
}

/*
 * sort request into queue
 * 'bp is inserted in the queue of the device (should be a disk drive)
 * given by 'dp'.  A weaving algorithm is used.
 */
disksort(dp, bp, bno)
struct wdunit	*dp;
register BUF	*bp;
daddr_t	bno;
{
	register BUF	*last;
	register BUF	*next;


	next = dp->wdu_actf;
	if (next == (BUF *)0) {
		dp->wdu_actf = bp;
		bp->b_actf = next;
		return;
	}

	while (last=next, next=next->b_actf)
		if ((wdbno(last) <= bno && bno <= wdbno(next))
		|| (wdbno(last) >= bno && bno >= wdbno(next))) {
			last->b_actf = bp;
			bp->b_actf = next;
			return;
		}

	last->b_actf = bp;
	bp->b_actf = next;
}

#if	VERBOSE
/*
 * dump a BUF struct to the terminal
 */
wdbufdump(bp)
register BUF *bp;
{
	printf("BUF: flag=%x dev=%x bno=%u req=%x count=%d\n", bp->b_flag,
		bp->b_dev, (int)bp->b_bno, bp->b_req, (int)bp->b_count);
	printf("     resid=%d vaddr=%p paddr=%p bp=%p\n", (int)bp->b_resid,
		bp->b_vaddr, bp->b_paddr, bp);
}

/*
 * dump a command block to the terminal
 */
wdcbdump(cbp)
register WDCMD	*cbp;
{
	register int	d;
	register unsigned char *ucp;

	printf("wd/cf: cb = [ ", cbp->c_errorbits);
	for (d = 0, ucp = (unsigned char *)cbp; d < sizeof(WDCMD); ++d)
		printf("%x ", *ucp++);
	printf("] ");
}
#endif

/*
 * perform controller and drive initialization. note that the parameters
 * given to the controller are used for both hard disk drives.
 * note that we have to make sure that both command blocks have been 
 * cleared to avoid problems with controller interaction.
 */
wdsetparam()
{
	register WDCMD *cbp;			/* cmd block pointer */
	register WDP *dpp;			/* drive param pointer */

	cbp = (WDCMD *) pfix(WDS, CFCMDBLKPADDR); /* map onto cmd block */
	wdcmdclr(cbp);				/* clear out block */
	cbp = (WDCMD *) pfix(WDS, WDCMDBLKPADDR); /* now hard disk cmd blk */
	wdcmdclr(cbp);				/* clear out block */
	cbp->c_highdma = (WDBUFPADDR >> 16) & 0xFF;
	cbp->c_middma =  (WDBUFPADDR >> 8) & 0xFF;
	cbp->c_lowdma =  (WDBUFPADDR) & 0xFF;
	cbp->c_op = WDSDP;			/* set drive params */
	cbp->c_errorbits = 0xFF;		/* mark block as valid */
	dpp = (WDP *) pfix(WDS, WDBUFPADDR);
	*dpp = wdparam;				/* to achieve DMA alignment */
	out(WDIO, 1);				/* strobe line to DMA part */
}

/*
 * format the Commodore floppy disk specified. Note that this command can
 * only format an entire disk, unlike the hard disk counterpart.
 */
wdioctl(dev, com, vec)
dev_t dev;
int com;
char *vec;
{
	register int s;

	if (dev != makedev(2, 32) && dev != makedev(2, 48)) {
		u.u_error = ENODEV;
		return;
	}
/*	if (com != FDFORMAT) */ {
		u.u_error = EINVAL;
		return;
	}
	if (! super())
		return;
	lock(wdbuf.b_gate);
	wdbuf.b_flag = BFNTP;
	wdbuf.b_req = FMTCMD;
	wdbuf.b_dev = dev;
	wdbuf.b_bno = 0;
	wdbuf.b_count = 512;
	wdbuf.b_paddr = 0;
	s = sphi();
	dblock(dev, &wdbuf);
	while ((wdbuf.b_flag&BFNTP) != 0)
		sleep((char *)&wdbuf, CVBLKIO, IVBLKIO, SVBLKIO);
	spl(s);
	if ((wdbuf.b_flag&BFERR) != 0)
		u.u_error = wdbuf.b_err ? wdbuf.b_err : EIO;
	unlock(wdbuf.b_gate);
}
wdformat(bp, cbp)
register BUF *bp;
register WDCMD *cbp;
{
	register int i;
	register unsigned char *cp;

	cbp->wdcmdu.wdcmdop.c_opcode = CFFMT;			/* format device */
	cbp->c_highdma =(WDBUFPADDR >> 16);	/* phys segment */
	cbp->c_middma = (WDBUFPADDR >> 8);	/* offset */
	cbp->c_lowdma = 0x00;
	cbp->wdcmdu.wdcmdop.c_lunhiaddr = (drive(bp->b_dev) << 5);
	cbp->c_errorbits = 0xff;		/* make ready */
#if	VERBOSE
	wdcbdump(cbp);
#endif
	out(WDIO, 1);				/* wake up controller */
	return (TRUE);
}
