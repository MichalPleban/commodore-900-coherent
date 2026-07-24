/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Centronics parallel printer interface driver for Commodore Z8000M
 * (uses the Z8036 chip).
 *
 * April 1984.
 * Improved February 1985 (grr)
 *
 */
#include	<coherent.h>
#include	<drvcon.h>
#include	<errno.h>
#include	<io.h>
#include	<proc.h>
#include	<uproc.h>
#include	<stat.h>

int	lpload();
int	lpuload();
int	lpwrite();
int	lpopen();
int	lpclose();
int	lpintr();
int	nulldev();
int	nonedev();

CON	lpcon =	{
	DFCHR,				/* Flags */
	3,				/* Major index */
	lpopen,				/* Open */
	lpclose,			/* Close */
	nulldev,			/* Block */
	nonedev,			/* Read */
	lpwrite,			/* Write */
	nonedev,			/* Ioctl */
	nulldev,			/* Powerfail */
	nulldev,			/* Timeout */
	lpload,				/* Load */
	lpuload				/* Unload */
};

/*
 * Various ports and masks.  All initialization of the Z8036
 * is done in the mach since there are other devices that
 * use the chip.  If you want to change LPIRQ you must also
 * modify its definition in the mach.
 */
#define	LPPORT		0x1D		/* Printer port */
#define BUSYPORT	0x1F		/* BUSY port */
#define ACKPORT		0x9D		/* Acknowledge port */
#define STROBEPORT	0x9F		/* Strobe port */
#define SETSTROBE	0x70		/* Sets strobe line */
#define CLEARSTROBE	0x78		/* Clears strobe line */
#define ACKMASK		0x02		/* Mask for acknowledge bit */
#define BUSYMASK	0x01		/* Mask for busy bit */
#define PBCS		0x93		/* Z8036 #2 port B cmd & stat reg */
#define CLRIPIUS	0x20		/* Mask to clear IP and IUS */
#define	LPIRQ		0x70		/* Printer interrupt */
#define	MAXBUF		256		/* # of bytes in buffer */
#define	LOLIM		30		/* Low buffer limit for wakeup */

/*
 * Printer database.
 */
struct	lpinfo {
	int	lpcol;			/* Current horizontal position */
	int	lpnbuf;			/* # of bytes in buffer */
	char	*lpin;			/* Input pointer */
	char	*lpout;			/* Output pointer */
	char	lpflag;			/* Flags */
	char	lpbuf[MAXBUF];		/* Buffer */
}	lp;

/* lpflag bits */
#define	LPOPEN	0x01			/* Printer is open */
#define	LPSLEEP	0x02			/* Sleeping on buffer event */
#define	LPRAW	0x04			/* Raw mode */
#define	LPWFI	0x08			/* Waiting for Interrupt */

/*
 * On load, set up the circular
 * buffer pointers in the printer database,
 * then grab the interrupt vector.
 * The data lines are initialised, as follows in md.s:
 *	8036 #1, port b, non-inverting:			output data lines
 *	8036 #1, port c, bit 0, non-inverting:		input busy line
 *	8036 #2, port b, bit 1, inverting:		inpupt acknowledge
 *	8036 #2, port c,  bit 3, non-inverting:		output strobe
 */
lpload()
{
	lp.lpin =  &lp.lpbuf[0];
	lp.lpout = &lp.lpbuf[0];
	lp.lpnbuf = 0;
	setivec(LPIRQ, &lpintr);
}

/*
 * Unload the driver and close up shop
 */
lpuload()
{
	clrivec(LPIRQ);
}

/*
 * The open routine makes sure
 * that only one process can have the
 * printer open at once, and copies the
 * RAW bit of the minor into the local
 * printer flags.
 */
lpopen(dev, mode)
dev_t	dev;
{
	if ((lp.lpflag&LPOPEN) != 0) {
		u.u_error = EDBUSY;
		return;
	}
	lp.lpin = &lp.lpbuf[0];
	lp.lpout = &lp.lpbuf[0];
	lp.lpnbuf = 0;
	lp.lpcol = 0;
	if ((inb(BUSYPORT) & BUSYMASK) != 0) {
		u.u_error = EDATTN;
		return;
	}
	lp.lpflag = LPOPEN;
	if ((minor(dev)&0x01) != 0)
		lp.lpflag |= LPRAW;
	if ((lp.lpflag & LPRAW) ==0)
		lpchar('\r');
}

/*
 * The close routine drains the
 * circular buffer in the per printer
 * database, and then marks the device
 * as no longer open.
 */
lpclose(dev)
dev_t	dev;
{
	register int	s;

	if ((lp.lpflag & LPRAW) == 0)
		lpchar('\r');
	s = sphi();
	while (lp.lpnbuf > 0) {
		lp.lpflag |= LPSLEEP;
		sleep((char *)&lp, 0, 0, 0);
	}
	spl(s);
	lp.lpflag = 0;
}

/*
 * The write routine copies the
 * characters from the user buffer to
 * the printer buffer, expanding tabs and
 * keeping track of the current horizontal
 * position of the print head.
 */
lpwrite(dev, iop)
dev_t	dev;
IO	*iop;
{
	register int	c;
	register int	s;

	while ((c=iogetc(iop)) >= 0) {
		if ((lp.lpflag&LPRAW) == 0) {
			switch (c) {
			case '\t':
				do {
					lpchar(' ');
				} while ((++lp.lpcol&07) != 0);
				continue;
	
			case '\n':
				lpchar('\r');
			case '\r':
			case '\f':
				lp.lpcol = 0;
				break;
	
			case '\b':
				--lp.lpcol;
				break;
	
			default:
				++lp.lpcol;
			}
		}
		lpchar(c);
		if (SELF->p_ssig!=0 && nondsig()) {
			u.u_error = EINTR;
			break;
		}
	}
	s = sphi();
	lpstart();
	spl(s);
}

/*
 * Put a character into the
 * printer buffer. Make sure that there is
 * room to do so, and wait for the interrupt
 * driven part to make more room if this is
 * indeed necessary.
 */
lpchar(c)
{
	register int	s;

	s = sphi();
	while (lp.lpnbuf >= MAXBUF) {
		lpstart();
		lp.lpflag |= LPSLEEP;
		sleep((char *)&lp, 0, 0, 0);
	}
	++lp.lpnbuf;
	if (lp.lpin == &lp.lpbuf[MAXBUF])
		lp.lpin = &lp.lpbuf[0];
	*lp.lpin++ = c;
	spl(s);
}

/*
 * Start up the printer, if it
 * is not already printing.
 * Note: Assumes that if printer is busy we will get an ack when it 
 *       is made ready again. (A questionable assumption...).
 * Always called with printer interrupts
 * disabled.
 */
lpstart()
{
	if ((lp.lpflag & LPWFI) == 0) {
		if (lp.lpnbuf > 0) {
			if ((inb(BUSYPORT) & BUSYMASK) == 0) {
				--lp.lpnbuf;
				if (lp.lpout == &lp.lpbuf[MAXBUF])
					lp.lpout = &lp.lpbuf[0];
				outb(LPPORT, *lp.lpout++);    /* Load char on output lines */
				outb(STROBEPORT, SETSTROBE);  /* Strobe it! */
				outb(STROBEPORT, CLEARSTROBE);	/* UnStrobe it! */
			}
			lp.lpflag |= LPWFI;
		} else
			lp.lpflag &= ~LPWFI;
	}
}

/*
 * Interrupt routine. Poke the printer
 * to get it going. Wake up the process level
 * half of the driver, if necessary.
 */
lpintr()
{
	register int	s;

	s = sphi();
	outb(PBCS, CLRIPIUS);	/* Clear Interrupt */
	outb(ACKPORT, inb(ACKPORT) & ~ACKMASK); /* Clear acknowledge bit */
	if ((lp.lpflag & LPWFI) != 0) {
		lp.lpflag &= ~LPWFI;
		lpstart();
		if ((lp.lpflag&LPSLEEP)!=0 && lp.lpnbuf<LOLIM) {
			lp.lpflag &= ~LPSLEEP;
			wakeup((char *)&lp);
		}
	}
	spl(s);
}
