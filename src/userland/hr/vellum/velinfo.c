/*
 * velinfo.c - velinfo: what the set SAYS, as opposed to what it draws.
 *
 *	velinfo -len file.d ...		the takeoff: run lengths, areas
 *	velinfo -where STR file.d ...	the set-wide Find
 *	velinfo -symsheet lib.sym	a library's reference card
 *	velinfo -book file.d ...	the set's contents page
 *
 * The set stops being something you only draw and print and becomes
 * something you ASK.  Two of the four answer in prose on stdout and
 * two answer with an ordinary DRAWING (a card, a contents page) --
 * which is the point: a drawing is the format the rest of the suite
 * already prints, so `velinfo -symsheet lib.sym | velplot -T ps -fit -'
 * needs no temp file and no new backend.
 */
#include <stdio.h>
#include "vellum.h"

/* the shared connectivity's results (libvellum, velnetc.c) -- -len
 * measures the nets the same union-find found, so a run length and a
 * netlist can never disagree */
extern short	pobj[], ppin[], pnet[];
extern int	np;
extern char	nnm[][10];
extern short	nroot[];
extern int	nnames;

/* net root -> the sheet-local net number the netlist would print:
 * every question asked of the connectivity numbers nets the same
 * way, from the same walk. */
static short	rid[MAXOBJ];
static char	seen[MAXOBJ];

/* ================================================================== */
/* -len: the takeoff (VELLUM.md sec. 46)                              */
/*                                                                    */
/* The drawing already knows what the purchasing clerk retypes: every  */
/* wire, cable and pipe run is geometry TIMES the sheet unit.  Runs    */
/* report per NET where the conductors have names (netbuild's merge    */
/* machinery, reused), per LAYER otherwise; closed filled polylines    */
/* report AREA, because sheet metal is ordered by area the way cable   */
/* is ordered by length.                                              */
/* ================================================================== */

#define	MAXLB	40		/* distinct run buckets                   */
#define	MAXLA	24		/* areas reported individually            */

static char	lbnm[MAXLB][24];
static long	lblen[MAXLB];
static int	nlb, lbfull;
static long	laa[MAXLA];	/* TWICE the area, grid units squared     */
static int	nla, lafull;

static
lbadd(nm, d)
char *nm;
long d;
{
	register int i;

	for ( i = 0; i < nlb; i++ )
		if ( strcmp(lbnm[i], nm) == 0 )
			break;
	if ( i == nlb )
	{
		if ( nlb >= MAXLB )
		{
			/* a takeoff that quietly stopped adding is worse
			 * than no takeoff: say so (once) */
			if ( !lbfull )
				fprintf(stderr,
			"velinfo: more than %d runs to total; %s dropped\n",
					MAXLB, nm);
			lbfull = 1;
			return 0;
		}
		strncpy(lbnm[i], nm, sizeof(lbnm[0]) - 1);
		lbnm[i][sizeof(lbnm[0]) - 1] = 0;
		lblen[i] = 0;
		nlb++;
	}
	lblen[i] += d;
	return 0;
}

/* One segment: H-V routes measure both legs, diagonals measure isqrt --
 * the same integer honesty as a dimension label, and the same
 * arithmetic (velbase's isqrt) doing it. */
static long
seglen(x0, y0, x1, y1)
{
	long dx, dy;

	dx = (long)x1 - x0;	if ( dx < 0 ) dx = -dx;
	dy = (long)y1 - y0;	if ( dy < 0 ) dy = -dy;
	if ( dx == 0 )
		return dy;
	if ( dy == 0 )
		return dx;
	return isqrt(dx * dx + dy * dy);
}

/* a length in sheet units, "340 mm" or bare when U names no unit */
static
lfmt(d, buf)
long d;
char *buf;
{
	d *= unum;
	if ( uname[0] )
		sprintf(buf, "%ld %s", d, uname);
	else
		sprintf(buf, "%ld", d);
	return 0;
}

/* an area from TWICE its integer value: halved with the remainder shown
 * as ".5" -- fixed point, no floats anywhere in this program */
static
afmt(a2, buf)
long a2;
char *buf;
{
	a2 *= (long)unum * unum;
	sprintf(buf, "%ld%s", a2 / 2, (a2 & 1) ? ".5" : "");
	if ( uname[0] )
		sprintf(buf + strlen(buf), " %s2", uname);
	return 0;
}

static
dolen(sheet)
{
	register DOBJ *o;
	register int i;
	int k, r, nid, base;
	long d, a2;
	char nb[24];

	netbuild();
	for ( i = 0; i < nobj; i++ )
	{
		rid[i] = 0;
		seen[i] = 0;
	}
	nid = 0;			/* donet's numbering, exactly */
	for ( i = 0; i < np; i++ )
	{
		r = pnet[i];
		if ( r < 0 || seen[r] )
			continue;
		seen[r] = 1;
		rid[r] = ++nid;
	}
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( !xprn((int)o->o_layer) )
			continue;
		switch ( o->o_type )
		{
		case OT_WIRE:
			/* a wire is an H-then-V route: both legs count */
			d = seglen((int)o->o_x, (int)o->o_y,
				   (int)o->o_x2, (int)o->o_y) +
			    seglen((int)o->o_x2, (int)o->o_y,
				   (int)o->o_x2, (int)o->o_y2);
			r = nfind(i);
			for ( k = 0; k < nnames; k++ )
				if ( nroot[k] == r )
					break;
			if ( k < nnames )
				strcpy(nb, nnm[k]);
			else if ( rid[r] && nsheets > 1 )
				sprintf(nb, "NET-%d.%d", sheet, rid[r]);
			else if ( rid[r] )
				sprintf(nb, "NET-%d", rid[r]);
			else
				sprintf(nb, "layer %d unnamed runs",
					o->o_layer);
			lbadd(nb, d);
			break;

		case OT_CONN:
			if ( o->o_sym == CS_HV || o->o_sym == CS_HARROW )
				d = seglen((int)o->o_x, (int)o->o_y,
					   (int)o->o_x2, (int)o->o_y) +
				    seglen((int)o->o_x2, (int)o->o_y,
					   (int)o->o_x2, (int)o->o_y2);
			else
				d = seglen((int)o->o_x, (int)o->o_y,
					   (int)o->o_x2, (int)o->o_y2);
			sprintf(nb, "layer %d unnamed runs", o->o_layer);
			lbadd(nb, d);
			break;

		case OT_POLY:
			base = o->o_x2;
			if ( o->o_flags & OF_FILL )
			{
				/* integer shoelace over the grid points,
				 * the outline taken as closed (which is
				 * how the fill itself reads it) */
				a2 = 0;
				for ( k = 0; k < o->o_sym; k++ )
				{
					register int n;

					n = (k + 1) % o->o_sym;
					a2 += (long)ppool[base + 2*k] *
					        ppool[base + 2*n + 1] -
					      (long)ppool[base + 2*n] *
					        ppool[base + 2*k + 1];
				}
				if ( a2 < 0 )
					a2 = -a2;
				if ( nla < MAXLA )
					laa[nla++] = a2;
				else if ( !lafull )
				{
					fprintf(stderr,
			"velinfo: more than %d areas on the set; the rest dropped\n",
						MAXLA);
					lafull = 1;
				}
				break;
			}
			d = 0;
			for ( k = 1; k < o->o_sym; k++ )
				d += seglen(ppool[base + 2*k - 2],
					    ppool[base + 2*k - 1],
					    ppool[base + 2*k],
					    ppool[base + 2*k + 1]);
			sprintf(nb, "layer %d unnamed runs", o->o_layer);
			lbadd(nb, d);
			break;
		}
	}
	return 0;
}

static
dolenend()
{
	register int i;
	long t;
	char b[40];

	t = 0;
	for ( i = 0; i < nlb; i++ )
	{
		lfmt(lblen[i], b);
		printf("%s: %s\n", lbnm[i], b);
		t += lblen[i];
	}
	for ( i = 0; i < nla; i++ )
	{
		afmt(laa[i], b);
		printf("area %d: %s\n", i + 1, b);
	}
	if ( nlb )
	{
		lfmt(t, b);
		printf("total: %s\n", b);
	}
	if ( nla )
	{
		t = 0;
		for ( i = 0; i < nla; i++ )
			t += laa[i];
		afmt(t, b);
		printf("total area: %s\n", b);
	}
	return 0;
}

/* ================================================================== */
/* -where: the set-wide Find (VELLUM.md sec. 47)                      */
/*                                                                    */
/* The editor's Find answers "where is R17" one sheet at a time, by    */
/* eye and pan.  This is the same contract -- a case-blind substring   */
/* over designators, values, text and net names, the editor's objmatch */
/* re-stated over FILES -- with the shell as the reader.  Zero hits    */
/* exits 0 silently, so the INVERTED use is free:                      */
/*	velinfo -where TODO *.d                                        */
/* in a check: rule fails the build while a sheet still carries a TODO */
/* note -- the drawing set growing its own -Werror for prose.          */
/* ================================================================== */

/* case-blind substring, veldlg's cifind: folding by bit 5 folds letters
 * exactly, and the couple of symbol pairs it also equates are harmless
 * in a drawing search */
static
cifind(h, n)
char *h;
register char *n;
{
	register int j;
	int i;

	for ( i = 0; h[i]; i++ )
	{
		for ( j = 0; n[j]; j++ )
			if ( ((h[i + j] ^ n[j]) & 0xdf) != 0 )
				break;
		if ( n[j] == 0 )
			return 1;
	}
	return 0;
}

static
wmatch(o, pat)
register DOBJ *o;
char *pat;
{
	switch ( o->o_type )
	{
	case OT_SYM:
	case OT_NNAME:
		if ( cifind(o->o_name, pat) )
			return 1;
		if ( o->o_type == OT_NNAME )
			return 0;
	case OT_TEXT:
	case OT_SHAPE:
	case OT_DIM:
		return cifind(oval(o), pat);
	}
	return 0;
}

static
dowhere(fn, pat)
char *fn, *pat;
{
	register DOBJ *o;
	register int i;
	int n;
	char db[64], xb[40];

	n = 0;
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( !wmatch(o, pat) )
			continue;
		ddesc(o, tpool, db);
		ddet(o, tpool, xb);
		if ( o->o_type == OT_SYM && o->o_name[0] )
			printf("%s: Y %s at %d,%d%s\n", fn, db, o->o_x,
			       o->o_y, xb);
		else
			printf("%s: %s at %d,%d%s\n", fn, db, o->o_x,
			       o->o_y, xb);
		n++;
	}
	return n;
}

/* ================================================================== */
/* -symsheet: the library on paper (VELLUM.md sec. 49)                */
/*                                                                    */
/* Every stencil library deserves the reference card the vendor's data */
/* book would have.  It is .d OUT, so it prints on any of the five     */
/* backends and the shop's binder fills itself from make:              */
/*	velinfo -symsheet pid.sym | velplot -T ps -fit -               */
/* ================================================================== */

#define	SSCOL	6		/* symbols across the card                */

dosymsheet(path)
char *path;
{
	register SYMDEF *s;
	register int i, k;
	int cw, ch, cx, cy, w, h;
	register short *pp;

	nsym = 0;		/* ONE library: the card is ITS card */
	nlib = 0;
	if ( loadlib(path) < 0 )
	{
		fprintf(stderr, "velinfo: cannot load %s\n", path);
		return 1;
	}
	cw = ch = 0;
	for ( i = 0; i < nsym; i++ )
	{
		s = &symtab[i];
		w = (s->sy_x1 - s->sy_x0) / 4 + 1;
		h = (s->sy_y1 - s->sy_y0) / 4 + 1;
		if ( w > cw ) cw = w;
		if ( h > ch ) ch = h;
	}
	cw += 8;		/* room for the label and the pin letters */
	ch += 8;
	printf("vellum1\n");
	printf("T 2 2 s2 %s library\n", libname[0]);
	for ( i = 0; i < nsym; i++ )
	{
		s = &symtab[i];
		cx = 4 + (i % SSCOL) * cw - s->sy_x0 / 4;
		cy = 10 + (i / SSCOL) * ch - s->sy_y0 / 4;
		printf("Y %s %d %d 0 0 - -\n", s->sy_code, cx, cy);
		printf("T %d %d s0 /0.1 %s%s%s\n", cx + s->sy_x0 / 4,
		       cy + s->sy_y1 / 4 + 2, s->sy_code,
		       s->sy_pfx[0] ? " " : "", s->sy_pfx);
		if ( (pp = s->sy_pins) == (short *)0 )
			continue;
		for ( k = 0; k < pp[0]; k++ )
		{
			register int t;

			t = pintyp[PINSLOT(s, k)];
			if ( t == 0 )
				continue;
			printf("T %d %d s0 /0.1 %c\n",
			       cx + pp[1 + 2*k] / 4 + 1,
			       cy + pp[2 + 2*k] / 4, t);
		}
	}
	return 0;
}


/* ================================================================== */
/* -book: the set as a document (sec. 59)                             */
/* ================================================================== */

#define	BKTOP	10		/* first row, grid units                  */
#define	BKROW	3		/* row pitch                              */
#define	BKNUM	4		/* the number column                      */
#define	BKFILE	9		/* the file-name column                   */
#define	BKTTL	28		/* the title column                       */

static int	bkrow;

/* The sheet's own title: the first text object on the FRAME layer
 * (where dostamp puts $F, so a stamped set titles itself), else the
 * LARGEST text on the sheet, else the file name. */
static char *
booktitle(fn)
char *fn;
{
	register DOBJ *o;
	register int i;
	int best;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type == OT_TEXT && o->o_layer == 2 && oval(o)[0] )
			return oval(o);
	}
	best = -1;
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_TEXT || oval(o)[0] == 0 )
			continue;
		if ( best < 0 || o->o_rot > obj[best].o_rot )
			best = i;
	}
	return best >= 0 ? oval(&obj[best]) : fn;
}

/* one cell of the contents, copied clean: a '|' would split the text
 * object into a label block and the row would grow feet */
static
bkcell(gx, gy, s)
register char *s;
{
	char b[TVMAX];
	register int n;

	for ( n = 0; n < TVMAX - 1 && s[n]; n++ )
		b[n] = s[n] == '|' ? ' ' : s[n];
	b[n] = 0;
	printf("T %d %d s1 %s\n", gx, gy, b);
	return 0;
}

dobook(sheet, fn)
char *fn;
{
	int y;

	if ( sheet == 1 )
	{
		printf("vellum1\n");
		printf("T %d 3 s2 Contents\n", BKNUM);
		printf("L %d 8 %d 8\n", BKNUM, SHW - BKNUM);
		bkrow = 0;
	}
	y = BKTOP + bkrow * BKROW;
	bkrow++;
	printf("T %d %d s1 %d\n", BKNUM, y, sheet);
	bkcell(BKFILE, y, fn);
	bkcell(BKTTL, y, booktitle(fn));
	return 0;
}


/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

static
usage()
{
	fprintf(stderr, "usage: velinfo -len file.d ...\n");
	fprintf(stderr, "       velinfo -where STR file.d ...\n");
	fprintf(stderr, "       velinfo -symsheet lib.sym\n");
	fprintf(stderr, "       velinfo -book file.d ...\n");
	return 2;
}

main(argc, argv)
char **argv;
{
	register int i;
	register char *mode;
	char *pat;
	int first, hits;
	static int u0;
	static char un0[UNAMEL];

	velprog = "velinfo";
	mode = (char *)0;
	pat = "";
	hits = 0;
	for ( i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++ )
	{
		if ( mode != (char *)0 )
			exit(usage());
		mode = argv[i] + 1;
	}
	if ( mode == (char *)0 )
		exit(usage());
	/* -where takes its PATTERN first, then the sheets */
	if ( strcmp(mode, "where") == 0 )
	{
		if ( i >= argc )
			exit(usage());
		pat = argv[i++];
	}
	nsheets = argc - i;
	if ( nsheets < 1 )
		exit(usage());
	/* the card reads a symbol LIBRARY, not a drawing, and reads only
	 * the one it is given: the card is THAT library's card */
	if ( strcmp(mode, "symsheet") == 0 )
	{
		if ( nsheets != 1 )
			exit(usage());
		exit(dosymsheet(argv[i]));
	}
	if ( strcmp(mode, "len") != 0 && strcmp(mode, "where") != 0 &&
	     strcmp(mode, "book") != 0 )
		exit(usage());
	loadsyms();
	first = i;
	for ( ; i < argc; i++ )
	{
		if ( loadsheet(argv[i]) < 0 )
			exit(1);
		if ( strcmp(mode, "len") == 0 )
		{
			/* -len adds no millimetres to inches: a set whose
			 * sheets disagree about the unit is refused in one
			 * line, and that disagreement is already a velcheck
			 * finding.  Only -len: -where never multiplies by a
			 * unit, and -book does not measure. */
			if ( i == first )
			{
				u0 = unum;
				strcpy(un0, uname);
			}
			else if ( unum != u0 || strcmp(uname, un0) != 0 )
			{
				fprintf(stderr,
		"velinfo: %s: units U %d %s disagree with the set's\n",
					argv[i], unum, uname);
				exit(1);
			}
			dolen(i - first + 1);
		}
		else if ( strcmp(mode, "where") == 0 )
			hits += dowhere(argv[i], pat);
		else
			dobook(i - first + 1, argv[i]);
	}
	if ( strcmp(mode, "len") == 0 )
		exit(dolenend());
	if ( strcmp(mode, "where") == 0 )
		exit(hits > 254 ? 254 : hits);
	exit(0);
}
