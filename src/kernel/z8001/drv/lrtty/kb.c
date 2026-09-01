/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
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

/*
 * Translate the nav and function keys to the same MicroEMACS bytes the
 * GUI terminal delivers (hr/zview/zvpump.c keymap, hr/zterm/hrpump.c),
 * so the console, zterm, me(1) and zedit all agree on what a key does:
 *
 *	arrows        ^P ^N ^B ^F
 *	Ctrl+arrows   ESC v, ^V, ^A, ^E  (= PgUp, PgDn, Home, End;
 *	              PgUp is M-v, NOT ^Z -- ^Z is me's save-and-exit)
 *	Shift+arrows  ESC < ESC > ESC b ESC f (top/end, word back/fwd)
 *	F1 ^@ (set mark)		F2 ^X^S (save)
 *	F3 ^X^V (visit a file)		F4 ^R (replace/reverse search)
 *	F5 ESC w (copy region)		F6 ^W (kill region)
 *	F7 ^S (search)			F8 ^K^K (kill line)
 *	F9 ^Y (yank)			F10 ^X^C (quit)
 *	Clear/Home (F12)  ESC < (top of buffer)
 *	Pop/Push   (F13)  ^X n  (next me window; SHIFTED: ^X p, prev)
 *	Screen/Print (F14) ^L   (redraw)
 *	Stop/Cont  (F15)  ^G    (abort)
 * plus a full SHIFT layer (save as, new/use buffer, swap mark, region
 * case, search back, delete word, begin/execute macro, end of buffer,
 * prev window, shrink window) and a full CONTROL layer (word case, set
 * name, revert, split/one window, delete word back, end macro,
 * quickexit, kill buffer, enlarge window, position, list buffers) --
 * the switch bodies below name them all, and the complete map with
 * every consumer is KEYBOARD.md at the source root.
 *
 * Only the modifiers matter to the gate (caps/num lock never block a
 * translation); combinations not listed, Alt+anything, Help (F11), the
 * W keys and the keypad keep the historical raw code + CSHIFT trailer.  In cooked mode the control
 * bytes act as themselves (F10's ^C interrupts, F7's ^S stops output
 * until ^Q), exactly as they do on a zterm pty.  The old raw arrow
 * bytes 0x80-0x83 no longer reach the queue (cooked mode mangled them
 * anyway: ttin's 7-bit mask turned CDOWN into the interrupt character).
 * Returns nonzero when the key was translated and queued.
 */
static int
kbmap(c)
register int c;
{
	register int mod = kbsstate & (SS1|SS2|SCT|SAL);
	register int sh = mod & (SS1|SS2);

	switch (c) {
	case CUP:	if (mod & SCT) { v0in(033);  v0in('v'); }
			else if (sh) { v0in(033);  v0in('<'); }
			else v0in(020);
			return 1;
	case CDOWN:	if (mod & SCT) v0in(026);
			else if (sh) { v0in(033);  v0in('>'); }
			else v0in(016);
			return 1;
	case CLEFT:	if (mod & SCT) v0in(001);
			else if (sh) { v0in(033);  v0in('b'); }
			else v0in(002);
			return 1;
	case CRIGHT:	if (mod & SCT) v0in(005);
			else if (sh) { v0in(033);  v0in('f'); }
			else v0in(006);
			return 1;
	}
	if (mod & SAL)
		return 0;
	if (mod & SCT) {
		switch (c) {			/* the Ctrl layer (KEYBOARD.md) */
		case F1:	v0in(033);  v0in('c');	break;
		case F2:	v0in(030);  v0in(006);	break;
		case F3:	v0in(030);  v0in(022);	break;
		case F4:	v0in(033);  v0in('u');	break;
		case F5:	v0in(030);  v0in('2');	break;
		case F6:	v0in(030);  v0in('1');	break;
		case F7:	v0in(033);  v0in('l');	break;
		case F8:	v0in(033);  v0in(010);	break;
		case F9:	v0in(030);  v0in(')');	break;
		case F10:	v0in(032);		break;
		case F12:	v0in(030);  v0in('k');	break;
		case F13:	v0in(030);  v0in('z');	break;
		case F14:	v0in(030);  v0in('=');	break;
		case F15:	v0in(030);  v0in(002);	break;
		default:	return 0;
		}
		return 1;
	}
	if (sh) {
		switch (c) {			/* the Shift layer (KEYBOARD.md) */
		case F1:	v0in(033);  v0in('!');	break;
		case F2:	v0in(030);  v0in(027);	break;
		case F3:	v0in(030);  v0in('b');	break;
		case F4:	v0in(030);  v0in(030);	break;
		case F5:	v0in(030);  v0in(025);	break;
		case F6:	v0in(030);  v0in(014);	break;
		case F7:	v0in(022);		break;
		case F8:	v0in(033);  v0in('d');	break;
		case F9:	v0in(030);  v0in('(');	break;
		case F12:	v0in(033);  v0in('>');	break;
		case F13:	v0in(030);  v0in('p');	break;
		case F14:	v0in(030);  v0in(032);	break;
		case F15:	v0in(030);  v0in('e');	break;
		default:	return 0;
		}
		return 1;
	}
	switch (c) {
	case F1:	v0in(0);		break;
	case F2:	v0in(030);  v0in(023);	break;
	case F3:	v0in(030);  v0in(026);	break;
	case F4:	v0in(022);		break;
	case F5:	v0in(033);  v0in('w');	break;
	case F6:	v0in(027);		break;
	case F7:	v0in(023);		break;
	case F8:	v0in(013);  v0in(013);	break;
	case F9:	v0in(031);		break;
	case F10:	v0in(030);  v0in(003);	break;
	case F12:	v0in(033);  v0in('<');	break;
	case F13:	v0in(030);  v0in('n');	break;
	case F14:	v0in(014);		break;
	case F15:	v0in(007);		break;
	default:	return 0;
	}
	return 1;
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
	/*
	 * KD0-KD6 give a scan code up to 0x7F, but ktab[] stops well short
	 * of that.  Codes past the end of the table would pick up whatever
	 * driver data follows it as key flags, so drop them here.
	 */
	if (c >= nktab) {
		kbintend();
		return;
	}
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
	if (kbmap(c) == 0) {
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
