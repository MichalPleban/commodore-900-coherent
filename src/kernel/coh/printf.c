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
 * Print formatted.
 */
#include <coherent.h>

/*
 * For indirecting and incrementing argument pointer.
 */
#define ind(p, t)	(*((t *) p))
#define inc(t1, t2)	((sizeof(t2 *)+sizeof(t1)-1) / sizeof(t1))

/*
 * Table for printing out digits.
 */
char digtab[] ={
	'0',	'1',	'2',	'3',	'4',	'5',	'6',	'7',
	'8',	'9',	'A',	'B',	'C',	'D',	'E',	'F'
};

/*
 * A simple printf.
 */
printf(fp, a1)
register char *fp;
{
	int i;
	char *cp;
	register int c;
	register int *ap;

	ap = (char *)&a1;
	for (;;) {
		while ((c=*fp++) != '%') {
			if (c == '\0')
				return;
			putchar(c);
		}
		switch (c=*fp++) {
		case 'c':
			putchar(*ap++);
			continue;
		case 'd':
			if ((i=*ap++) < 0) {
				i = -i;
				putchar('-');
			}
			printn(i, 10);
			continue;
		case 'o':
			printn(*ap++, 8);
			continue;
		case 'r':
			ap = *((int **) ap);
			fp = ind(ap, char *);
			ap += inc(int, char *);
			continue;
		case 's':
			cp = ind(ap, char *);
			ap += inc(int, char *);
			while ((c=*cp++) != '\0')
				putchar(c);
			continue;
		case 'x':
			printn(*ap++, 16);
			continue;
		case 'u':
			printn(*ap++, 10);
			continue;
		case 'p':
			if (sizeof(char *) > sizeof(int)) {
				printn(*ap++, 16);
				putchar(':');
			}
			printn(*ap++, 16);
			continue;
		default:
			putchar(c);
			continue;
		}
	}
}

/*
 * Print out the unsigned `v' in the base `b'.
 */
printn(v, b)
unsigned v;
{
	register int n;

	if ((n=v/b) != 0)
		printn(n, b);
	putchar(digtab[v%b]);
}
