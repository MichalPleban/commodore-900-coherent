/*
 * Look at a file and try to
 * figure out its type. Knows about the various
 * flavours of filesystem entries, object files of various
 * types, C programs, input to one of the various flavours
 * of text formatter, etc.
 */
#include <stdio.h>
#include <ctype.h>
#include <stat.h>
#include "n.out.h"
#include <canon.h>
#include <ar.h>

/* UNIX archive magic numbers */
#define	UARMAG	0177545		/* UNIX v7 archives */
#define	OUARMAG	0177555		/* UNIX v6 and previous archives */

#define	EXEC	(S_IEXEC|(S_IEXEC<<3)|(S_IEXEC<<6))

/*
 * The first BUFSIZ bytes of the
 * file in question are read into this
 * union. It contains members for accessing
 * the possible file structures.
 */
union	iobuf
{
	char	u_buf[BUFSIZ];		/* General data */
	struct	ldheader u_ld;		/* Object file header */
	int	u_armag;		/* Archive number */
};

union	iobuf	iobuf;
char	*file();
char	*textclass();
char	*objclass();
char	*dirtype();
char	*mtype();
char	*strcat();

main(argc, argv)
char *argv[];
{
	struct stat sb;
	register char *p;
	register int i;
	register int estat;

	if (argc < 2) {
		fprintf(stderr, "Usage: file name ...\n");
		exit(1);
	}
	estat = 0;
	for (i=1; i<argc; i++) {
		p = argv[i];
		if (stat(p, &sb) < 0) {
			fprintf(stderr, "file: %s: not accessible\n", p);
			estat = 1;
			continue;
		}
		printf("%s: %s\n", p, file(&sb, p));
	}
	exit(estat);
}

/*
 * Routine to guess filetype
 */
char *
file(sbp, fn)
struct stat *sbp;
char *fn;
{
	static char buf[50];
	int magic;
	register int i, nb;
	register int fd = -1;

	if ((sbp->st_mode&S_IFMT) != S_IFREG) {
		switch (sbp->st_mode & S_IFMT) {
		case S_IFDIR:
			return (dirtype(fn));

		case S_IFBLK:
			sprintf(buf, "block special file %d/%d",
			    major(sbp->st_rdev), minor(sbp->st_rdev));
			return (buf);

		case S_IFCHR:
			sprintf(buf, "character special file %d/%d",
			    major(sbp->st_rdev), minor(sbp->st_rdev));
			return (buf);

		case S_IFMPB:
			return ("block multiplexor file");

		case S_IFMPC:
			return ("character multiplexor file");

		case S_IFPIP:
			return ("named pipe");

		default:
			return ("invalid filetype");
		}
	}
	if ((fd = open(fn, 0)) < 0)
		return ("unreadable");
	if ((nb = read(fd, (char *) &iobuf, sizeof(iobuf))) < 0) {
		close(fd);
		return ("read error");
	}
	close(fd);
	if (nb == 0)
		return ("empty");
	if (nb >= sizeof(struct ldheader)) {
		magic = iobuf.u_ld.l_magic;
		canint(magic);
		if (magic == L_MAGIC) {
			canint(iobuf.u_ld.l_flag);
			canint(iobuf.u_ld.l_machine);
			return (objclass(fd, &iobuf.u_ld));
		}
	}
	if (nb >= sizeof (int)) {
		magic = iobuf.u_armag;
		canint(magic);
		if (magic == ARMAG)
			return ("archive");
		if (magic == UARMAG)
			return ("seventh edition archive");
		if (magic == OUARMAG)
			return ("sixth edition archive");
	}
	if (hasnonascii((unsigned char *) &iobuf, nb))
		return ("binary data");
	if ((sbp->st_mode&EXEC) != 0)
		return ("commands");
	return (textclass((unsigned char *) &iobuf, nb));
}

/*
 * Return the type of the directory.
 * Currently, only "s.*" is recognised as SCCS directory.
 */
char *
dirtype(dn)
register char *dn;
{
	register char *cp;

	for (cp=dn; *cp!='\0'; cp++)
		;
	while (cp > dn)
		if (*--cp == '/') {
			cp++;
			break;
		}
	if (cp[0]=='s' && cp[1]=='.')
		return ("SCCS directory");
	return ("directory");
}

/*
 * Return true if there ar characters
 * in the buffer that do not look like good
 * ascii characters.
 * This routine knows that ascii is a seven
 * bit code.
 */
hasnonascii(bp, nb)
register unsigned char *bp;
register nb;
{
	while (nb--) {
		if (!isascii(*bp))
			return (1);
		if (!(isprint(*bp) || isspace(*bp) || *bp=='\b'))
			return (1);
		bp++;
	}
	return (0);
}

/*
 * Classify the first `nb'
 * bytes of a text file.
 */
char *
textclass(bp, nb)
register unsigned char *bp;
register nb;
{
	register int c;
	int nlf = 1;
	int nlbrace;
	int nrbrace;
	int nsharps;
	int nsemi;
	int xmail;
	char *sbp = bp;

	nlbrace = 0;
	nrbrace = 0;
	nsharps = 0;
	nsemi = 0;
	xmail = 0;
	while (nb--) {
		c = *bp++;
		if (c=='.' && strncmp(bp, "globl", 5)==0)
			return ("assembler");
		if (nlf) {
			if (c == '.')
				return ("nroff, tbl or eqn input");
			else if (c == ':')
				return ("commands");
			else if (c == '#')
				++nsharps;
			else if (c=='%')
				if (*bp=='%' || *bp=='{' || *bp=='}')
					return ("yacc or lex input");
		}
		if (c == '{')
			++nlbrace;
		else if (c == '}')
			++nrbrace;
		if (c == '\n') {
			nlf = 1;
			if (bp[-2] == ';')
				nsemi++;
			if (((bp-sbp) % 22) == 0) {
				if (bp[-2]==' ' || bp[-2]=='!')
					xmail++;
			} else
				xmail = 0;
		} else
			nlf = 0;
	}
	if (xmail)
		return ("xmail encoded text");
	if ((nsharps || nsemi) && (nlbrace || nrbrace))
		return ("C program");
	return ("probably text");
}

/*
 * Figure out the type of an
 * object file. Tag it with the machine
 * id if not for the machine upon which the
 * command is running.
 */
char *
objclass(fd, lhp)
register struct ldheader *lhp;
{
	static char type[64];
	register char *mch;
	struct ldsym lds;
	register size_t stbase;
	register i;

	type[0] = '\0';
	if ((lhp->l_flag&LF_32) != 0)
		strcat(type, "32 bit ");
	if ((lhp->l_flag&LF_SLIB) != 0)
		strcat(type, "shared library ");
	if ((lhp->l_flag&LF_SLREF) != 0)
		strcat(type, "libref ");
	if ((lhp->l_flag&LF_SHR) != 0)
		strcat(type, "shared ");
	if ((lhp->l_flag&LF_SEP) != 0)
		strcat(type, "separate ");
	if ((lhp->l_flag&LF_KER) != 0) {
		register unsigned ssize;

		ssize = sizeof (lds);
		if ((lhp->l_flag & LF_32) == 0) {
			stbase = sizeof(*lhp) - 2*sizeof(int);
			ssize -= sizeof (int);
		} else {
			canshort(lhp->l_tbase);
			stbase = lhp->l_tbase;
		}
		for (i=L_SHRI; i<L_SYM; ++i)
			if (i!=L_BSSI && i!=L_BSSD) {
				cansize(lhp->l_ssize[i]);
				stbase += lhp->l_ssize[i];
			}
		lseek(fd, stbase, 0);
		cansize(lhp->l_ssize[L_SYM]);
		while (lhp->l_ssize[L_SYM] != 0) {
			if (read(fd, &lds, ssize) != ssize)
				break;
			canshort(lds.ls_type);
			if (strncmp(lds.ls_id, "conftab_", NCPLN) == 0
			&& (lds.ls_type&LR_SEG) != L_REF)
				break;
			lhp->l_ssize[L_SYM] -= ssize;
		}
		strcat(type, lhp->l_ssize[L_SYM]==0?"kernel ":"driver ");
	}
	strcat(type, "executable");
	if ((lhp->l_flag&LF_NRB) == 0)
		strcat(type, " with relocation");
	if ((mch = mtype(lhp->l_machine)) == NULL)
		mch = "Unknown machine type";
	sprintf(type, "%s (%s)", type, mch);
	return (type);
}
