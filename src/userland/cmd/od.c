/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Octal dump.
 * The name is a bit of a joke on a
 * machine that uses hexadecimal, but it will
 * stay like this for now.
 * If you use a -DHEX on the command line of
 * the cc command the defaults will all switch to
 * hexadecimal.
 * Revised by Michael Griffin, 7-6-84.  Calls usage() if
 * more than one option char are used.
 */
#include <stdio.h>
#include <mdata.h>


#if HEX
#define	DEFC		'x'
#else
#define	DEFC		'o'
#endif

#define	WIDTH		80
#define	getbyte()	getc(ifp)
#define	getword()	getw(ifp)

FILE	*ifp;
char	buf[BUFSIZ];

main(argc, argv)
char *argv[];
{
	register char *p;
	register c, i;
	long dofs, oofs;
	int bump, fc, mol, nof, nol, pw;
	char afmt[10], pfmt[10];
	int found = 0;

	i = 1;
	fc = DEFC;
	if (i<argc && argv[i][0]=='-') {
		p = &argv[i++][1];
		while ((c = *p++) != 0) {
			switch (c) {

			case 'b':
			case 'c':
			case 'd':
			case 'o':
			case 'x':
				if (found)
					usage();
				fc = c;
				found++;
				break;

			default:
				usage();
			}
		}
	}
	setbuf(stdout, buf);
	ifp = stdin;
	nof = 0;
	if (i < argc) {
		++nof;
		if (*(p=argv[i++]) != '+') {
			nof = 0;
			if ((ifp=fopen(p, "r")) == NULL) {
				fprintf(stderr, "%s: cannot open.\n", p);
				exit(1);
			}
			if (i<argc && argv[i][0]=='+') {
				++i;
				++nof;
			}
		}
	}
	if (nof && i>=argc)
		usage();
	oofs = 0;
	if (i < argc) {
		dofs = 0;
		p = argv[i];
		while ((c=*p++)>='0' && c<='9') {
			c -= '0';
			oofs =  8*oofs + c;
			dofs = 10*dofs + c;
		}
		if (c == '.') {
			oofs = dofs;
			c = *p++;
		}
		if (c == 'b') {
			oofs *= BUFSIZ;
			c = *p++;
		}
		if (c != 0)
			usage();
		fseek(ifp, oofs, 0);
	}
	if (fc=='b' || fc=='c') {
		bump = sizeof(char);
		if (DEFC == 'o')
			pw = (NBCHAR+2) / 3; else
			pw = (NBCHAR+3) / 4;
	} else {
		bump = sizeof(int);
		if (fc != 'x')
			pw = (NBINT+2) / 3; else
			pw = (NBINT+3) / 4;
	}
	mkfmt(pfmt, pw, fc);
	if (DEFC == 'o')
		mkfmt(afmt, (NBLONG+2)/3, 'O'); else
		mkfmt(afmt, (NBLONG+3)/4, 'X');
	for (mol=1; mol < (WIDTH-(NBLONG+2)/3)/(pw+1); mol<<=1)
		;
	mol >>= 1;
	nol = 0;
	for (;;) {
		c = (bump==sizeof(char)) ? getbyte() : getword();
		if (ferror(ifp))	/* partial read */
			clearerr(ifp);	/* treated as good */
		else if (feof(ifp))
			break;
		if (nol >= mol) {
			putchar('\n');
			nol = 0;
		}
		if (nol == 0)
			printf(afmt, oofs);
		if (fc!='c' || !putasc(c))
			printf(pfmt, c);
		++nol;
		oofs += bump;
	}
	if (nol != 0)
		putchar('\n');
}

mkfmt(p, w, c)
register char *p;
register w, c;
{
	if (c >= 'a')
		*p++ = ' ';
	*p++ = '%';
	if (c != 'd')
		*p++ = '0';
	if (w >= 10) {
		*p++ = '1';
		w -= 10;
	}
	*p++ = w+'0';
	if (c=='b' || c=='c')
		c = DEFC;
	*p++ = c;
	*p = 0;
}

putasc(c)
register c;
{
	register n;

	if (c>=' ' && c<='~') {
		putchar(' ');
		putchar(c);
		if (DEFC == 'o')
			n = (NBCHAR+2)/3 - 1; else
			n = (NBCHAR+3)/4 - 1;
		while (n--)
			putchar(' ');
		return (1);
	}
	switch (c) {

	case '\0':
		c = '0';
		break;

	case '\b':
		c = 'b';
		break;

	case '\f':
		c = 'f';
		break;

	case '\n':
		c = 'n';
		break;

	case '\r':
		c = 'r';
		break;

	case '\t':
		c = 't';
		break;

	default:
		return (0);
	}
	putchar(' ');
	putchar('\\');
	putchar(c);
	if (DEFC == 'o')
		n = (NBCHAR+2)/3 - 2; else
		n = (NBCHAR+3)/4 - 2;
	while (n--)
		putchar(' ');
	return (1);
}

usage()
{
	fprintf(stderr, "Usage: od [-bcdox] [file] [ [+] offset[.][b] ]\n");
	exit(1);
}
