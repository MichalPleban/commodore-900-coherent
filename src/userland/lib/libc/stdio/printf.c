/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Standard I/O library printf/fprintf/sprintf.
 * Non-portable things:
 * 1) alignment of arguments is assumed to be completely contiguous.
 * 2) the smallest number is assumed to negate to itself.
 * 3) All of long, int, char*, and double are assumed to
 *    be held in an exact number of ints.
 */

#include <stdio.h>
#include <mdata.h>
#include <ctype.h>

union	alltypes {
	char	c;
	int	i;
	unsigned u;
	long	l;
	double	d;
	char	*s;
};

#define	bump(p,s)	(p+=sizeof(s)/sizeof(int))

char	*printi();
char	*printl();
char	*_dtefg();

static	char	null[] = "{NULL}";

/*
 * formatted print on standard output
 */
printf(args)
union alltypes args;
{
	xprintf(stdout, &args);
}

/*
 * Formatted print on given file
 */
fprintf(fp, args)
FILE *fp;
union alltypes args;
{
	xprintf(fp, &args);
}

/*
 * Formatted print into given string.
 * Handcrafted file structure created for putc.
 */
char *
sprintf(sp, args)
char *sp;
union alltypes args;
{
	FILE	file;

	_stropen(sp, -MAXINT-1, &file);
	xprintf(&file, &args);
	putc('\0', &file);
	return (sp);
}

static
xprintf(fp, argp)
FILE *fp;
union alltypes *argp;
{
	register char *cbp;
	int *iap;
	register c;
	char *s;
	char *cbs;
	char adj, pad;
	int prec;
	int fwidth;
	int pwidth;
	register char *fmt;
	union alltypes elem;
	char cbuf[64];

	iap = (int *)argp;
	fmt = *(char **)iap;
	bump(iap, char*);
	for (;;) {
		while((c = *fmt++) != '%') {
			if(c == '\0') {
				return;
			}
			putc(c, fp);
		}
		pad = ' ';
		fwidth = -1;
		prec = -1;
		c = *fmt++;
		if (c == '-') {
			adj = 1;
			c = *fmt++;
		} else
			adj = 0;
		if (c == '0') {
			pad = '0';
			c = *fmt++;
		}
		if (c == '*') {
/* 
 * I made it check for a negative field width and set adj accordingly - 
 * John Thomas
 */
			if ((fwidth = *iap++) < 0) {
				adj = 1;
				fwidth *= -1;
			}
			c = *fmt++;
		} else
			for (fwidth = 0; c>='0' && c<='9'; c = *fmt++)
				fwidth = fwidth*10 + c-'0';
		if (c == '.') {
			c = *fmt++;
			if (c == '*') {
				prec = *iap++;
				c = *fmt++;
			} else
				for (prec=0; c>='0' && c<='9'; c=*fmt++)
					prec = prec*10 + c-'0';
		}
		if (c == 'l') {
			c = *fmt++;
			if (c=='d' || c=='o' || c=='u' || c=='x')
				c = toupper(c);
		}
		cbp = cbs = cbuf;
		switch (c) {

		case 'd':
			elem.i = *iap++;
			if (elem.i < 0) {
				elem.i = -elem.i;
				*cbp++ = '-';
			}
			cbp = printi(cbp, elem.i, 10);
			break;

		case 'u':
			cbp = printi(cbp, *iap++, 10);
			break;
	
		case 'o':
			cbp = printi(cbp, *iap++, 8);
			break;

		case 'x':
			cbp = printi(cbp, *iap++, 16);
			break;

		case 'D':
			elem.l = *(long *)iap;
			bump(iap, long);
			if (elem.l < 0) {
				elem.l = -elem.l;
				*cbp++ = '-';
			}
			cbp = printl(cbp, elem.l, 10);
			break;

		case 'U':
			cbp = printl(cbp, *(long *)iap, 10);
			bump(iap, long);
			break;

		case 'O':
			cbp = printl(cbp, *(long *)iap, 8);
			bump(iap, long);
			break;

		case 'X':
			cbp = printl(cbp, *(long *)iap, 16);
			bump(iap, long);
			break;

		case 'e':
		case 'g':
		case 'f':
			elem.d = *(double *) iap;
			bump(iap, double);
			cbp = _dtefg(c, elem.d, prec, cbp);
			break;

		case 's':
			if ((s = *(char **)iap) == NULL)
				s = null;
			bump(iap, char*);
			/*
			 * Do %s specially so it can be longer.
			 */
			cbp = cbs = s;
			while (*cbp++ != '\0')
				if (prec>=0 && cbp-s>prec)
					break;
			cbp--;
			break;
	
		case 'c':
			elem.c = *iap++;
			*cbp++ = elem.c;
			break;
	
		case 'r':
			xprintf(fp, *(char ***)iap);
			bump(iap, char**);
			break;
	
		default:
			putc(c, fp);
			continue;
		}
		if ((pwidth = fwidth + cbs-cbp) < 0)
			pwidth = 0;
		if (!adj)
			while (pwidth-- != 0)
				putc(pad, fp);
		while (cbs < cbp)
			putc(*cbs++, fp);
		if (adj)
			while (pwidth-- != 0)
				putc(pad, fp);
	}
}

static	char	digits[] = {
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
	'A', 'B', 'C', 'D', 'E', 'F',
};

/*
 * Print an unsigned integer in base b.
 */
static
char *
printi(cp, n, b)
char *cp;
register unsigned n;
{
	register a;
	register char *ep;
	char pbuf[10];

	ep = &pbuf[10];
	*--ep = 0;
	for ( ; a = n/b; n=a)
		*--ep = digits[n%b];
	*--ep = digits[n];
	while (*ep)
		*cp++ = *ep++;
	return (cp);
}

/*
 * Print an unsigned long in base b.
 */
static
char *
printl(cp, n, b)
register char *cp;
unsigned long n;
register b;
{
	char pbuf[13];
	unsigned long a;
	register char *ep;

	ep = &pbuf[13];
	*--ep = '\0';
	for ( ; (a = n/b) != 0; n = a)
		*--ep = digits[n%b];
	*--ep = digits[n];
	while (*ep)
		*cp++ = *ep++;
	return (cp);
}
