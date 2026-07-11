/*
 * Nroff/Troff.
 * TTY driver.
 */
#include <stdio.h>
#include <ctype.h>
#include "roff.h"
#include "code.h"
#include "char.h"
#include "env.h"
#include "font.h"

/*
 * Device parameters.
 */
int	ntroff	=	NROFF;		/* Programme is NROFF type */
long	sinmul	=	120;		/* Multiplier for inch */
long	sindiv	=	1;		/* Divisor for inch */
long	semmul	=	3;		/* Multiplier for em space */
long	semdiv	=	5;		/* Divisor for em space */
long	senmul	=	3;		/* Multiplier for en space */
long	sendiv	=	5;		/* Divisor for en space */
long	snrmul	=	0;		/* Narrow space (mul) */
long	snrdiv	=	1;		/* Narrow space (div) */
long	sdwmul	=	3;		/* Digit width (mul) */
long	sdwdiv	=	5;		/* Digit width (div) */
long	swdmul	=	1;		/* Multiplier for width tables */
long	swddiv	=	20;		/* Divisor for width tables */
long	shrmul	=	12;		/* Horizontal resolution (mul) */
long	shrdiv	=	1;		/* Horizontal resolution (div) */
long	svrmul	=	20;		/* Vertical resolution (mul) */
long	svrdiv	=	1;		/* Vertical resolution (div) */

/*
 * For mapping user fonts to real fonts.
 */
FTB fontab[] ={
	'R',  '\0', TTTY, 0, 0,
	'I',  '\0', TTTY, 0, 1,
	'B',  '\0', TTTY, 1, 0,
	'\0'
};

/*
 * Table to convert from the internal character set to ASCII.
 */
char intasc[] ={
	  0,  '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',
	'9',  'A',  'B',  'C',  'D',  'E',  'F',  'G',  'H',  'I',
	'J',  'K',  'L',  'M',  'N',  'O',  'P',  'Q',  'R',  'S',
	'T',  'U',  'V',  'W',  'X',  'Y',  'Z',  'a',  'b',  'c',
	'd',  'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  'm',
	'n',  'o',  'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
	'x',  'y',  'z',  '!',  '"',  '#',  '$',  '%',  '&',  '(',
	')',  '*',  '+',  ',',  '-',  '.',  '/',  ':',  ';',  '<',
	'=',  '>',  '?',  '@',  '[', '\\',  ']',  '^',  '_',  '{',
	'|',  '}',  '~',  '`', '\'', '\'',  '`',  '^',  '-'
};

/*
 * Width table.
 */
char widtab[] ={
	  0,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	 12,   12,   12,   12,   12,   12,   12,   12,   12
};

/*
 * Set up the non constant parameters that are dependent on a
 * particular device.  Namely pointsize and font.
 */
devparm()
{
}

/*
 * Given a font, consisting of the font number and flags indicating
 * whether the font will be in bold or italics, set the new font
 * to the one given.
 */
devfont(font, bold, ital)
{
	fonwidt = widtab;
	defwidt = widtab;
	newfont = TTTY;
	newbold = bold;
	newital = ital;
}

/*
 * Change the pointsize to the one specified.
 */
devpsze(n)
{
	newpsz = unit(SMINCH, 6*SDINCH);
}

/*
 * Change the vertical spacing.
 */
devvlsp(n)
{
	vls = unit(SMINCH, 6*SDINCH);
}

/*
 * Given a pointer to a buffer containing stream directives
 * and a pointer to the end of the buffer, print the buffer
 * out.
 */
flushl(buffer, bufend)
CODE *buffer;
CODE *bufend;
{
	static int	hpos, hres, vres, font, bold, ital;
	register CODE	*cp;
	register int	n;

	for (cp=buffer; cp<bufend; cp++) {
		switch (cp->c_code) {
		case DNULL:
		case DHYPH:
			continue;
		case DHMOV:
		case DPADC:
			hres += cp->c_iarg;
			if ((hpos+=cp->c_iarg)<0) {
				hres -= hpos;
				hpos = 0;
			}
			continue;
		case DVMOV:
			vres += cp->c_iarg;
			continue;
		case DTYPE:
			bold = cp->c_car1;
			ital = cp->c_car2;
			continue;
		case DFONT:
			font = cp->c_iarg;
			continue;
		case DPSZE:
			continue;
		case DSPAR:
			hpos = hres = 0;
			vres += cp->c_iarg;
			if (vres >= 0) {
				n = (vres+10) / 20;
				vres -= n*20;
				while (n--)
					putchar('\n');
			} else {
				putchar('\r');
				n = (-vres+9)/20;
				vres += n*20;
				while (n--)
					printf("\0337");
			}
			continue;
		default:
			if (vres >= 0) {
				vres += 5;
				n = (vres) / 20;
				while (n--)
					printf("\033B");
				if (vres%20/10)
					printf("\0339");
			} else {
				vres -= 5;
				n = (-vres)/20;
				while (n--)
					printf("\0337");
				if (-vres%20/10)
					printf("\0338");
			}
			vres = 0;
			if (hres >= 0) {
				n = (hres+6) / 12;
				hres -= n*12;
				while (n--)
					putchar(' ');
			} else {
				n = (-hres+5)/12;
				hres += n*12;
				while (n--)
					putchar('\b');
			}
			if (cp->c_code==DHYPC)
				n = CMINUS;
			else
				n = cp->c_code;
			if (n<=0 || n>=sizeof intasc)
				panic("Bad directive %d", n);
			n = intasc[n];
			if (ital || bold)
				if (isascii(n))
					if (isupper(n)||islower(n)||isdigit(n))
						if (ital)
							printf("_\b");
						else	/* bold */
							printf("%c\b", n);
			putchar(n);
			hres += cp->c_move-12;
			hpos += cp->c_move;
		}
	}
}
