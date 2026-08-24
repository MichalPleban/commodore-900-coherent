/*
 * velv6.c - Vellum's v6 CONTENT modes (VELLUM.md secs. 55, 56, 59):
 * the three verbs that close the loop between what a shop DRAWS and
 * what it draws WITH.
 *
 *	vellum -mksym CODE file.d >> lib.sym	a drawing -> a stencil
 *	vellum -symcheck lib.sym ...		prove the library
 *	vellum -book sheet*.d > contents.d	the set -> a contents page
 *
 * velxport only: the editor forwards the flag and links none of it.
 * All three stream their output -- no new pools (sec. 62) -- and
 * -mksym writes exactly the .sym text SymEdit's savelib writes, so a
 * GENERATED stencil is indistinguishable from a hand-cut one (rule 2).
 */
#include <stdio.h>
#include "vellum.h"

extern long	xsqrt();

/* ================================================================== */
/* -mksym: a drawing becomes a stencil (sec. 55)                      */
/* ================================================================== */

/* the command line's half, filled by velxport's option loop */
char	*mkcode;		/* the stencil's file CODE                */
char	*mkpfx = "-";		/* -pfx: the designator prefix            */
int	mkorgx, mkorgy;		/* -org x,y: the grid point that is 0,0   */
int	mkorgf;			/* ... was given                          */
int	mksc = 1;		/* -scale n: the sketch is n x oversize   */

#define	MKTYPE	16		/* -type NAME=t entries                   */
static char	mktnm[MKTYPE][NAMEL];
static char	mktty[MKTYPE];
static int	nmktype;

/* one -type NAME=t; returns 0 when the argument does not parse */
mktype(s)
register char *s;
{
	register char *p;
	register int n;

	for ( p = s; *p && *p != '='; p++ )
		;
	if ( *p != '=' || p == s || p[1] == 0 || p[2] != 0 ||
	     nmktype >= MKTYPE )
		return 0;
	for ( n = 0; n < NAMEL - 1 && s + n < p; n++ )
		mktnm[nmktype][n] = s[n];
	mktnm[nmktype][n] = 0;
	mktty[nmktype] = p[1];
	nmktype++;
	return 1;
}

static
mktlook(nm)
char *nm;
{
	register int i;

	for ( i = 0; i < nmktype; i++ )
		if ( strcmp(mktnm[i], nm) == 0 )
			return mktty[i];
	return 0;
}

/* grid -> quarter-grid.  A stencil is in quarter units and a drawing
 * in whole ones, so the conversion is x4 and every pin lands on a
 * multiple of 4 -- exactly the constraint loadlib wants.  -scale n
 * divides instead, for a stencil sketched n x oversize so its detail
 * was drawable at all; at -scale 4 a drawing unit IS a quarter unit. */
static
mqx(g)
{
	register int v;

	v = (g - mkorgx) * 4;
	return v >= 0 ? (v + mksc / 2) / mksc : -((-v + mksc / 2) / mksc);
}

static
mqy(g)
{
	register int v;

	v = (g - mkorgy) * 4;
	return v >= 0 ? (v + mksc / 2) / mksc : -((-v + mksc / 2) / mksc);
}

static
mqr(g)				/* a radius: a length, not a position     */
{
	register long v;

	v = (long)g * 4;
	return (int)((v + mksc / 2) / mksc);
}

static
mkseg(x0, y0, x1, y1)
{
	printf("s %d %d %d %d\n", mqx(x0), mqy(y0), mqx(x1), mqy(y1));
	return 0;
}

/* Everything a stencil has no form for is SKIPPED AND COUNTED, on
 * stderr -- veldxf's border lesson for the third time: a silently
 * half-converted stencil is a part that draws wrong forever. */
#define	MKDROP	5
static char	*mkdnm[MKDROP] = {
	"shapes", "connectors", "dimensions", "symbols", "long texts"
};
static int	mkdct[MKDROP];

domksym(fn)
char *fn;
{
	register DOBJ *o;
	register int i, k;
	int gx0, gy0, gx1, gy1, npin, got;
	char *nm;
	long r;

	for ( i = 0; i < MKDROP; i++ )
		mkdct[i] = 0;
	/* the origin: -org, else the FIRST PIN (a stencil's origin is
	 * where it snaps), else the printable drawing's top-left */
	if ( !mkorgf )
	{
		got = 0;
		for ( i = 0; i < nobj; i++ )
			if ( obj[i].o_type == OT_NNAME &&
			     xprn((int)obj[i].o_layer) )
			{
				mkorgx = obj[i].o_x;
				mkorgy = obj[i].o_y;
				got = 1;
				break;
			}
		if ( !got )
		{
			if ( xextent(&gx0, &gy0, &gx1, &gy1) )
			{
				mkorgx = gx0;
				mkorgy = gy0;
			}
			else
				mkorgx = mkorgy = 0;
		}
	}
	printf("symbol %s %s\n", mkcode, mkpfx[0] ? mkpfx : "-");
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( !xprn((int)o->o_layer) )
			continue;
		switch ( o->o_type )
		{
		case OT_LINE:
		case OT_WIRE:
			mkseg((int)o->o_x, (int)o->o_y, (int)o->o_x2,
			      (int)o->o_y2);
			break;

		case OT_BOX:		/* four honest lines beat a new op */
			mkseg((int)o->o_x, (int)o->o_y, (int)o->o_x2,
			      (int)o->o_y);
			mkseg((int)o->o_x2, (int)o->o_y, (int)o->o_x2,
			      (int)o->o_y2);
			mkseg((int)o->o_x2, (int)o->o_y2, (int)o->o_x,
			      (int)o->o_y2);
			mkseg((int)o->o_x, (int)o->o_y2, (int)o->o_x,
			      (int)o->o_y);
			break;

		case OT_POLY:
			for ( k = 1; k < o->o_sym; k++ )
				mkseg((int)ppool[o->o_x2 + 2*k - 2],
				      (int)ppool[o->o_x2 + 2*k - 1],
				      (int)ppool[o->o_x2 + 2*k],
				      (int)ppool[o->o_x2 + 2*k + 1]);
			break;

		case OT_CIRC:
			r = xsqrt((long)(o->o_x2 - o->o_x) *
				  (o->o_x2 - o->o_x) +
				  (long)(o->o_y2 - o->o_y) *
				  (o->o_y2 - o->o_y));
			printf("c %d %d %d\n", mqx((int)o->o_x),
			       mqy((int)o->o_y), mqr((int)r));
			break;

		case OT_ARC:
			printf("a %d %d %d %d %d\n", mqx((int)o->o_x),
			       mqy((int)o->o_y), mqr((int)o->o_x2),
			       (int)OA0(o), (int)OA1(o));
			break;

		case OT_TEXT:	/* the stencil font is one glyph per op */
			if ( oval(o)[0] && oval(o)[1] == 0 )
				printf("t %d %d %c\n", mqx((int)o->o_x),
				       mqy((int)o->o_y), oval(o)[0]);
			else
				mkdct[4]++;
			break;

		case OT_SHAPE:	mkdct[0]++;	break;
		case OT_CONN:	mkdct[1]++;	break;
		case OT_DIM:	mkdct[2]++;	break;
		case OT_SYM:	mkdct[3]++;	break;
		}
	}
	/* Pins come from the N net-name markers, and the net name becomes
	 * the PIN name: an N at a point already means "this point is a
	 * terminal called VBUS".  Pin ORDER is their order in the file --
	 * z-order, which Front/Back edits and -spice reads.  Pin TYPES
	 * come from -type, never from the drawing (sec. 61). */
	npin = 0;
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_NNAME || !xprn((int)o->o_layer) )
			continue;
		k = mktlook(o->o_name);
		/* the .sym format spells "no name" as "-", and so does a
		 * sketch: an N marker called "-" is a terminal with no
		 * name, which is what an unnamed hand-cut pin is */
		nm = (o->o_name[0] == '-' && o->o_name[1] == 0) ? ""
								: o->o_name;
		printf("p %d %d", mqx((int)o->o_x), mqy((int)o->o_y));
		if ( nm[0] || k )
			printf(" %s", nm[0] ? nm : "-");
		if ( k )
			printf(" %c", k);
		printf("\n");
		npin++;
	}
	printf("end\n");
	got = 0;
	for ( i = 0; i < MKDROP; i++ )
		if ( mkdct[i] )
			fprintf(stderr, "%s %d %s", got++ ? "," :
				"vellum: -mksym: dropped:", mkdct[i],
				mkdnm[i]);
	if ( got )
		fprintf(stderr, "\n");
	if ( npin == 0 )
		fprintf(stderr,
	"vellum: -mksym: %s: no N markers, so the stencil has no pins\n",
			fn);
	return 0;
}

/* ================================================================== */
/* -symcheck: prove the library (sec. 56)                             */
/* ================================================================== */

int	scn;			/* finding count = the exit status        */
static char	*scfile;

/* One finding: "lib.sym: message"; a/b ride printf %s/%d holes -- the
 * -check shape exactly, so make(1) gates on it the same way. */
static
sc(msg, a, b)
char *msg, *a, *b;
{
	printf("%s: ", scfile);
	printf(msg, a, b);
	printf("\n");
	scn++;
	return 0;
}

static char *
scpnm(s, k)
SYMDEF *s;
{
	register int slot;

	slot = PINSLOT(s, k);
	return pinnm[slot] ? &pnmpool[pinnm[slot]] : (char *)0;
}

/* the pin-name repeat check, over one symbol's own pins */
static
scnames(s)
register SYMDEF *s;
{
	register short *pp;
	register int k, j;
	char *a, *b;

	if ( (pp = s->sy_pins) == (short *)0 )
		return 0;
	for ( k = 1; k < pp[0]; k++ )
	{
		if ( (a = scpnm(s, k)) == (char *)0 )
			continue;
		for ( j = 0; j < k; j++ )
		{
			b = scpnm(s, j);
			if ( b != (char *)0 && strcmp(a, b) == 0 )
			{
				printf("%s: code %s: pin name %s repeats\n",
				       scfile, s->sy_code, a);
				scn++;
				break;
			}
		}
	}
	return 0;
}

/* one library file, already loaded: symbols n0..nsym-1 are its own */
static
scsyms(n0)
{
	register SYMDEF *s;
	register short *pp;
	register int i, k;
	int typed, sx, sy;
	char cb[32];

	/* is this library TYPED at all?  v4.3's rule stands: a library
	 * that types NOTHING is silent, one that types most of its pins
	 * and forgets one is wrong, and this is where that gets said. */
	typed = 0;
	for ( i = n0; i < nsym; i++ )
	{
		s = &symtab[i];
		if ( (pp = s->sy_pins) == (short *)0 )
			continue;
		for ( k = 0; k < pp[0]; k++ )
			if ( pintyp[PINSLOT(s, k)] )
				typed = 1;
	}
	for ( i = n0; i < nsym; i++ )
	{
		s = &symtab[i];
		/* a code defined twice -- in this file, and across the
		 * libraries already loaded: loadlib keeps the FIRST, so
		 * the load ORDER decides which stencil a drawing gets,
		 * and nothing tells anyone */
		for ( k = 0; k < i; k++ )
			if ( strcmp(symtab[k].sy_code, s->sy_code) == 0 )
			{
				if ( k >= n0 )
					sc("code %s defined twice",
					   s->sy_code, 0);
				else
				{
					printf(
			"%s: code %s defined in both %s and %s\n",
					  scfile, s->sy_code,
					  libname[symtab[k].sy_lib],
					  libname[s->sy_lib]);
					scn++;
				}
				break;
			}
		if ( s->sy_ops[0] == SEND )
			sc("code %s: no geometry", s->sy_code, 0);
		pp = s->sy_pins;
		if ( pp == (short *)0 || pp[0] == 0 )
		{
			if ( s->sy_pfx[0] )
				sc("code %s: designator prefix but no pins",
				   s->sy_code, 0);
			continue;
		}
		if ( s->sy_nfile > pp[0] )
		{
			sprintf(cb, "%s: %d", s->sy_code, (int)s->sy_nfile);
			sc("code %s pins, the loader keeps %d", cb,
			   pp[0]);
		}
		for ( k = 0; k < pp[0]; k++ )
		{
			sx = pp[1 + 2*k];
			sy = pp[2 + 2*k];
			if ( (sx & 3) || (sy & 3) )
			{
				printf(
			"%s: code %s: pin %d at %d,%d is not on a whole unit\n",
				       scfile, s->sy_code, k + 1, sx, sy);
				scn++;
			}
			if ( typed && pintyp[PINSLOT(s, k)] == 0 )
			{
				sprintf(cb, "%s: pin %d", s->sy_code, k + 1);
				sc("code %s untyped in a typed library",
				   cb, 0);
			}
		}
		scnames(s);
	}
	return 0;
}

dosymcheck(path)
char *path;
{
	int n0;

	scfile = path;
	n0 = nsym;
	libdrop = 0;
	libpool = "";
	if ( loadlib(path) < 0 && nsym == n0 )
	{
		sc("cannot load it", 0, 0);
		return 0;
	}
	/* loadlib drops SILENTLY when a pool fills or a code repeats, and
	 * nothing tells anyone -- that is the class of failure this
	 * project exists to refuse (sec. 56). */
	if ( libdrop )
		sc("library exceeds %s: %d symbols dropped at load",
		   libpool, libdrop);
	scsyms(n0);
	return 0;
}

/* after the last library: the exit status, capped like -check */
dosymcheckend()
{
	if ( scn == 0 )
		fprintf(stderr, "vellum: symcheck clean\n");
	return scn > 254 ? 254 : scn;
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
