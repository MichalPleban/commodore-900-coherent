/*
 * size -- l.out sizes.
 * size [-c] [file file ...]
 * dgc	10-Oct-80
 */
#include <stdio.h>
#include <canon.h>
#include "n.out.h"

int	cflag;
int	tflag;
FILE	*fp;
long	tsize[L_DEBUG];
long	tcomm;
long	total;

main(argc, argv)
char *argv[];
{
	register char *p;
	register c, i;
	int nf;

	i = 1;
	while (i<argc && argv[i][0]=='-') {
		p = &argv[i][1];
		while (c = *p++) {
			switch (c) {

			case 'c':
				++cflag;
				break;

			case 't':
				++tflag;
				break;

			default:
				usage();
			}
		}
		++i;
	}
	if ((nf = argc-i) != 0) {
		while (i < argc)
			size(argv[i++], nf>1);
		if (nf>1 && tflag) {
			printf("Total: ");
			for (i=L_SHRI; i<L_DEBUG; ++i) {
				if (i != L_SHRI)
					putchar('+');
				printf("%D", tsize[i]);
			}
			if (cflag && tcomm!=0)
				printf("+%D", tcomm);
			printf(" %D (", total);
			printf(AFMT, (vaddr_t)total);
			printf(")\n");
		}
	} else
		size("l.out", 0);
}

size(fn, f)
char *fn;
{
	register i;
	long b, c, s, t;
	struct ldheader ldh;
	struct ldsym	lds;

	if ((fp = fopen(fn, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open.\n", fn);
		return;
	}
	fread(&ldh, sizeof(ldh), 1, fp);
	canshort(ldh.l_magic);
	if (feof(fp) || ldh.l_magic!=L_MAGIC) {
		fprintf(stderr, "%s: not an object file.\n", fn);
		fclose(fp);
		return;
	}
	canshort(ldh.l_flag);
	if ((ldh.l_flag & LF_32) != 0) {
		canshort(ldh.l_tbase);
		b = ldh.l_tbase;
	} else
		b = sizeof(ldh) - 2*sizeof(short);
	if (f)
		printf("%s: ", fn);
	t = 0;
	for (i=L_SHRI; i<L_SYM; ++i) {
		cansize(ldh.l_ssize[i]);
		s = ldh.l_ssize[i];
		if (i != L_DEBUG) {
			if (i != L_SHRI)
				putchar('+');
			printf("%D", s);
			t += s;
			tsize[i] += s;
		}
		if(i!=L_BSSI && i!=L_BSSD)
			b += s;
	}
	if (cflag) {
		c = 0;
		fseek(fp, b, 0);
		cansize(ldh.l_ssize[L_SYM]);
		if (ldh.l_flag & LF_32) {
			while (ldh.l_ssize[L_SYM]) {
				fread(&lds, sizeof(lds), 1, fp);
				canshort(lds.ls_type);
				canlong(lds.ls_addr);
				if (lds.ls_type == (L_GLOBAL|L_REF))
					c += (long)lds.ls_addr;
				ldh.l_ssize[L_SYM] -= sizeof(lds);
			}
		} else {
			while (ldh.l_ssize[L_SYM]) {
				fread(&lds, sizeof(lds)-sizeof(short), 1, fp);
				canshort(lds.ls_type);
				i = *(unsigned *)(&lds.ls_addr);
				canshort(i);
				if (lds.ls_type == (L_GLOBAL|L_REF))
					c += (unsigned)i;
				ldh.l_ssize[L_SYM] -= sizeof(lds)-sizeof(short);
			}
		}
		if (c != 0) {
			printf("+%D", c);
			t += c;
			tcomm += c;
		}
	}
	printf(" %D (", t);
	printf(AFMT, (vaddr_t)t);
	printf(")\n");
	total += t;
	fclose(fp);
}

usage()
{
	fprintf(stderr, "Usage: size [-ct] [file ...]\n");
	exit(1);
}
