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
 * Commodore 900 Keyboard-specific driver.
 * NOTE:  this driver must be loaded with the
 *	  appropriate memory-mapped video driver.
 *	  These two drivers together with the `kv.c'
 *	  driver form the whole driver.
 */

#include <coherent.h>
#include <errno.h>
#include <tty.h>
#include <io.h>
#include <kbtab.h>

#define	KBIRQ	8			/* Interrupt level for keyboard */

void	kbintr();
void	kbinit();
void	kbterm();
void	kbintend();
int	v0in();

static	int	kbopenf;
static	int	kbkeypad;		/* Set into keypad mode */
static	int	kbsstate;		/* Shift state */

/*
 * Code to drive the Z8036 interface on the Commodore
 * 900.  This has all the bit defintions.
 */
#define	clearint()	outb(PACS, 0x20)

/* Port definitions - Z8036 #1 */
#define	PADD	0x47			/* Port A Data Direction */
#define	PCDD	0x0D			/* Port C Data Direction */
#define	PADPP	0x45			/* Port A Data Path Polarity */
#define	PCDPP	0x0B			/* Port C Data Path Polarity */
#define	PASIOC	0x49			/* Port A Special I/O Control */
#define	PCSIOC	0x0F			/* Port C Special I/O Control */
#define	PAMS	0x41			/* Port A Mode Specification */
#define	PAHS	0x43			/* Port A Handshake Specification */
#define	PAPPR	0x4B			/* Port A Pattern Polarity */
#define	PAPTR	0x4D			/* Port A Pattern Transition */
#define	PAPMR	0x4F			/* Port A Pattern Mask */
#define	PAIV	0x05			/* Port A Interrupt Vector */
#define	PACS	0x11			/* Port A Command and Status */
#define	MICR	0x01			/* Master Interrupt Control */
#define	MCC	0x03			/* Master Configuration Control */
#define	PADATA	0x1B			/* Port A Data */
#define	PCDATA	0x1F			/* Port C Data */

#define	SETKBD	0x78			/* Set pc3 bit (KBDCLR) */
#define	CLRKBD	0x70			/* Clear pc3 bit (KBDCLR) */

static void
kbinit()
{
	outb(PADD, 0xFF);		/* Input data direction Port A */
	outb(PCDD, (inb(PCDD)&~0xC)|0x4); /* pc2 input/ pc3 output */
	outb(PADPP, 0);			/* Non-inverting all of Port A */
	outb(PCDPP, inb(PCDPP)&~0xC);	/* Non-inverting pc2, pc3 */
	outb(PASIOC, 0);		/* No special I/O on Port A */
	outb(PCSIOC, inb(PCSIOC)&~0xC); /* No special I/O pc2, pc3 */

	outb(PAMS, 0x1A);		/* Single buffer, int. match, OR */
	outb(PAHS, 0);			/* No Handshake */

	outb(PAPPR, 0x80);		/* Pattern polarity: */
	outb(PAPTR, 0x00);		/*  one bit set in 0x80 */
	outb(PAPMR, 0x80);		/*  or pa7 */

	outb(PAIV, KBIRQ);		/* Interrupt vector */
	clearint();
	outb(PACS, 0xC0);		/* Enable interrupts */
	outb(MICR, inb(MICR)|0x80);	/* Master interrupt enable */
	outb(MCC, inb(MCC)|0x04);	/* Enable Port A interrupt */

	/*
	 * This enables the keyboard.
	 * Unfortunately, there is a bit
	 * for IEEE REN here (0x01) as well
	 * which needs to be saved via a
	 * software copy.
	 */
	outb(0x205, 0x02);

	/*
	 * Strobe keyboard bit.
	 */
	outb(PCDATA, CLRKBD);
	outb(PCDATA, SETKBD);
}

static void
kbterm()
{
	outb(0x205, 0);			/* disable keyboard */
	outb(MCC, inb(MCC)&~0x04);	/* Disable Port A int. enable */
}

/*
 * Keyboard load
 * Set up interrupts
 * and initialise Z8036 hardware.
 */
kbload()
{
	setivec(KBIRQ, kbintr);
	kbinit();
}

/*
 * Keyboard unload
 * Make device quiescent and turn off
 * interrupts.
 */
kbuload()
{
	kbterm();
	clrivec(KBIRQ);
}

kbopen(dev, m)
dev_t dev;
int m;
{
	++kbopenf;
}

kbclose(dev)
dev_t dev;
{
	if (--kbopenf < 0)
		kbopenf = 0;
}

kbioctl(dev, com, p)
{
/*
	printf("MCC=%x\n", inb(MCC));
	printf("MICR=%x\n", inb(MICR));
	printf("PACS=%x\n", inb(PACS));
*/
}

static void
kbintr(dev)
dev_t dev;
{
	register KEY *kp;
	register int f;
	register int c;
	register int s;
	register int u;

	/*
	 * Port A has KD0-KD6 bits in it.  KD8 is
	 * the interrupt pattern bit and should be
	 * masked off.  KD7 is the keyup/keydown bit
	 * and it in Port C.
	 */
	s = sphi();
	u = inb(PCDATA)&0x04;		/* Keyup/Keydown */
	c = inb(PADATA)&~0x80;
	spl(s);
#if 0
	printf("ss=%d, Key %d, %s\n", kbsstate, c, u ? "up" : "down");
#endif
	kp = &ktab[c];
	/*
	 * Shift keys are the only ones that
	 * use keyup and keydown differentiation.
	 */
	f = kp->k_flag;
	if (f & KINV) {
		kbintend();
		return;
	}
	if (f & (KSHIFT|KLOCK)) {
		if (f & KLOCK) {
			if (u)
				kbsstate ^= kp->k_control;
		} else if (u)
			kbsstate &= ~kp->k_control;
		else
			kbsstate |= kp->k_control;
		kbintend();
		return;
	}
	if (u) {
		kbintend();
		return;
	}
	/*
	 * This check is here to ensure that
	 * shift states map properly (in case
	 * any keys lock).
	 */
	if (kbopenf == 0) {
		kbintend();
		return;
	}
	if (kbkeypad!=0 && (f&KP)!=0) {
		c = kp->k_control;
	} else if (kbsstate != 0) {
		register int shift = 0x00;

		if ((kbsstate & (SS1|SS2)) != 0)
			shift = 0x01;
		if (kbsstate & (SCL|SNL)) {
			if ((f&KCAP)!=0 && (kbsstate&SCL)!=0
			 || (f&KNL)!=0  && (kbsstate&SNL)!=0)
				shift ^= 0x01;
		}
		if (kbsstate & SCT) {
			if (f & KC)
				c = kp->k_control;
			else {
				kbintend();
				return;
			}
		} else if (shift)
			c = kp->k_upper;
		else
			c = kp->k_lower;
		if (kbsstate & SAL)
			c |= 0x80;
	} else
		c = kp->k_lower;
	v0in(c);
	if (f & KDUP)
		v0in(c);
	/*
	 * Send shift state for all
	 * Function keys
	 */
	if (c>=FBASE && c<=FEND) {
		c = CSHIFT;
		if ((kbsstate & (SS1|SS2)) != 0)
			c |= SUPPER;
		if (kbsstate & SAL)
			c |= SALT;
		if (kbsstate & SCT)
			c |= SCTRL;
		v0in(c);
	}
	kbintend();
}

static void
kbintend()
{
	register int s;

	s = sphi();
	clearint();
	/*
	 * Strobe keyboard.
	 */
	outb(PCDATA, CLRKBD);
	outb(PCDATA, SETKBD);
	spl(s);
}

/*
 * Set/reset keypad mode.
 * Non-zero `f' implies keypad mode in
 * which certain keypad keys return
 * special sequences rather than numbers, etc.
 */
kbkpmode(f)
int f;
{
	kbkeypad = f;
}
