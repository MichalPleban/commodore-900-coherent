/*
 * Archive manager.
 * Also manages libraries for the
 * link editor.
 */
#define	RUNRSX	0
#define	FMTRSX	0		/* Achives in COHERENT format */

#include <stdio.h>
#include <ar.h>
#include <canon.h>
#if !RUNRSX
#include <stat.h>
#endif

#define	RO	0
#define	RW	1

#define	NONE	0
#define	BEFORE	1
#define	AFTER	2

#define	ALLOK	0
#define	NOTALL	1
#define	ERROR	2

#define	aechk()	{ if(ferror(afp)) ioerr(anp); }
#define	techk()	{ if(ferror(tfp)) ioerr(tnp); }

char	nwork[] = "No work";
char	found[]	= "found";
char	copen[]	= "%s: cannot open";
char	ccrea[] = "%s: cannot create";
char	creop[] = "%s: cannot reopen";

FILE	*afp;
FILE	*tfp;
struct	ar_hdr ahb;
int	nname;
char	**namep;
char	*ctime();
int	usage();
long	fsize();
long	ftell();
int	cflag;
int	lflag;
int	uflag;
int	vflag;
char	*pnp;
char	*anp;
char	tnp[32];
int	xstat	 = ALLOK;
int	pos	 = NONE;
int	(*ffp)() = usage;

#if RUNRSX
struct	header fhb;
time_t	fmdate();
time_t	getmdate();
#else
char	state[] = "%s: stat error";
#endif

main(argc, argv)
char *argv[];
{
	register char *p;
	register i;
	int usage(), rfunc();

	if (argc < 2)
		usage();
	key(argv[1]);
	nname = argc-2;
	namep = &argv[2];
	for (i=2; i<argc; ++i) {
		p = argv[i];
		if (pos!=NONE && pnp==NULL) {
			pnp = p;
			++namep;
			--nname;
		} else if (anp == NULL) {
			anp = p;
			++namep;
			--nname;
		}
	}
	if (anp == NULL)
		usage();
	if (pos != NONE) {
		if (pnp == NULL)
			usage();
		if (ffp == usage)
			ffp = rfunc;
	}
	(*ffp)();
	delexit(xstat);
}

/*
 * Decode key.
 * Save function name in `ffp'.
 * Set `c', `l' and `v' flags.
 */
key(p)
register char *p;
{
	register c;
	int dfunc(), rfunc(), qfunc(), tfunc();
	int mfunc(), xfunc(), pfunc();

	while (c = *p++) {
		switch (c) {

		case 'd':
			ffp = dfunc;
			break;

		case 'r':
			ffp = rfunc;
			break;

		case 'q':
			ffp = qfunc;
			break;

		case 't':
			ffp = tfunc;
			break;

		case 'p':
			ffp = pfunc;
			break;

		case 'm':
			ffp = mfunc;
			break;

		case 'x':
			ffp = xfunc;
			break;

		case 'c':
			++cflag;
			break;

		case 'l':
			++lflag;
			break;

		case 'u':
			++uflag;
			break;

		case 'v':
			++vflag;
			break;

		case 'a':
			pos = AFTER;
			break;

		case 'b':
		case 'i':
			pos = BEFORE;
			break;

		default:
			usage();
		}
	}
}

/*
 * Replace.
 * Eliminate dead stuff if `u'.
 * Copy up to insert point.
 * Copy in new files.
 * Copy the rest of the file.
 * Copy back.
 */
rfunc()
{
	register char *qfn;
	register i, nef;
	FILE *qfp;

	if (nname == 0)
		diag(1, nwork);
	aopen(RW);
	if (uflag) {
		update();
		fseek(afp, (long)sizeof(int), 0);
	}
	topen();
	while (nef = geth()) {
		if (pos==BEFORE && eqh(pnp)) {
			fseek(afp, (long)-sizeof(struct ar_hdr), 1);
			break;
		}
		if (pos == NONE) {
			for (i=0; i<nname; ++i) {
				if ((qfn=namep[i])!=NULL && eqh(qfn))
					break;
			}
			if (i != nname) {
				fseek(afp, ahb.ar_size, 1);
				namep[i] = NULL;
				remove(i, qfn);
				if ((qfp = fopen(qfn, "r")) == NULL)
					diag(1, copen, qfn);
				makeh(qfn, qfp);
				if (vflag)
					printf("%s: in place replace.\n", qfn);
				puth();
				ffcopy(tfp, tnp, qfp, qfn, fsize(qfp, qfn));
				fclose(qfp);
				continue;
			}
		}
		mmove(0);
		if (pos==AFTER &&  eqh(pnp))
			break;
	}
	if (nef==0 && pos!=NONE)
		diag(1, "%s: not in archive", pnp);
	for (i=0; i<nname; ++i) {
		if ((qfn=namep[i]) == NULL)
			continue;
		remove(i, qfn);
		if ((qfp=fopen(qfn, "r")) == NULL)
			diag(1, copen, qfn);
		makeh(qfn, qfp);
		if (vflag)
			printf("%s: replaced.\n", qfn);
		puth();
		ffcopy(tfp, tnp, qfp, qfn, fsize(qfp, qfn));
		fclose(qfp);
	}
	while (geth())
		mmove(0);
	tacopy();
}

/*
 * Handle the `u' option.
 * Read through the archive, comparing
 * the dates in the headers to the last
 * modification dates of the files in
 * the command line. Make some files
 * go away.
 */
update()
{
	register char *p;
	register i;
#if !RUNRSX
	struct stat sb;
#endif

	while (geth()) {
		for (i=0; i<nname; ++i) {
			p = namep[i];
			if (p!=NULL && eqh(p)) {
#if RUNRSX
				if (ahb.ar_date >= fmdate(p)) {
#else
				if (stat(p, &sb) < 0)
					diag(1, state, p);
				if (ahb.ar_date >= sb.st_mtime) {
#endif
					if (vflag)
						printf("%s: no update.\n", p);
					namep[i] = NULL;
					remove(i, p);
				}
			}
		}
		fseek(afp, ahb.ar_size, 1);
	}
	for (i=0; i<nname && namep[i]==NULL; ++i)
		;
	if (i >= nname)
		diag(1, nwork);
}

/*
 * Move.
 * Copy stuff before insert.
 * Copy moved stuff.
 * Copy remainder.
 * Copy back to archive.
 */
mfunc()
{
	register nef;
	long s;

	aopen(RW);
	topen();
	while (nef = geth()) {
		if (pos==BEFORE && eqh(pnp))
			break;
		mmove(0);
		if (pos==AFTER &&  eqh(pnp))
			break;
	}
	if (nef==0 && pos!=NONE)
		diag(1, "%s: not in archive", pnp);
	s = ftell(afp);
	if (pos == BEFORE)
		s -= sizeof(struct ar_hdr);
	fseek(afp, (long)sizeof(int), 0);
	while (geth())
		mmove(1);
	if (nef) {
		fseek(afp, s, 0);
		while (geth())
			mmove(0);
	}
	tacopy();
}

/*
 * Conditional move to the
 * temp file from the archive
 * file. Used by move and replace
 */
mmove(f1)
register f1;
{
	register f2, i;
	register long size;

	f2 = 0;
	for (i=0; i<nname; ++i) {
		if (eqh(namep[i])) {
			++f2;
			break;
		}
	}
	if (f1 == f2) {
		if (vflag)
			amsg("copied");
		size = ahb.ar_size;
		puth();
		ffcopy(tfp, tnp, afp, anp, size);
	} else {
		if (vflag)
			amsg("skipped");
		fseek(afp, ahb.ar_size, 1);
	}
}

/*
 * Print.
 */
pfunc()
{
	aopen(RO);
	while (geth()) {
		if (nname==0 || match())
			pfile();
		else
			fseek(afp, ahb.ar_size, 1);
	}
	if (nname != 0)
		notdone(found);
}

pfile()
{
	register char *rb;
	register i;
	long s;
	int n;

	if (vflag)
		amsg(NULL);
#if FMTRSX
	if ((rb = malloc(ahb.ar_ufat.f_rsiz)) == NULL)
		diag(1, "Out of space");
	s = ahb.ar_size;
	while (s != 0) {
		if (ahb.ar_ufat.f_rtyp == R_FIX)
			n = ahb.ar_ufat.f_rsiz;
		else {
			fread(&n, sizeof(int), 1, afp);
			s -= sizeof(int);
		}
		fread(rb, sizeof(char), n, afp);
		for (i=0; i<n; ++i)
			putchar(rb[i]);
		if ((ahb.ar_ufat.f_ratt&FD_CR) != 0)
			putchar('\n');
		if ((n&01) != 0) {
			++n;
			fseek(afp, (long)1, 1);
		}
		s -= n;
	}
	free(rb);
#else
	ffcopy(stdout, "Stdout", afp, anp, ahb.ar_size);
#endif
}

/*
 * Delete.
 * Copy archive to temp, deleting
 * members along the way. If all of
 * the files have been deleted then
 * copy back.
 */
dfunc()
{
	register long size;

	if (nname == 0)
		diag(1, nwork);
	aopen(RW);
	topen();
	while (geth()) {
		if (match()) {
			if (vflag)
				amsg("deleted");
			fseek(afp, ahb.ar_size, 1);
			continue;
		}
		if (vflag)
			amsg("copied");
		size = ahb.ar_size;
		puth();
		ffcopy(tfp, tnp, afp, anp, size);
	}
	if (notdone("deleted"))
		delexit(ERROR);
	tacopy();
}

/*
 * Quick insert.
 * Copy archive to temp file.
 * Tack new files onto the end.
 * If no errors, copy back.
 */
qfunc()
{
	register char *qfn;
	register FILE *qfp;
	register i;

	if (nname == 0)
		diag(1, nwork);
	aopen(RW);
	fseek(afp, (long)0, 2);
	for (i=0; i<nname; ++i) {
		if ((qfn=namep[i]) == NULL)
			continue;
		remove(i, qfn);
		if ((qfp=fopen(qfn, "r")) == NULL)
			diag(1, copen, qfn);
		makeh(qfn, qfp);
		if (vflag)
			printf("%s: quick insert.\n", qfn);
		cantime(ahb.ar_date);
		canshort(ahb.ar_gid);
		canshort(ahb.ar_uid);
		canshort(ahb.ar_mode);
		cansize(ahb.ar_size);
		fwrite(&ahb, sizeof(ahb), 1, afp);
		aechk();
		ffcopy(afp, anp, qfp, qfn, fsize(qfp, qfn));
		fclose(qfp);
	}
}

/*
 * Table.
 * Read through archive.
 * If good member print its name.
 * If verbose, print extra stuff.
 */
tfunc()
{
	register char *p;
	register c, n;

	aopen(RO);
	while (geth()) {
		if (nname==0 || match()) {
			n = 0;
			p = ahb.ar_name;
			while (n<DIRSIZ && (c=*p++)) {
				putchar(c);
				++n;
			}
			if (vflag) {
				while (n++ < DIRSIZ+1)
					putchar(' ');
#if FMTRSX
				printf("[%03o,%03o] ", ahb.ar_gid, ahb.ar_uid);
				printf("%06o ", ahb.ar_mode);
#else
				printf("%5d %5d ", ahb.ar_gid, ahb.ar_uid);
				printf("%03o ",  ahb.ar_mode);
#endif
				printf("%10ld ", ahb.ar_size);
				printf("%s", ctime(&ahb.ar_date));
			} else
				putchar('\n');
		}
		fseek(afp, ahb.ar_size, 1);
	}
	if (nname != 0)
		notdone(found);
}

/*
 * Extract.
 * Read through archive.
 * Extract any files you find.
 * At end, mutter about files
 * that were not there.
 */
xfunc()
{
	register char *p1, *p2;
	register c;
	char fnb[DIRSIZ+1];
	FILE *xfp;

	aopen(RO);
	while (geth()) {
		if (nname==0 || match()) {
			p1 = ahb.ar_name;
			p2 = fnb;
			while (p1<&ahb.ar_name[DIRSIZ] && (c=*p1++))
				*p2++ = c;
			*p2 = 0;
			if ((xfp=fopen(fnb, "w")) == NULL) {
				diag(0, ccrea, fnb);
				fseek(afp, ahb.ar_size, 1);
				continue;
			}
			if (vflag)
				amsg("extracting");
			ffcopy(xfp, fnb, afp, anp, ahb.ar_size);
#if RUNRSX && FMTRSX
			fseek(xfp, (long)0, 0);
			xfp->v_rtyp = ahb.ar_ufat.f_rtyp;
			xfp->v_ratt = ahb.ar_ufat.f_ratt;
			xfp->v_rsiz = ahb.ar_ufat.f_rsiz;
			xfp->v_hibk = ahb.ar_ufat.f_hibk;
			xfp->v_efbk = ahb.ar_ufat.f_efbk;
			xfp->v_ffby = ahb.ar_ufat.f_ffby;
#endif
#if !RUNRSX && !FMTRSX
			chmod(fnb, ahb.ar_mode);
#endif
			fclose(xfp);
		} else
			fseek(afp, ahb.ar_size, 1);
	}
	if (nname != 0)
		notdone(found);
}

/*
 * Make an archive header.
 * Put it in the external archive
 * header buffer `ahb'. The arg.
 * `fn' is the file name. The file
 * is open on `fp'.
 */
makeh(fn, fp)
char *fn;
FILE *fp;
{
	register char *p1, *p2;
	register c;
#if !RUNRSX
	struct stat sb;
#endif

	for (p1=fn; *p1++; )
		;
	while (p1 > fn) {
		c = p1[-1];
#if RUNRSX
		if (c==':' || c==']')
			break;
#else
		if (c == '/')
			break;
#endif
		--p1;
	}
	p2 = ahb.ar_name;
	while (c = *p1++) {
		if (p2 < &ahb.ar_name[DIRSIZ])
			*p2++ = c;
	}
	while (p2 < &ahb.ar_name[DIRSIZ])
		*p2++ = 0;
#if RUNRSX
	if (ratt(fp, &fhb) == 0)
		diag(1, "%s: ratt failed", fn);
	ahb.ar_date = getmdate(&fhb);
	ahb.ar_size = fsize(fp, fn);
#if FMTRSX
	ahb.ar_uid  = fhb.h_prog;
	ahb.ar_gid  = fhb.h_proj;
	ahb.ar_mode = fhb.h_fpro;
	ahb.ar_ufat.f_rtyp = fp->v_rtyp;
	ahb.ar_ufat.f_ratt = fp->v_ratt;
	ahb.ar_ufat.f_rsiz = fp->v_rsiz;
	ahb.ar_ufat.f_hibk = fp->v_hibk;
	ahb.ar_ufat.f_efbk = fp->v_efbk;
	ahb.ar_ufat.f_ffby = fp->v_ffby;
#else
	ahb.ar_uid  = 0;
	ahb.ar_gid  = 0;
	ahb.ar_mode = 0644;
#endif
#else
	if (fstat(fileno(fp), &sb) < 0)
		diag(1, state, fn);
	time(&ahb.ar_date);
	ahb.ar_uid  = sb.st_uid;
	ahb.ar_gid  = sb.st_gid;
	ahb.ar_mode = sb.st_mode&0777;
	ahb.ar_size = sb.st_size;
#endif
}

/*
 * Test if the member whose
 * header is held in the archive
 * header buffer is mentioned in
 * the user's list of members.
 * Return the number of matches.
 * All matches are NULLed.
 */
match()
{
	register char *p;
	register i, n;

	n = 0;
	for (i=0; i<nname; ++i) {
		if ((p=namep[i]) == NULL)
			continue;
		if (eqh(p)) {
			++n;
			namep[i] = NULL;
		}
	}
	return (n);
}

/*
 * Remove all instances of name
 * `fn' from the list of names that
 * is described by `namep' and
 * `nname'.
 * Start at index `i+1'.
 */
remove(i, fn)
register i;
register char *fn;
{
	register char *p;

	for (++i; i<nname; ++i) {
		if ((p=namep[i]) == NULL)
			continue;
		if (strcmp(fn, p) == 0)
			namep[i] = NULL;
	}
}

/*
 * This routine digs through the
 * list of names described by `namep'
 * and `nname' looking for names that
 * have not been NULLed out. If any
 * are found it prints a title and 
 * the names. The number of names that
 * were found is returned.
 */
notdone(s)
char *s;
{
	register char *p;
	register i, n;

	n = 0;
	for (i=0; i<nname; ++i) {
		p = namep[i];
		if (p != NULL) {
			if (n++ == 0)
				fprintf(stderr, "Not %s:\n", s);
			fprintf(stderr, "%s\n", p);
		}
	}
	return (n);
}

/*
 * File to file copy.
 */
ffcopy(tfp, tfn, ffp, ffn, s)
FILE *tfp, *ffp;
char *tfn, *ffn;
long s;
{
	register n;
	static char fb[BUFSIZ];

	while (s != 0) {
		n = (s>BUFSIZ) ? BUFSIZ : s;
		if (fread (fb, sizeof(char), n, ffp) != n)
			ioerr(ffn);
		if (fwrite(fb, sizeof(char), n, tfp) != n)
			ioerr(tfn);
		s -= n;
	}
}

/*
 * Get the next archive header
 * into `ahb'. Check for any I/O
 * errors. Return true if a header
 * was read and false on EOF.
 */
geth()
{
	if (fread(&ahb, sizeof(ahb), 1, afp) != 1) {
		aechk();
		return (0);
	}
	cantime(ahb.ar_date);
	canshort(ahb.ar_gid);
	canshort(ahb.ar_uid);
	canshort(ahb.ar_mode);
	cansize(ahb.ar_size);
	return (1);
}

/*
 * Write the header in `ahb' to
 * the temp file.
 */
puth()
{
	cantime(ahb.ar_date);
	canshort(ahb.ar_gid);
	canshort(ahb.ar_uid);
	canshort(ahb.ar_mode);
	cansize(ahb.ar_size);
	fwrite(&ahb, sizeof(ahb), 1, tfp);
	techk();
}

/*
 * Compare a string to the name
 * of the member in the archive header
 * buffer. True return if same.
 */
eqh(p)
char *p;
{
	register char *p1, *p2;
	register c;

	if ((p1 = p) == NULL)
		return (0);
	while (*p1++)
		;
	while (p1 > p) {
		c = p1[-1];
#if RUNRSX
		if (c==':' || c==']')
			break;
#else
		if (c == '/')
			break;
#endif
		--p1;
	}
	p2 = ahb.ar_name;
	c  = DIRSIZ;
	while (c && *p1 == *p2++) {
		if (*p1++ == 0)
			return (1);
		--c;
	}
	if (c == 0)
		return (1);
	return (0);
}

/*
 * Open archive.
 * The argument `aam' is the
 * access mode (RO or RW).
 */
aopen(aam)
{
	int i;

	if ((afp=fopen(anp, "r")) == NULL) {
		if (aam == RO)
			diag(1, copen, anp);
		if ((afp=fopen(anp, "w"))==NULL
		 || (afp=freopen(anp, "w+r", afp))==NULL)
			diag(1, ccrea, anp);
		if (cflag == 0)
			printf("%s: new archive.\n", anp);
		i = ARMAG;
		canint(i);
		fwrite(&i, sizeof(i), 1, afp);
		aechk();
		return;
	}
	if (aam != RO) {
		fclose(afp);
		if ((afp=fopen(anp, "r+w"))==NULL)
			diag(1, copen, anp);
	}
	fread(&i, sizeof(i), 1, afp);
	aechk();
	canint(i);
	if (i != ARMAG)
		diag(1, "%s: not an archive", anp);
}

/*
 * Open tempfile.
 * Stash the name in `tnp' for
 * the benefit of `delexit'.
 * Honour the `l' option w.r.t.
 * file placement.
 */
topen()
{
	register char *p1, *p2;
	int i;

	p1 = tnp;
#if RUNRSX
	p2 = "ar.tmp";
	while (*p1++ = *p2++)
		;
#else
	if (lflag == 0) {
		p2 = "/tmp/";
		while (*p1++ = *p2++)
			;
		--p1;
	}
	p2 = "vxxxxxx";
	while (*p1++ = *p2++)
		;
	mktemp(tnp);
#endif
	if ((tfp=fopen(tnp, "w")) == NULL) 
		diag(1, ccrea, tnp);
	i = ARMAG;
	canint(i);
	fwrite(&i, sizeof(i), 1, tfp);
	techk();
}

/*
 * Copy tempfile back to the
 * archive.
 */
tacopy()
{
	register FILE *xtp;

	fclose(tfp);
	tfp = NULL;  /* Scare off delexit */
	fclose(afp);
	if ((xtp=fopen(tnp, "r")) == NULL)
		diag(1, creop, tnp);
	if ((afp=fopen(anp, "w")) == NULL)
		diag(1, creop, tnp);
	if (vflag)
		printf("%s: copy back.\n", anp);
	ffcopy(afp, anp, xtp, tnp, fsize(xtp, tnp));
	tfp = xtp;   /* Delete */
}

/*
 * Write diagnostic.
 * The flag `f' marks fatal errors.
 */
diag(f, a)
{
	fprintf(stderr, "%r", &a);
	fprintf(stderr, ".\n");
	if (f)
		delexit(ERROR);
	xstat = NOTALL;
}

/*
 * Print a message for a
 * given archive member. The header
 * is in the header buffer. 
 */
amsg(s)
char *s;
{
	register char *p;
	register c;

	p = ahb.ar_name;
	while (p<&ahb.ar_name[DIRSIZ] && (c=*p++)!=0)
		putchar(c);
	putchar(':');
	if (s != NULL)
		printf(" %s.", s);
	putchar('\n');
}

/*
 * Exit.
 * Delete the tempfile if
 * present.
 */
delexit(s)
{
#if RUNRSX
	if (tfp != NULL)
		fmkdl(tfp);
#else
	if (tfp != NULL)
		unlink(tnp);
#endif
#ifdef DEBUG
	if (s)
		abort();
#endif
	exit(s);
}

/*
 * Mutter about an I/O error
 * on file `s'.
 */
ioerr(s)
char *s;
{
	diag(1, "%s: I/O error", s);
}

/*
 * Print usage message.
 */
usage()
{
	fprintf(stderr, "Usage: ar options [posname] afile [name ...].\n");
	delexit(ERROR);
}

/*
 * Compute the size of a file.
 * In bytes.
 * The file must not be seeked.
 */
long
fsize(fp, fn)
register FILE *fp;
char *fn;
{
#if RUNRSX
	return (((fp->v_efbk-1)<<9) + fp->v_ffby);
#else
	struct stat sb;

	if (fstat(fileno(fp), &sb) < 0)
		diag(1, state, fn);
	return (sb.st_size);
#endif
}

#if RUNRSX
/*
 * The following routines simulate
 * some aspects of Coherent under the
 * dreaded RSX.
 */
#define	DAY	(24L*60L*60L)

time_t
fmdate(fn)
char *fn;
{
	register FILE *fp;
	register s;

	if ((fp=fopen(fn, "r")) == NULL)
		diag(1, copen, fn);
	s = ratt(fp, &fhb);
	fclose(fp);
	if (s == 0)
		diag(1, "%s: ratt failed", fn);
	return (getmdate(&fhb));
}

time_t
getmdate(fhp)
register struct header *fhp;
{
	register i;
	time_t date;
	int rsxdate[8];
	static	char *mtab[] = {
		"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
		"JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
	};

	rsxdate[0] = (fhp->i_rvdt[5]-'0')*10+fhp->i_rvdt[6]-'0';
	for (i=0; i<12; ++i) {
		if (cmp3(&fhp->i_rvdt[2], mtab[i])) {
			rsxdate[1] = i+1;
			break;
		}
	}
	rsxdate[2] = (fhp->i_rvdt[0]-'0')*10+fhp->i_rvdt[1]-'0';
	rsxdate[3] = (fhp->i_rvti[0]-'0')*10+fhp->i_rvti[1]-'0';
	rsxdate[4] = (fhp->i_rvti[2]-'0')*10+fhp->i_rvti[3]-'0';
	rsxdate[5] = (fhp->i_rvti[4]-'0')*10+fhp->i_rvti[5]-'0';
	cvttime(&date, rsxdate);
	return (date);
}

cmp3(p1, p2)
register char *p1, *p2;
{
	if (*p1++==*p2++ && *p1++==*p2++ && *p1++==*p2++)
		return (1);
	return (0);
}

/*
 * Convert rsx date to Coherent
 * format. This routine assumes that
 * the timezone is CST.
 * Courtesy of Tom Duff.
 */
cvttime(date, rsxdate)
register long *date;
int rsxdate[8];
{
	register i;

	*date = 0;
	for (i=70; i!=rsxdate[0]; i++) {
		if (i%4==0)
			*date += 366L*DAY; else
			*date += 365L*DAY;
	}
	for (i=1; i!=rsxdate[1]; i++)
		switch (i){
		case 9:  /* Sep */
		case 4:  /* Apr */
		case 6:  /* Jun */
		case 11: /* Nov */
			*date += 30L*DAY;
			break;

		case 2:  /* Feb */
			if (rsxdate[0]%4 == 0)
				*date += 29L*DAY; else
				*date += 28L*DAY;
			break;

		default:
			*date += 31L*DAY;
		}
	*date += (rsxdate[2]-1)*DAY;
	*date += rsxdate[3]*60L*60L;
	*date += rsxdate[4]*60L;
	*date += rsxdate[5];
	*date += 6L*60L*60L;	/* adjust cst to gmt */
}
#endif
