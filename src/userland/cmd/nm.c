/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Print the name list of an object
 * file.
 */
#include <stdio.h>
#include "n.out.h"
#include <ar.h>
#include <ctype.h>
#include <canon.h>

int	aflag;
int	dflag;
int	gflag;
int	nflag;
int	oflag;
int	pflag;
int	rflag;
int	uflag;

struct	ldheader	ldh;
struct	ldsym		lds;

char	*fn;
FILE	*fp;

int	title;
int	amemb;
struct	ar_hdr	ahb;
int	stuff;

char	usage[]	= "Usage: nm [-adgnopru] [file ...]\n";

char	*gn[]	= {
	"SI",	"PI",	"BI",
	"SD",	"PD",	"BD",
	" D",	"  ",	"  ",
	" A",   " C",   "??"
};

char	*ln[]	= {
	"si",	"pi",	"bi",
	"sd",	"pd",	"bd",
	" d",	"  ",	"  ",
	" a",   " c",   " ?"
};

struct	fmtab
{
	char	*f_fmt1;
	char	*f_fmt2;
};

char	octb[]	= "       ";
char	octf[]	= "%06O ";
char	hexb[]	= "     ";
char	hexf[]	= "%04X ";
char	lngb[]	= "         ";
char	lngf[]	= "%08X ";
char	segb[]	= "       ";
char	segf[]	= "%06X ";

struct	fmtab	fmtab[]	= {
	octb,	octf,			/* Unused or unknown */
	octb,	octf,			/* 11 */
	lngb,	lngf,			/* VAX */
	segb,	segf,			/* 360 */
	lngb,	lngf,			/* Z-8001 */
	hexb,	hexf,			/* Z-8002 */
	hexb,	hexf,			/* 8086 */
	hexb,	hexf,			/* 8080 and 8085 */
	hexb,	hexf,			/* 6800 */
	hexb,	hexf,			/* 6809 */
	lngb,	lngf			/* 68000 */
};

#define	MTYPES	(sizeof(fmtab)/sizeof(struct fmtab))

main(argc, argv)
char *argv[];
{
	register char *cp;
	register c, i;
	int nf;

	nf = argc-1;
	for (i=1; i<argc; ++i) {
		cp = argv[i];
		if (cp[0] == '-') {
			argv[i] = NULL;
			--nf;
			while ((c = *++cp) != '\0') {
				switch (c) {

				case 'a':
					++aflag;
					break;

				case 'd':
					++dflag;
					break;

				case 'g':
					++gflag;
					break;

				case 'n':
					++nflag;
					break;

				case 'o':
					++oflag;
					++title;
					break;

				case 'p':
					++pflag;
					break;

				case 'r':
					++rflag;
					break;

				case 'u':
					++uflag;
					break;

				default:
					fprintf(stderr, usage);
					exit(1);
				}
			}
		}
	}
	if (nf == 0) {
		fn = "l.out";
		nm();
	} else {
		if (nf > 1)
			title = 1;
		for (i=1; i<argc; ++i)
			if ((fn = argv[i]) != NULL)
				nm();
	}
	exit(0);
}

/*
 * Level I.
 * Open and close files.
 * If an archive arrange that level
 * II is called on each member.
 * The file name is in `fn'.
 */
nm()
{
	register long offs;
	int magic;

	amemb = 0;
	if ((fp = fopen(fn, "r")) == NULL) {
		nmerr("cannot open");
		return;
	}
	magic = getw(fp);
	canshort(magic);
	if (magic == ARMAG) {
		title = amemb = 1;
		while (fread(&ahb, sizeof(ahb), 1, fp) == 1) {
			offs = ftell(fp);
			if (strncmp(ahb.ar_name, "__.SYMDEF", DIRSIZ) != 0)
				nm1();
			cansize(ahb.ar_size);
			fseek(fp, offs+ahb.ar_size, 0);
		}
	} else {
		fseek(fp, (long)0, 0);
		nm1();
	}
	fclose(fp);
}

/*
 * Level II.
 * You are seeked to the start of
 * an `l.out' file.
 * Read in symbols, sort if required
 * and print them out.
 */
nm1()
{
	register struct ldsym *wstp;
	register i, t;
	register struct ldsym *stp, *estp;
	register long offs;
	register char *fmt1, *fmt2;
	int qscmp();

	if (nmhdr(&ldh) != 0)
		return;
	if (ldh.l_ssize[L_SYM] == 0) {
		nmerr("no symbol table");
		return;
	}
	i = ldh.l_ssize[L_SYM];
	if ((ldh.l_flag & LF_32) == 0)
		i = i/(sizeof(lds)-2*sizeof(short)) * sizeof(lds);
	if ((stp = malloc(i)) == NULL) {
		nmerr("too many symbols");
		return;
	}
	if ((t = ldh.l_machine) >= MTYPES)
		t = 0;
	fmt1 = fmtab[t].f_fmt1;
	fmt2 = fmtab[t].f_fmt2;
	offs = ldh.l_tbase;
	for (i=L_SHRI; i<L_SYM; ++i)
		if (i!=L_BSSI && i!=L_BSSD)
			offs += ldh.l_ssize[i];
	fseek(fp, offs-sizeof(ldh), 1);
	wstp = stp;
	while (ldh.l_ssize[L_SYM] > 0) {
		if (readsym(wstp) != 0)
			break;
		if (gflag && (wstp->ls_type&L_GLOBAL)==0)
			continue;
		if ((wstp->ls_type&~L_GLOBAL) == L_REF) {
			if (dflag)
				continue;
			if (uflag && wstp->ls_addr!=0)
				continue;
		} else {
			if (uflag)
				continue;
		}
		if (!aflag && !csymbol(wstp))
			continue;
		++wstp;
	}
	if ((estp = wstp) == stp) {
		free((char *) stp);
		return;
	}
	if (!pflag)
		qsort(stp, estp-stp, sizeof(*stp), qscmp);
	if (title && !oflag) {
		if (stuff)
			printf("\n");
		++stuff;
		if (amemb)
			printf("%.*s:\n", DIRSIZ, ahb.ar_name);
		else
			printf("%s:\n", fn);
	}
	wstp = stp;
	while (wstp < estp) {
		if (title && oflag) {
			if (amemb)
				printf("%.*s ", DIRSIZ, ahb.ar_name);
			else
				printf("%s ", fn);
		}
		t = wstp->ls_type & ~L_GLOBAL;
		if (t<L_SHRI || t>L_REF)
			t = L_REF+1;
		if (t==L_REF && wstp->ls_addr==0) {
			printf(fmt1);
			putchar(' ');
			putchar((wstp->ls_type&L_GLOBAL)!=0 ? 'U' : 'u');
		} else {
			printf(fmt2, wstp->ls_addr);
			printf((wstp->ls_type&L_GLOBAL)!=0 ? gn[t] : ln[t]);
		}
		printf(" %.*s\n", NCPLN, wstp->ls_id);
		++wstp;
	}
	free((char *) stp);
}

/*
 * Read the l.out header and convert
 * old ones to new ones.  Canonicalise
 * the result.
 */
nmhdr(ldhp)
register struct ldheader *ldhp;
{
	register int i;

	if ( fread((char *)ldhp, 1, sizeof(*ldhp), fp)
	    <  sizeof(*ldhp) - 2*sizeof(short)) {
		nmerr("read error");
		return (1);
	}
	canshort(ldhp->l_magic);
	canshort(ldhp->l_flag);
	canshort(ldhp->l_machine);
	if (ldhp->l_magic != L_MAGIC) {
		nmerr("not an object file");
		return (1);
	}
	if ((ldhp->l_flag&LF_32) == 0) {
		canshort(ldhp->l_tbase);
		ldhp->l_entry = ldhp->l_tbase;
		ldhp->l_tbase = sizeof(*ldhp) - 2*sizeof(short);
	} else {
		canshort(ldhp->l_tbase);
		canlong(ldhp->l_entry);
	}
	for (i=0; i<NLSEG; i++)
		cansize(ldhp->l_ssize[i]);
	return (0);
}

/*
 * Read in one entry from the symbol table.
 * Adjusts for 32-bit or 16-bit l.out formats.
 */
readsym(lsp)
register struct ldsym *lsp;
{
	vaddr_t vaddr;

	fread(lsp->ls_id, NCPLN, 1, fp);
	fread((char *)&lsp->ls_type, sizeof(int), 1, fp);
	if ((ldh.l_flag & LF_32) != 0) {
		fread((char *)&lsp->ls_addr, sizeof(long), 1, fp);
		canlong(lsp->ls_addr);
		ldh.l_ssize[L_SYM] -= sizeof(lds);
	} else {
		fread((char *)&vaddr, sizeof vaddr, 1, fp);
		canvaddr(vaddr);
		lsp->ls_addr = vaddr;
		ldh.l_ssize[L_SYM] -= sizeof(lds)-sizeof(short);
	}
	if (feof(fp)) {
		nmerr("read error");
		return (1);
	}
	canint(lsp->ls_type);
	return (0);
}

/*
 * This routine gets called if we
 * are not in `-a' mode. It determines if
 * the symbol pointed to by `sp' is a C
 * style symbol (trailing `_' or longer than
 * 7 characters). If it is it eats the `_'
 * and returns true.
 */
csymbol(sp)
register struct ldsym *sp;
{
	register char *cp1, *cp2;

	cp1 = &sp->ls_id[0];
	cp2 = &sp->ls_id[NCPLN];
	while (cp2!=cp1 && *--cp2==0)
		;
	if (*cp2 != 0) {
		if (*cp2 == '_') {
			*cp2 = 0;
			return (1);
		}
		if (cp2-cp1 >= 7)
			return (1);
	}
	return (0);
}

/*
 * Compare routine for `qsort'.
 * Handles the `-n' and `-r' command
 * line options.
 */
qscmp(sp1, sp2)
register struct ldsym *sp1, *sp2;
{
	register v;

	if (nflag) {
		if (sp1->ls_addr < sp2->ls_addr)
			v = -1;
		else if (sp1->ls_addr > sp2->ls_addr)
			v = 1;
		else
			v = 0;
	} else
		v = strncmp(sp1->ls_id, sp2->ls_id, NCPLN);
	if (rflag)
		return (-v);
	return (v);
}

/*
 * Give up.
 * Tag the line with the file
 * name.
 */
nmerr(a)
{
	fprintf(stderr, "nm: ");
	if (amemb)
		fprintf(stderr, "%.*s (file %s): ", DIRSIZ, ahb.ar_name, fn);
	else
		fprintf(stderr, "%s: ", fn);
	fprintf(stderr, "%r\n", &a);
}
