/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include	<stdio.h>


main( argc, argv)
char	*argv[];
{
	char	lbuf[128];

	if (argc > 1)
		convert( argv[1]);
	else while (gets( lbuf) != NULL)
		if (lbuf[0])
			convert( lbuf);
}


convert( lbuf)
char	*lbuf;
{
	register	c,
			base;
	long	l;
	int	negative;

	base = 10;
	negative = 0;

	while (c = *lbuf++) {
		switch (c) {
		case '-':
			++negative;
		case '\t':
		case ' ':
			continue;
		case '#':
			base = 16;
			break;
		case '0':
			base = 8;
			if (*lbuf=='x' || *lbuf=='X') {
				base = 16;
				++lbuf;
			}
			break;
		case '$':
			base = 2;
			break;
		case '\'':
			print( (long)*lbuf);
			return;
		default:
			--lbuf;
			break;
		}
		break;
	}

	l = 0;
	while (c = *lbuf++) {
		if (c==' ' || c=='\t')
			continue;
		if (c>='A' && c<='F' || c>='a' && c<='f')
			c = (c&~040) - 'A' + '9' + 1;
		if ((unsigned)(c-='0') >= base) {
			fprintf( stderr, "bad digit\n");
			return;
		}
		l = l*base + c;
	}

	print( negative? -l: l);
}


print( l)
long	l;
{
	register	c;
	long		m;

	/*
	 * hex, decimal, octal
	 */
	printf( "#%X %D 0%O $", l, l, l);
	/*
	 * binary
	 */
	for (c=32, m=l; m; --c, m<<=1)
		if (m < 0)
			break;
	do {
		printf( m<0? "1": "0");
		m <<= 1;
	} while (--c);
	/*
	 * char
	 */
	printf( " '");
	if ((c=(char)l) < 0) {
		printf( "~");
		c &= 0177;
	}
	if (c<' ' || c=='\177') {
		printf( "^");
		if (c == '\177')
			c = '?';
		else
			c += '@';
	}
	printf( "%c'\n", c);
}
