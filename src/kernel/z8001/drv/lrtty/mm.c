/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.  Commodore Z-8000 memory-mapped video using the
 * Commodore/MOS Technology "6845" (a. k. a. the 6545-1).
 *
 * The terminal emulation is as close as is
 * reasonable to the Heath H-19 (Zenith Z-19) terminal.
 *
 * The CRT controller is wired as follows:
 *	bit 0 of the memory address determines whether
 * character or attribute (0 is character, 1 attribute).
 * Thus each character position takes 2 bytes of display
 * memory.
 *	14 bits are given to the CRT controller.
 * For the monochrome display which has only 2048 character
 * positions (11 bits) the upper 3 address bits are replicated
 * into the controller as 0.  Therefore no masks need be used
 * on ascending (circular) addresses.
 */
/* Special hacking by rec 85.VI.06 */
/* Cannot emulate z19 very closely because CBU keyboard driver doesn't */
#define istty kvtty
#define ismmfunc(c)	/* ismmfunc(c) */
#define initkeys()	/* initkeys() */

#include <coherent.h>

#if	0
#include <i8086.h>
#endif

#include <errno.h>
#include <uproc.h>
#include <stat.h>
#include <tty.h>
#include <signal.h>
#include <timeout.h>

#define	setcurse()	mmcrt(14,((mmoffset)>>1)&mmmask)
#define	position(r,c)	(2*((r)*NCOL+(c)))
#define	reposition()	(mmoffset=position(mmrow,mmcol))
#define	setctype(x)	mmcrt(10,x)
#define	setpage()	mmcrt(12,0)           /* Set display page 0 */
#define checkspec()	(mmrow == SPECIAL-1)  /* Cursor on line 25? */

#define TRUE 	1
#define FALSE 	0

#define	NROW	24			/* Maximum rows */
#define	NCOL	80			/* Columns on screen */
#define TABLIM	72			/* Tab limit (0-relative) */
#define SPECIAL 25			/* Line 25 is special */
#undef	ESC
#define	ESC	033
#define CAN	030
#define	DEL	0177
#define NUL	0

#define	ATT	0x0700			/* Ordinary Video */
#define	INTENSE	0x0800			/* Intense video */
#define	BLINK	0x8000			/* Blinking video */
#define REVERSE 0x7000			/* Reverse video */
#define UNDERL  0x0100	  		/* Underline video (mono board) */

#if	0
/* For beeper */
#define	TIMCTL	0x43			/* Timer control port */
#define	TIMCNT	0x42			/* Counter timer port */
#define	PORTB	0x61			/* Port containing speaker enable */

#define	FREQ	((int)(1193181L/440))	/* Counter for 440 Hz. tone */

unsigned mmbase = 0;		/* Segment of memory */
unsigned mmport = 0;		/* Port address of 6845 */
#endif
unsigned mmmask = 0;		/* Mask for page+offset */

#define	STANDALONE 1		/*				   JFF */
#define	MMSEG	0x3a		/* screen memory segment 	   JFF */
#define	MMADDR	0x370000L	/* screen memory base address	   JFF */
char *pfix();			/* map memory base address to char JFF */
static char *mmbase = 0;	/* pointer to screen memory base   JFF */

char mminsmode = 0;		/* Insert mode */
static unsigned mmoffset = 0;	/* Working offset saves calculation */
static char mmrow = 0;		/* Row on screen */
static char mmcol = 0;		/* Column on screen */
static char mmsrow = 0;		/* Saved row */
static char mmscol = 0;		/* Saved column */
static char mmtimer = 0;	/* Audible bell sounding */
static char mmescesc = 0;	/* Escape escape */
static char mmspec = 0;		/* Special line (25) enabled */
static char mmwrap = TRUE;	/* End-of-line wrap mode */
static char mmlfcr = 0;		/* Auto-CR after LF */
static char mmcrlf = 0;		/* Auto-LF after CR */
static unsigned mmattr = 0;	/* Current attribute byte */
static int mmncurse = 0;	/* Normal (underline) cursor */
static int mmbcurse = 0;	/* Block cursor */
static char mmblank = 0;	/* Blank video on scroll - REC */
int (*mmfunc)() = 0;		/* Output function */
int mmbeepoff();
int mmput();
int mmescape();
int mmsetrow();
int mmsetcol();
int mmsetmode();
int mmresetmode();
int mmstart();

/*
 * Initialise memory-mapped video.
 * Have to enable sense switches to
 * find out monitor style selected on
 * the switches.
 */
mmload()
{
#ifdef STANDALONE		/* Let the rom do it otherwise */
	register unsigned char i;
	register char *ip;
	static char col80x25[] = {	/* Colour card settings 80x25 */
		0x71, 0x50, 0x5A, 0x0A, 0x1F, 0x06,
		0x19, 0x1c, 0x02, 0x07
	};
	static char mono[] = {		/* Monochrome settings 80x25 */
		0x61, 0x50, 0x52, 0x0F, 0x19, 0x06,
		0x19, 0x19, 0x02, 0x0D
	};
#endif

	if (mmfunc != NULL)
		return;
#if	0
	switch (int11() & 0x30) {
#endif
	switch (0x30) {						/* JFF */

	case 0x30:			/* Monochrome card */
#if	0
		mmbase = 0xB000;
		mmport = 0x3B4;
#else
		mmmask = 0xFFF;
		mmbase = pfix(MMSEG, MMADDR); /* fix screen memory base  JFF */
#endif
		mmncurse = (0xB<<8)|0xC;  /* normal (underline) cursor */
                mmbcurse = (0x1<<8)|0xC;  /* block cursor */
		mmblank = 0;		  /* No blanking - REC */
#ifdef STANDALONE
		ip = mono;
#endif
		break;

	case 0x20:		      /* Colour card 80x25 */
#if	0
		mmbase = 0xB800;
		mmport = 0x3D4;
#endif
		mmmask = 0x3FFF;
		mmncurse = (6<<8)|7;  /* normal (underline) cursor */
		mmbcurse = (1<<8)|7;  /* block cursor */
		mmblank = 1;	      /* Blank video - REC */
#ifdef STANDALONE
		ip = col80x25;
#endif
		break;

	default:
		/* Cannot print error here */
		panic("");
	}
	/*
	 * Set up initial attributes of the
	 * 6845 CRT controller.
	 */
	mmvonn();			/* Video on, high-res */
#ifdef STANDALONE
	for (i=0; i<10; i+=2) {
		register int mode;

		mode = *ip++<<8;
		mode |= *ip++;
		mmcrt(i, mode);
	}
#endif
	mmfunc = mmput;
	mmattr = ATT;
	setctype(mmncurse);	/* set normal cursor */
	setpage();
	mmclear();
	setcurse();
}

/*
 * Start the output stream.
 * Called from `ttwrite' and `isrint' routines.
 * This is probably the most grossly
 * inefficient way to do memory mapped i/O.
 */

mmstart(tp)
register TTY *tp;
{
	register int c;

	while ((c = ttout(tp)) >= 0)
		(*mmfunc)(c);
}

/*
 * Turn the speaker off from the
 * previously begun beep.
 */
mmbeepoff(save)
int save;
{
#if	0
	outb(PORTB, save);
	mmtimer = 0;
#endif
}

/*
 * Put a character to the memory-mapped
 * display, taking into account escape
 * sequences.
 */
mmput(c)
int c;
{
	static TIM tout;
	register unsigned off;
	register unsigned n;

	switch (c) {
	case '\007':		/* Bell */
#if	0
		outb(TIMCTL, 0xB6);	/* Timer 2, lsb, msb, binary */
		outb(TIMCNT, FREQ&0xFF);
	        outb(TIMCNT, FREQ>>8);
		n = inb(PORTB);
		outb(PORTB, n|03);	/* Turn speaker on */
		if (mmtimer == 0) {
		   mmtimer++;
		   timeout(&tout, (HZ/8), mmbeepoff, n);
		}
#endif
		break;

	case '\t':
		if (mmcol >= TABLIM)	     /* at tab limit? */
		   n = 1;		     /* only move one space */
		else
	  	{  n = (mmcol + 8) & ~7;     /* normal tab */
		   n -= mmcol;
		}
		mmstuff(mmoffset, ' '|mmattr, n, mmbase);	/* JFF */
		if ((mmcol += n) == NCOL)   /* end of line? */
		{  if (mmwrap)
		   {  mmcol = 0;
		      if (++mmrow >= NROW)
			 mmscroll(0);
		      reposition();
		   }
		}
		else
	            mmoffset += 2*n;
		break;

	case '\r':
		mmoffset -= 2*mmcol;
		mmcol = 0;

		if (mmcrlf)		  /* auto-LF after CR? */
		   donl();		  /* newline processing */
		break;   

	case '\n':
		donl();			  /* newline */
		if (mmlfcr)		  /* auto-CR after LF? */
		{  mmoffset -= 2*mmcol;
		   mmcol = 0;
		}
		break;

	case '\b':
		if (mmcol) {
			mmcol--;
			mmoffset -= 2;
		}
		break;

	case DEL:
	case NUL:
		break;			  /* ignore these chars */

	case ESC:
		if (!mmescesc)
		{	mmfunc = mmescape;
			break;
		}
		mmescesc = 0;
		/* Fall through */

	default:
		if (mmcol == NCOL)         /* stuck at end of line? */
		   break;		   /* ignore char (wrap wasn't on) */		
		if (mminsmode && mmcol < NCOL-1)
		{  off = position(mmrow, NCOL-1);
		   n = NCOL-1-mmcol;
		   mmcopy(off-2, off, n, 1, mmbase);		/* JFF */
		}
								/* JFF */
		mmstuff(mmoffset, c|mmattr, 1, mmbase);  /* up goes the char */
		if (mmcol == NCOL-1)  /* at end of line? */
		{  if (mmwrap)      /* wrap mode? */
		   {  mmoffset -= 2*mmcol;  /* carriage return */
		      mmcol = 0;
		      donl();               /* newline or scroll */
		   }
		   else
		      mmcol++;		    /* to make cursor stick */
		}
		else			    /* normal cursor advance */
		{  mmcol++;
		   reposition();
		}
	}
	setcurse();
}

/*
 * Handle special escape sequences for H-19/Z-19 
 */
mmescape(c)
register int c;
{
	register unsigned off;
	register unsigned n;
	extern TTY istty;

	mmfunc = mmput;
	switch (c) {
	case CAN:		/* [<CTRL><X>] cancel escape */
		return;

	case '=':		/* Enter Alternate Keypad Mode */
		ismmfunc(c);
		return;

	case '>':		/* Exit Alternate Keypad Mode */
		ismmfunc(c);
		return;

	case '@':		/* Enter Insert Character Mode */
		mminsmode = TRUE;
		return;

	case 'A':		/* Cursor Up */
		if (checkspec())  	  /* no action if on line 25 */
   		   return;
		if (mmrow) {
			--mmrow;
			mmoffset -= 2*NCOL;
		}
		break;

	case 'B':		/* Cursor Down */
		if (checkspec())  	  /* no action if on line 25 */
   		   return;
		if (mmrow < NROW-1) {
			++mmrow;
			mmoffset += 2*NCOL;
		}
		break;

	case 'C':		/* Cursor Forward */
		if (mmcol < NCOL-1)
		   ++mmcol;		
		reposition();
		break;

	case 'D':		/* Cursor Backward */
		if (mmcol)
		   --mmcol;
		reposition();
		break;

	case 'E':		/* Clear Screen */
		mmclear();
		break;

	case 'H':		/* Cursor Home */
		if (checkspec())
		{  mmoffset = position(SPECIAL-1, 0);  /* start of line 25 */
		   mmrow = SPECIAL-1;
		   mmcol = 0;
		}
		else
		   mmoffset = mmrow = mmcol = 0;
		break;

	case 'I':		/* Reverse Index */
		if (checkspec())  	  /* no action if on line 25 */
   		   return;
		if (--mmrow < 0)
			mmscroll(1);
		else
			mmoffset -= 2*NCOL;
		break;

	case 'J':		/* Erase rest of screen */
		if (!checkspec())	/* if not on line 25 */
		{  n = (NROW-1-mmrow)*NCOL + NCOL-mmcol;
		   mmstuff(mmoffset, ' '|ATT, n, mmbase);	/* JFF */
		   break;
 		}
		/* fall through */
	
	case 'K':		/* Erase to end of line */
		off = position(mmrow, mmcol);
		mmstuff(off, ' '|ATT, NCOL-mmcol, mmbase);	/* JFF */
		break;

	case 'L':		/* Insert line */
		if (checkspec())	/* no action if on line 25 */
		   return;
		if (mmrow < NROW-1) {
			off = position(NROW-1, NCOL-1);
			n = NCOL * (NROW-1-mmrow);
			mmcopy(off-2*NCOL, off, n, 1, mmbase);	/* JFF */
		}
		off = position(mmrow, 0);
		mmstuff(off, ' '|ATT, NCOL, mmbase);		/* JFF */
		break;

	case 'M':		/* Delete line */
		if (checkspec())	/* no action if on line 25 */
		   return;
		if (mmrow < NROW-1) {
			off = position(mmrow, 0);
			n = NCOL * (NROW-1-mmrow);
			mmcopy(off+2*NCOL, off, n, 0, mmbase);	/* JFF */
		}
		off = position(NROW-1, 0);
		mmstuff(off, ' '|ATT, NCOL, mmbase);		/* JFF */
		break;

	case 'N':		/* Delete character */
		if (mmcol < NCOL-1) 
		{	n = NCOL-1-mmcol;
			mmcopy(mmoffset+2, mmoffset, n, 0, mmbase);  /* JFF */
		}
		off = position(mmrow, NCOL-1);
		mmstuff(off, ' '|ATT, 1, mmbase);		/* JFF */
		break;

	case 'O':		/* Leave insert mode */
		mminsmode = FALSE;
		return;

	case 'Y':		/* Direct cursor addressing */
		mmfunc = mmsetrow;
		return;

	case 'b':		/* Erase to beginning of display */
		if (!checkspec())   /* if not on line 25 */
		{  off = position(0, 0);
		   n = mmrow*NCOL + mmcol+1;
		   mmstuff(off, ' '|ATT, n, mmbase);		/* JFF */
		   break;
 		}
		/* fall through */

	case 'o':		/* Erase to beginning of Line */
		off = position(mmrow, 0);
		mmstuff(off, ' '|ATT, mmcol+1, mmbase);		/* JFF */
		break;

	case 'c':		/* Blink mode */
		mmattr |= BLINK;
		return;

	case 'd':		/* Non-blink mode */
		mmattr &= ~BLINK;
		return;

	case 'e':		/* Intense mode */
		mmattr |= INTENSE;
		return;

	case 'f':		/* Non-intense mode */
		mmattr &= ~INTENSE;
		return;

	case 'j':		/* Save cursor position */
		mmsrow = mmrow;
		mmscol = mmcol;
		break;

	case 'k':		/* Restore cursor position */
		mmrow = mmsrow;
		mmcol = mmscol;
		reposition();
		break;

	case 'l':		/* Erase entire line */
		off = position(mmrow, 0);
		mmstuff(off, ' '|ATT, NCOL, mmbase);		/* JFF */
		break;

	case 'n':		/* Report cursor position */
		n = sphi();
		ttin(&istty, ESC);
		ttin(&istty, 'Y');
		ttin(&istty, mmrow+' ');
		ttin(&istty, mmcol+' ');
		spl(n);
		return;

	case 'p':		/* Reverse Video */
		mmattr &= ~0x7700;  /* clear video flags */
		mmattr |= REVERSE;
		return;

	case 'q':		/* Leave reverse video */
		mmattr &= ~0x7700;  /* clear video flags */
		mmattr |= ATT;
		return;

	case 'h':		/* Underline video */
		mmattr &= ~0x7700;  /* clear video flags */
		mmattr |= UNDERL;
		return;

	case 'i':		/* Don't underline video */
		mmattr &= ~0x7700;  /* clear video flags */
		mmattr |= ATT;
		return;

	case 't':		/* Enter keypad shifted mode */
		ismmfunc(c);
		return;

	case 'u':		/* Exit keypad shifted mode */
		ismmfunc(c);
		return;

	case 'v':		/* Wrap mode on (default) */
		mmwrap = TRUE;	
		return;	
	
	case 'w':		/* Wrap mode off */
		/* Note:  This is the H19 "discard at end of line"
		 * mode.  Attempting to delete chars (e.g. with ^H) at the
		 * end of a line when wrap mode is off may cause
		 * the wrong chars on that line to be "visually" erased due
		 * to confusion between chars in the output queue and chars
		 * actually displayed on the screen.  This could be fixed,
		 * but would require considerable code.  Since this situation
		 * should be EXTREMELY rare in practice, it may safely 
		 * be considered to be pathological.
		 */
		mmwrap = FALSE;	
		return;	

	case 'Z':		/* Identify terminal (send "ESC/K") */
		n = sphi();
		ttin(&istty, ESC);	
		ttin(&istty, '/');
		ttin(&istty, 'K');	
		spl(n);
		return;	

	case 'x':		/* Set mode */
		mmfunc = mmsetmode;
		return;

	case 'y':		/* Reset mode */
		mmfunc = mmresetmode;
		return;

	case 'z':		/* Reset to powerup configuration */
		mmattr = ATT;
		mminsmode = FALSE;
		mmwrap = TRUE;
		ismmfunc(c);	     /* reset shift states */
		initkeys();	     /* re-init function keys */
		setctype(mmncurse);  /* set normal (underline) cursor */
		mmclear();           /* clear screen */
		setcurse();          /* home up */
		/* fall through */

	case '2':		/* Disable line 25 */
		disspec();
		return;

	case '1':		/* Enable line 25 */
		mmspec = TRUE;
		return;

	case 'G':		/* Exit graphics mode */
	case '\\':		/* Exit "hold-screen" mode */
		return;		/* nothing to do */

	default:
		mmescesc += 1;
		mmput(ESC);
		mmput(c);
		return;
	}
	setcurse();
}

/*
 * Clear the screen.
 */
mmclear()
{
	if (checkspec())	/* if on line 25 */
	{  mmoffset = position(SPECIAL-1, 0); /* start of line 25 */
	   mmcol = 0;
								/* JFF */
	   mmstuff(mmoffset, ' '|ATT, NCOL, mmbase);  /* clear line 25, only */
	}
	else 			/* not on line 25 */
	{  mmoffset = mmrow = mmcol = 0;
								/* JFF */
   	   mmstuff(0, ' '|ATT, (mmspec ? NROW*NCOL : (NROW+1)*NCOL), mmbase);
	}
}

/*
 * Set the row in direct cursor
 * addressing sequences:
 *	ESC Y line# row#
 */
unsigned int trow, tcol;

mmsetrow(c)
register int c;
{
	mmfunc = mmput;
	if (c == CAN)
		return;
	mmfunc = mmsetcol;
	trow = c - ' ';
}

/*
 * Set the column number in
 * direct cursor addressing.
 */
mmsetcol(c)
register int c;
{
	mmfunc = mmput;
	if (c == CAN)
		return;
	tcol = c - ' ';
	if (trow < (mmspec ? SPECIAL : NROW) && tcol < NCOL) {
		mmrow = trow;
		mmcol = tcol;
		reposition();
		setcurse();
	}
}

/*
 * Implement set mode and reset mode sequences.
 */
mmsetmode(c)
register int c;
{
	mmfunc = mmput;
	switch(c)
	{	case '1':	/* enable line 25 */
		   mmspec = TRUE;
		   break;
		case '4':	/* block cursor */	
		   setctype(mmbcurse);		   
		   break;		   
		case '6':	/* keypad shifted */
		   ismmfunc('t');
		   break;					
		case '7':	/* alternate keypad */
		   ismmfunc('=');
		   break;
		case '8':	/* Auto-LF after CR */
		   mmcrlf = TRUE;		
		   break;
		case '9':	/* Auto-CR after LF */
		   mmlfcr = TRUE;
		   break;
	}
}

mmresetmode(c)
register int c;
{
	mmfunc = mmput;
	switch(c)
	{	case '1':	/* disable line 25 */
		   disspec();
		   break;
		case '4':	/* underscore cursor */	
		   setctype(mmncurse);		   
		   break;		   
		case '6':	/* keypad unshifted */
		   ismmfunc('u');
		   break;					
		case '7':	/* exit alternate keypad */
		   ismmfunc('>');
		   break;
		case '8':	/* no Auto-LF after CR */
		   mmcrlf = FALSE;		
		   break;
		case '9':	/* no Auto-CR after LF */
		   mmlfcr = FALSE;
		   break;
	}
}

/*
 * Scroll the screen. The flag indicates direction.
 * Non-zero indicates downward (normally scrolling
 * is upward).
 */
mmscroll(down)
register int down;
{
	register int off;

	if (mmblank) mmvoff();		/* REC */
	if (down) {
		off = position(0, 0);
		mmrow = 0;
		mmcopy(position(NROW-2, NCOL-1), position(NROW-1, NCOL-1),
		   (NROW-1)*NCOL, 1, mmbase);			/* JFF */
	} else {
		off = position(NROW-1, 0);
		mmrow = NROW-1;
		mmcopy(position(1, 0), position(0, 0), (NROW-1)*NCOL, 0,
		    mmbase);					/* JFF */
	}
	mmstuff(off, ' '|ATT, NCOL, mmbase);			/* JFF */
	reposition();
	if (mmblank) mmvonn();		/* REC */
}

/* Disable line 25 */
disspec()
{   if (mmspec)     /* if already enabled, clear line */
    {  mmstuff(position(SPECIAL-1, 0), ' '|ATT, NCOL, mmbase); 	/* JFF */
       if (checkspec())  /* cursor on line 25? */
          mmrow--;		    /* yes, move off line 25 */
	  mmspec = FALSE;
    }
}

/* Newline processing */
donl()
{  if (checkspec())  	  /* no action if on line 25 */
      return;
   if (++mmrow >= NROW)
      mmscroll(0);
   else
      mmoffset += 2*NCOL;
}

/* Video ioctl */
mmioctl(com, vec)
int com;
int vec;
{
	paddr_t pos;
	int count;
	char *buffer;

	pos = getuwd(vec);
	count = getuwd(vec += 2);
	buffer = getupd(vec += 2);
	if (pos < 0 || pos + count > mmmask)
		u.u_error = EINVAL;
	if (u.u_error)
		return;
	pos += ((paddr_t)mmbase) << 4;
	if (com == TIOVGETB)
		pucopy(pos, buffer, count);
	else {
		if (mmblank) mmvoff();
		upcopy(buffer, pos, count);
		if (mmblank) mmvonn();
	}
}

