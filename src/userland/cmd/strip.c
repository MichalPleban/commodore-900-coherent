/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Strip the debug table, the symbol
 * table and the relocation information from a
 * object file.
 */
#include <stdio.h>
#include "n.out.h"
#include <signal.h>
#include <canon.h>

char	tmpnam[]  = "/tmp/stripXXXXXX";
char	werror[]  = "write error";
char	rerror[]  = "cannot reopen";
char	xerror[]  = "unexpected end of file";

struct	ldheader lh;
FILE	*ifp;
FILE	*ofp;

int	cleanup();

main(argc, argv)
char *argv[];
{
	register estat, i;

	if (argc < 2)
		usage();
	mktemp(tmpnam);
	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		signal(SIGINT, cleanup);
	estat = 0;
	for (i=1; i<argc; i++) {
		estat |= strip(argv[i]);
		if (ifp != NULL) {
			fclose(ifp);
			ifp = NULL;
		}
		if (ofp != NULL) {
			fclose(ofp);
			ofp = NULL;
		}
	}
	cleanup(estat);
}

/*
 * Do all the work.
 * The external variables `ifp' and `ofp'
 * will be closed by the mainline. This makes the
 * many error conditions a little easier to
 * deal with.
 */
strip(fn)
char *fn;
{
	register c;
	register size_t nsize;
	register short flags;

	if ((ifp = fopen(fn, "r")) == NULL) {
		diag("cannot open", fn);
		return (1);
	}
	if ((ofp = fopen(tmpnam, "w")) == NULL) {
		diag("cannot create", tmpnam);
		return (1);
	}
	if (fread(&lh, sizeof(lh), 1, ifp) != 1) {
		diag(xerror, fn);
		return (1);
	}
	canlh();
	if (lh.l_magic != L_MAGIC) {
		diag("bad format", fn);
		return (1);
	}
	if (stripped())
		return (0);
	nsize = lh.l_ssize[L_SHRI] + lh.l_ssize[L_SHRD]
	      + lh.l_ssize[L_PRVI] + lh.l_ssize[L_PRVD];
	lh.l_flag |= LF_NRB;
	lh.l_ssize[L_DEBUG] = 0;
	lh.l_ssize[L_SYM] = 0;
	lh.l_ssize[L_REL] = 0;
	flags = lh.l_flag;
	canlh();
	if ((flags & LF_32) != 0) {
		if (fwrite(&lh, sizeof(lh), 1, ofp) != 1) {
			diag(werror, tmpnam);
			return (1);
		}
	} else {
		if (fwrite(&lh, sizeof(lh)-2*sizeof(short), 1, ofp) != 1) {
			diag(werror, tmpnam);
			return(1);
		}
		fseek(ifp, (long)-2*sizeof(short), 1);
	}
	while (nsize-- > 0) {
		if ((c = getc(ifp)) == EOF) {
			diag(xerror, fn);
			return (1);
		}
		putc(c, ofp);
	}
	fflush(ofp);
	if (ferror(ofp)) {
		diag(werror, tmpnam);
		return (1);
	}
	fclose(ifp);
	ifp = NULL;
	fclose(ofp);
	ofp = NULL;
	if ((ifp = fopen(tmpnam, "r")) == NULL) {
		diag(rerror, tmpnam);
		return (1);
	}
	if ((ofp = fopen(fn, "w")) == NULL) {
		diag(rerror, fn);
		return (1);
	}
	while ((c = getc(ifp)) != EOF)
		putc(c, ofp);
	fflush(ofp);
	if (ferror(ofp)) {
		diag(werror, fn);
		fprintf(stderr, "strip: file saved in %s\n", tmpnam);
		exit(1);
	}
	return (0);
}

/*
 * Convert the l.out header structure
 * in the buffer `lh' to and from standard
 * ordering.
 */
canlh()
{
	register i;

	canshort(lh.l_magic);
	canshort(lh.l_flag);
	for (i=0; i<NLSEG; ++i)
		cansize(lh.l_ssize[i]);
}

/*
 * Print diagnostics.
 */
diag(s, fn)
char *s;
char *fn;
{
	fprintf(stderr, "strip: ");
	if (fn != NULL)
		fprintf(stderr, "%s: ", fn);
	fprintf(stderr, "%s\n", s);
}

/*
 * Check if a file has been stripped.
 * The header is in `lh' and has been converted
 * into machine native format.
 */
stripped()
{
	if ((lh.l_flag&LF_NRB) == 0)
		return (0);
	if (lh.l_ssize[L_DEBUG] || lh.l_ssize[L_SYM] || lh.l_ssize[L_REL])
		return (0);
	return (1);
}

/*
 * Cleanup tempfile and exit.
 * The status is the exit status.
 * When a signal occurs, it is the signal
 * number which is non-zero so
 * this will be an acceptable exit status.
 */
cleanup(s)
{
	unlink(tmpnam);
	exit(s);
}

/*
 * Print usage message.
 */
usage()
{
	fprintf(stderr, "Usage: strip name ...\n");
	exit(1);
}
