/*
 * velv5.c - the ASKING modes (VELLUM.md secs. 45-49): the drawing set
 * stops being something you only draw and print, and becomes something
 * you ASK.
 *
 *	vellum -diff   old.d new.d	what changed
 *	vellum -diff -mark old.d new.d	... as a MARKED-UP DRAWING
 *	vellum -len    f.d ...		the takeoff: run lengths, areas
 *	vellum -where  STR f.d ...	the set-wide Find
 *	vellum -spice  f.d ...		a SPICE 2G6 deck
 *	vellum -symsheet lib.sym	a library's reference card
 *
 * Every one of them is an EXPORTER mode, so the editor pays ZERO bytes
 * (sec. 44 rule 1) and nothing here needs a window.  The format is not
 * merely unbroken but UNTOUCHED (rule 2): these modes only READ it, and
 * the one that writes (-diff -mark) writes ordinary objects any v1.4
 * binary displays.
 *
 * This is a unit of its OWN rather than more velport.c: velport is a
 * module that has already grown five backends, and the assembler's
 * fix-up tables are a wall we have walked into before.
 */
#include <stdio.h>
#include "vellum.h"

/* velport.c: the shared connectivity (netbuild) and the sheet count */
extern int	nsheets;
extern short	pobj[], ppin[], pnet[];
extern int	np;
extern char	nnm[][10];
extern short	nroot[];
extern int	nnames;
extern int	netbuild(), nfind();

/* ================================================================== */
/* The file name "-" is STDIN (sec. 49): it is what makes the export   */
/* modes composable with EACH OTHER --                                 */
/*	vellum -symsheet pid.sym | vellum -ps -fit -                    */
/* puts a generated drawing on paper with no temp file anywhere.  It   */
/* is the exporter's file loop that changes, not velfile's loadfile:   */
/* the editor is closed (sec. 44 rule 1) down to the byte, and the     */
/* format unit already exposes the parser this needs -- parseobj has   */
/* been "shared by Open, clipboard Paste and any script that feeds     */
/* lines in" since v1.3.                                               */
/* ================================================================== */

loadstdin()
{
	register int i;
	char lb[220];
	int e;

	selclear();
	nobj = 0;
	ppuse = 0;
	tpuse = 0;
	unum = 1;
	uname[0] = 0;
	parsereset();
	while ( fgets(lb, sizeof(lb), stdin) != 0 && nobj < MAXOBJ )
		parseobj(lb);
	for ( i = 0; i < nobj; i++ )
		if ( obj[i].o_type == OT_CONN )
			for ( e = 0; e < 2; e++ )
				if ( ACOBJ(&obj[i], e) == -2 )
					resolveatt(i, e, 0);
	modified = 0;
	voxg = voyg = 0;
	rejunc();
	uvalid = 0;
	return 0;
}

/* ================================================================== */
/* The SECOND revision (sec. 45): a diff needs both drawings in memory */
/* at once, so the first one is copied here whole -- objects and both  */
/* pools.  This is velxport-only large-model data (sec. 13.1: the      */
/* exporter's data is the cheap wall, and it still is); the editor     */
/* never links this unit and never pays for it.                        */
/* ================================================================== */

DOBJ	dobj[MAXOBJ];
int	dnobj;
short	dppool[PPOOL];
int	dppuse;
char	dtpool[TPOOL];
int	dtpuse;
int	dunum;			/* the old revision's U header, so a       */
char	duname[UNAMEL];		/* re-scaled sheet reports as one change   */

/* the live drawing -> the shadow */
dsave()
{
	register int i;

	for ( i = 0; i < nobj; i++ )
		dobj[i] = obj[i];
	dnobj = nobj;
	dunum = unum;
	strcpy(duname, uname);
	for ( i = 0; i < ppuse; i++ )
		dppool[i] = ppool[i];
	dppuse = ppuse;
	for ( i = 0; i < tpuse; i++ )
		dtpool[i] = tpool[i];
	dtpuse = tpuse;
	return 0;
}

/* ... and back, so fmtobj can serialize the OLD revision too */
static
dload()
{
	register int i;

	for ( i = 0; i < dnobj; i++ )
		obj[i] = dobj[i];
	nobj = dnobj;
	for ( i = 0; i < dppuse; i++ )
		ppool[i] = dppool[i];
	ppuse = dppuse;
	for ( i = 0; i < dtpuse; i++ )
		tpool[i] = dtpool[i];
	tpuse = dtpuse;
	return 0;
}

/* ================================================================== */
/* -diff: what changed (VELLUM.md sec. 45)                            */
/*                                                                    */
/* Matching is by IDENTITY, not by line: diff(1) on two .d files       */
/* drowns in reordering, because the line ORDER is the z-order, not    */
/* the content.  Symbols match by DESIGNATOR first (R7 is R7 wherever  */
/* it moved in the list), everything else by exact (type, geometry,    */
/* value) key equality -- no fuzzy scoring, no tolerance: an object    */
/* that moved one unit is a remove plus an add, and that is the honest */
/* answer on a grid.                                                   */
/* ================================================================== */

#define	DV_SAME	0		/* matched, identical                     */
#define	DV_ADD	1		/* new only / old only                    */
#define	DV_CHG	2		/* matched at the same place, differs     */
#define	DV_MOV	3		/* designator matched, position differs   */

static char	vold[MAXOBJ], vnew[MAXOBJ];
static short	pold[MAXOBJ];	/* old index -> its new partner, -1 = none */

/* net root -> the sheet-local net number donet would print (-len and
 * -spice number nets the same way -net does, from the same walk) */
static short	rid[MAXOBJ];
static char	seen[MAXOBJ];

/* Geometry equality.  A polyline's o_x2 is a POOL INDEX, meaningless
 * across two loads, so its points are compared instead. */
static
dgeoeq(a, b)
register DOBJ *a, *b;
{
	register int k;

	if ( a->o_type != b->o_type )
		return 0;
	if ( a->o_type == OT_POLY )
	{
		if ( a->o_sym != b->o_sym )
			return 0;
		for ( k = 0; k < 2 * a->o_sym; k++ )
			if ( dppool[a->o_x2 + k] != ppool[b->o_x2 + k] )
				return 0;
		return a->o_x == b->o_x && a->o_y == b->o_y;
	}
	return a->o_x == b->o_x && a->o_y == b->o_y &&
	       a->o_x2 == b->o_x2 && a->o_y2 == b->o_y2;
}

/* Everything else about the object: kind, orientation, style, layer,
 * name and value.  A CONNECTOR's o_name holds ATTACHMENT indices, which
 * shift with the object list and are re-derived on load -- comparing
 * them would invent differences, so they are left out. */
static
dvaleq(a, b)
register DOBJ *a, *b;
{
	if ( a->o_type != OT_POLY && a->o_sym != b->o_sym )
		return 0;
	if ( a->o_rot != b->o_rot || a->o_mir != b->o_mir )
		return 0;
	if ( a->o_flags != b->o_flags || a->o_layer != b->o_layer )
		return 0;
	if ( a->o_type == OT_ARC )
		return OA0(a) == OA0(b) && OA1(a) == OA1(b);
	if ( a->o_type == OT_SYM || a->o_type == OT_NNAME )
		if ( strcmp(a->o_name, b->o_name) != 0 )
			return 0;
	return strcmp(ovalp(a, dtpool), ovalp(b, tpool)) == 0;
}

/* One object, named the way the report names it. */
static
ddesc(o, tp, buf)
register DOBJ *o;
char *tp, *buf;
{
	static char lt[] = " YWLBCTSKPAND";

	switch ( o->o_type )
	{
	case OT_SYM:
		if ( o->o_name[0] )
			strcpy(buf, o->o_name);
		else
			sprintf(buf, "Y %s", symtab[o->o_sym].sy_code);
		break;
	case OT_NNAME:
		sprintf(buf, "N %s", o->o_name);
		break;
	case OT_TEXT:
	case OT_SHAPE:
	case OT_DIM:
		sprintf(buf, "%c \"%.40s\"", lt[o->o_type], ovalp(o, tp));
		break;
	case OT_POLY:
		sprintf(buf, "P %d pts at %d,%d", o->o_sym, o->o_x, o->o_y);
		break;
	case OT_ARC:
		sprintf(buf, "A %d,%d r%d", o->o_x, o->o_y, o->o_x2);
		break;
	default:
		sprintf(buf, "%c %d,%d-%d,%d", lt[o->o_type], o->o_x,
			o->o_y, o->o_x2, o->o_y2);
		break;
	}
	return 0;
}

/* "(R 47k)" -- the part detail an added or removed symbol carries. */
static
ddet(o, tp, buf)
register DOBJ *o;
char *tp, *buf;
{
	register char *v;

	buf[0] = 0;
	if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
		return 0;	/* ddesc already said "Y GND": no echo */
	v = ovalp(o, tp);
	if ( v[0] )
		sprintf(buf, " (%s %.16s)", symtab[o->o_sym].sy_code, v);
	else
		sprintf(buf, " (%s)", symtab[o->o_sym].sy_code);
	return 0;
}

/* Old object i is a designated symbol: its partner in the new drawing,
 * or -1.  Designators are the identity, so this pass runs first and
 * alone -- a symbol that lost its designator is a remove plus an add. */
static
dsymmatch(i)
{
	register int j;
	register DOBJ *a;

	a = &dobj[i];
	for ( j = 0; j < nobj; j++ )
	{
		if ( vnew[j] != DV_ADD || obj[j].o_type != OT_SYM )
			continue;
		if ( obj[j].o_name[0] &&
		     strcmp(a->o_name, obj[j].o_name) == 0 )
			return j;
	}
	return -1;
}

/* The report, one line each, the -check shape: "<file>: <what>". */
static int	dchg;		/* the change count = the exit status     */
static char	*dfile;

static
dsay(o, tp, verb)
DOBJ *o;
char *tp, *verb;
{
	char db[64], xb[40];

	ddesc(o, tp, db);
	ddet(o, tp, xb);
	printf("%s: %s %s%s\n", dfile, db, verb, xb);
	return 0;
}

dodiff(mark)
{
	register DOBJ *a, *b;
	register int i, j;
	int dunit;
	char db[64];

	for ( i = 0; i < nobj; i++ )
		vnew[i] = DV_ADD;
	for ( i = 0; i < dnobj; i++ )
	{
		vold[i] = DV_ADD;
		pold[i] = -1;
	}
	/* pass 1: designators.  A designator match with a different
	 * position reports as a MOVE -- the one concession to
	 * readability, and it is still exact. */
	for ( i = 0; i < dnobj; i++ )
	{
		a = &dobj[i];
		if ( a->o_type != OT_SYM || a->o_name[0] == 0 )
			continue;
		if ( (j = dsymmatch(i)) < 0 )
			continue;
		b = &obj[j];
		pold[i] = j;
		if ( a->o_x != b->o_x || a->o_y != b->o_y )
			vold[i] = vnew[j] = DV_MOV;
		else if ( !dvaleq(a, b) )
			vold[i] = vnew[j] = DV_CHG;
		else
			vold[i] = vnew[j] = DV_SAME;
	}
	/* pass 2: everything else, by EXACT key (type, geometry, value) */
	for ( i = 0; i < dnobj; i++ )
	{
		a = &dobj[i];
		if ( vold[i] != DV_ADD )
			continue;
		if ( a->o_type == OT_SYM && a->o_name[0] )
			continue;
		for ( j = 0; j < nobj; j++ )
		{
			b = &obj[j];
			if ( vnew[j] != DV_ADD )
				continue;
			if ( b->o_type == OT_SYM && b->o_name[0] )
				continue;
			if ( dgeoeq(a, b) && dvaleq(a, b) )
			{
				vold[i] = vnew[j] = DV_SAME;
				pold[i] = j;
				break;
			}
		}
	}
	/* pass 3: what is left, reconciled by GEOMETRY alone -- an object
	 * still standing exactly where it stood, saying something else,
	 * is a CHANGE, not a removal followed by an unrelated addition */
	for ( i = 0; i < dnobj; i++ )
	{
		a = &dobj[i];
		if ( vold[i] != DV_ADD )
			continue;
		if ( a->o_type == OT_SYM && a->o_name[0] )
			continue;
		for ( j = 0; j < nobj; j++ )
		{
			b = &obj[j];
			if ( vnew[j] != DV_ADD )
				continue;
			if ( b->o_type == OT_SYM && b->o_name[0] )
				continue;
			if ( dgeoeq(a, b) )
			{
				vold[i] = vnew[j] = DV_CHG;
				pold[i] = j;
				break;
			}
		}
	}
	/* the change COUNT is the exit status in both forms: a Makefile
	 * gates a release print on "no drift since the approved
	 * revision" exactly the way it gates on -check */
	dunit = dunum != unum || strcmp(duname, uname) != 0;
	dchg += dunit;
	for ( i = 0; i < dnobj; i++ )
		if ( vold[i] != DV_SAME )
			dchg += (vold[i] == DV_MOV &&
				 !dvaleq(&dobj[i], &obj[pold[i]])) ? 2 : 1;
	for ( j = 0; j < nobj; j++ )
		if ( vnew[j] == DV_ADD )
			dchg++;
	if ( mark )
		return domark();
	/* the report, in OLD order then new additions: a review reads
	 * down the drawing it already knows */
	if ( dunit )
		printf("%s: sheet units U %d %s -> U %d %s\n", dfile,
		       dunum, duname[0] ? duname : "-",
		       unum, uname[0] ? uname : "-");
	for ( i = 0; i < dnobj; i++ )
	{
		a = &dobj[i];
		switch ( vold[i] )
		{
		case DV_MOV:
			b = &obj[pold[i]];
			ddesc(a, dtpool, db);
			printf("%s: %s moved %d,%d -> %d,%d\n", dfile, db,
			       a->o_x, a->o_y, b->o_x, b->o_y);
			if ( !dvaleq(a, b) )
				dsay(b, tpool, "changed");
			break;
		case DV_CHG:
			dsay(&obj[pold[i]], tpool, "changed");
			break;
		case DV_ADD:
			dsay(a, dtpool, "removed");
			break;
		}
	}
	for ( j = 0; j < nobj; j++ )
		if ( vnew[j] == DV_ADD )
			dsay(&obj[j], tpool, "added");
	return 0;
}

/* ------------------------------------------------------------------ */
/* -diff -mark: the answer AS A DRAWING.  Everything unchanged on its  */
/* own layers, ADDED (and changed-to) objects BOLD, REMOVED (and       */
/* changed-from) objects re-emitted DASHED on the annotation layer.    */
/* No new viewer and no new UI: the markup is ordinary objects, so     */
/* velprev shows it, -print prints it and the editor opens it -- the   */
/* change review on the wall next to the old print, produced by make.  */
/* ------------------------------------------------------------------ */

static
domark()
{
	register int i;
	int n, j;
	char lb[220];

	printf("vellum1\n");
	if ( unum != 1 || uname[0] )
		printf("U %d %s\n", unum, uname[0] ? uname : "-");
	for ( i = 0; i < nobj; i++ )
		if ( vnew[i] != DV_SAME )
			obj[i].o_flags |= OF_BOLD;
	for ( i = 0; i < nobj; i++ )	/* writefile's shape, on stdout */
	{
		if ( obj[i].o_grp &&
		     (i == 0 || obj[i - 1].o_grp != obj[i].o_grp) )
		{
			n = 0;
			for ( j = i; j < nobj &&
				     obj[j].o_grp == obj[i].o_grp; j++ )
				n++;
			printf("G %d\n", n);
		}
		fmtobj(i, lb);
		if ( lb[0] )
			printf("%s\n", lb);
	}
	/* the OLD revision back into the live table, so fmtobj -- the one
	 * serializer, the one that stays right -- writes the removals too */
	dload();
	for ( i = 0; i < nobj; i++ )
	{
		if ( vold[i] == DV_SAME )
			continue;	/* moved, changed or removed */
		obj[i].o_layer = 1;
		obj[i].o_flags = (obj[i].o_flags & ~OF_STYLE) | OF_DASH;
		obj[i].o_grp = 0;
		fmtobj(i, lb);
		if ( lb[0] )
			printf("%s\n", lb);
	}
	return 0;
}

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
			"vellum: more than %d runs to total; %s dropped\n",
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
			"vellum: more than %d areas on the set; the rest dropped\n",
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
/*	vellum -where TODO *.d                                         */
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
/* -spice: the simulator crossing (VELLUM.md sec. 48)                 */
/*                                                                    */
/* The netlist discipline proves connectivity and (since v4.3) sense;  */
/* the department VAX runs SPICE, and between them stands a clerk      */
/* retyping R values.  The same union-find as -net, emitted as SPICE   */
/* 2G6 cards.  Everything the subset cannot say is SKIPPED AND         */
/* COUNTED, veldxf's border lesson verbatim: a silent partial netlist  */
/* is how simulations lie.  Models are the user's problem by design -- */
/* the deck ends .END and a wrapper file .INCLUDEs it beside the       */
/* .MODEL lines; make composes, as always.                             */
/* ================================================================== */

#define	MAXSN	80		/* distinct SPICE nodes in the set        */

static char	snky[MAXSN][12];	/* net name, or "#<sheet>.<nid>"  */
static short	snnum[MAXSN];
static int	nsn, snext;

static
snode(key)
char *key;
{
	register int i;

	for ( i = 0; i < nsn; i++ )
		if ( strcmp(snky[i], key) == 0 )
			return snnum[i];
	if ( nsn >= MAXSN )
		return -1;
	strncpy(snky[nsn], key, sizeof(snky[0]) - 1);
	snky[nsn][sizeof(snky[0]) - 1] = 0;
	/* the simulator's law: ground is node 0 */
	if ( ((key[0] ^ 'G') & 0xdf) == 0 && ((key[1] ^ 'N') & 0xdf) == 0 &&
	     ((key[2] ^ 'D') & 0xdf) == 0 && key[3] == 0 )
		snnum[nsn] = 0;
	else if ( key[0] == '0' && key[1] == 0 )
		snnum[nsn] = 0;
	else
		snnum[nsn] = ++snext;
	return snnum[nsn++];
}

/* the dropped-component accounting, veldxf's drop() */
#define	MAXSDROP 16
static char	sdnm[MAXSDROP][10];
static int	sdct[MAXSDROP];
static int	nsdrop;

static
sdrop(nm)
char *nm;
{
	register int i;

	for ( i = 0; i < nsdrop; i++ )
		if ( strcmp(sdnm[i], nm) == 0 )
		{
			sdct[i]++;
			return 0;
		}
	if ( nsdrop < MAXSDROP )
	{
		strncpy(sdnm[nsdrop], nm, sizeof(sdnm[0]) - 1);
		sdnm[nsdrop][sizeof(sdnm[0]) - 1] = 0;
		sdct[nsdrop] = 1;
		nsdrop++;
	}
	return 0;
}

/* The engineering suffix a drawing writes -> the one SPICE reads.  A
 * SPICE deck spells mega MEG and milli M; a schematic writes both as M
 * and means mega, so an upper-case M is MEG here and a lower-case one
 * milli, which is what every draughtsman already assumes.  0 = the
 * character is not a suffix at all; "" = it is (ohms' R), and emits
 * nothing. */
static char *
sufmap(c)
{
	switch ( c )
	{
	case 'R':	case 'r':	return "";
	case 'k':	case 'K':	return "K";
	case 'M':			return "MEG";
	case 'm':			return "M";
	case 'u':	case 'U':	return "U";
	case 'n':	case 'N':	return "N";
	case 'p':	case 'P':	return "P";
	case 'G':	case 'g':	return "G";
	}
	return (char *)0;
}

/* "4k7", "100n", "2.2M", "0.1u", "47" -> the value SPICE understands.
 * An integer suffix parser, fixed point x1000 like veldxf's: the suffix
 * letter doubles as the decimal point where the drawing writes it that
 * way (the R notation the whole trade uses), and rides after the point
 * where it does not.  0 when there is no number there at all. */
static
spval(s, buf)
register char *s;
char *buf;
{
	long v;
	int i, f;
	char *sfx;

	while ( *s == ' ' || *s == '\t' )
		s++;
	if ( *s < '0' || *s > '9' )
		return 0;
	v = 0;
	while ( *s >= '0' && *s <= '9' )
		v = v * 10 + (*s++ - '0');
	v *= 1000;
	if ( *s == '.' )		/* "2.2M": point, then the suffix */
	{
		s++;
		for ( i = 100; i >= 1 && *s >= '0' && *s <= '9'; i /= 10 )
			v += (long)(*s++ - '0') * i;
		sfx = sufmap(*s);
	}
	else if ( (sfx = sufmap(*s)) != (char *)0 )
	{				/* "4k7": the suffix IS the point  */
		s++;
		for ( i = 100; i >= 1 && *s >= '0' && *s <= '9'; i /= 10 )
			v += (long)(*s++ - '0') * i;
	}
	if ( sfx == (char *)0 )
		sfx = "";
	sprintf(buf, "%ld", v / 1000);
	if ( (f = (int)(v % 1000)) != 0 )
	{
		char fb[8];

		sprintf(fb, "%03d", f);
		for ( i = 2; i > 0 && fb[i] == '0'; i-- )
			fb[i] = 0;
		sprintf(buf + strlen(buf), ".%s", fb);
	}
	strcat(buf, sfx);
	return 1;
}

/* The SPICE node of pin k of symbol object i, or -1 when the pin hangs
 * (which drops the component: a card with a missing node is a lie). */
static
spnode(i, k, sheet, rid)
short *rid;
{
	register int j, m;
	int r;
	char key[16];

	for ( j = 0; j < np; j++ )
		if ( pobj[j] == i && ppin[j] == k )
			break;
	if ( j >= np || pnet[j] < 0 )
		return -1;
	r = pnet[j];
	for ( m = 0; m < nnames; m++ )
		if ( nroot[m] == r )
			break;
	if ( m < nnames )
		strcpy(key, nnm[m]);
	else if ( rid[r] )
		sprintf(key, "#%d.%d", sheet, rid[r]);
	else
		return -1;
	return snode(key);
}

/* Pin ORDER is the library's, except where the library NAMES its pins:
 * SPICE wants a transistor as C B E and a diode as anode-cathode, and a
 * .sym that says B/C/E (or A/K) says which drawn point is which.  An
 * unnamed library falls back to pin order, and the manual says so. */
static
sporder(s, dev, ord, n)
register SYMDEF *s;
int *ord;
{
	register int k, e;
	int try[4];
	char got[4];
	static char qn[] = "CBE";
	static char dn[] = "AK";
	register char *want;

	for ( k = 0; k < n; k++ )
	{
		ord[k] = k;
		got[k] = 0;
	}
	want = dev == 'Q' ? qn : dn;
	for ( k = 0; k < n; k++ )
	{
		register int slot;

		slot = pinnm[PINSLOT(s, k)];
		if ( slot == 0 || pnmpool[slot + 1] != 0 )
			return 0;		/* not a one-letter name */
		for ( e = 0; e < n; e++ )
			if ( pnmpool[slot] == want[e] )
			{
				got[e] = 1;
				try[e] = k;
				break;
			}
		if ( e == n )
			return 0;
	}
	for ( k = 0; k < n; k++ )
		if ( !got[k] )
			return 0;
	for ( k = 0; k < n; k++ )
		ord[k] = try[k];
	return 1;
}

static
dospice(sheet, fn)
char *fn;
{
	register DOBJ *o;
	register int i, k;
	int r, nid, want, npin, dev, ok;
	int nd[4], ord[4];
	char vb[24], card[120];
	register char *pfx;

	netbuild();
	for ( i = 0; i < nobj; i++ )
	{
		rid[i] = 0;
		seen[i] = 0;
	}
	nid = 0;
	for ( i = 0; i < np; i++ )
	{
		r = pnet[i];
		if ( r < 0 || seen[r] )
			continue;
		seen[r] = 1;
		rid[r] = ++nid;
	}
	printf("* sheet %s\n", fn);
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
			continue;
		pfx = symtab[o->o_sym].sy_pfx;
		dev = pfx[0];
		if ( pfx[1] != 0 || (dev != 'R' && dev != 'C' &&
		     dev != 'L' && dev != 'D' && dev != 'Q') )
		{
			sdrop(symtab[o->o_sym].sy_code);
			continue;
		}
		want = dev == 'Q' ? 3 : 2;
		npin = symtab[o->o_sym].sy_pins ?
		       symtab[o->o_sym].sy_pins[0] : 0;
		if ( npin != want )	/* a BRKR is a Q that is not one */
		{
			sdrop(symtab[o->o_sym].sy_code);
			continue;
		}
		if ( dev == 'R' || dev == 'C' || dev == 'L' )
		{
			if ( !spval(oval(o), vb) )
			{
				sdrop(symtab[o->o_sym].sy_code);
				continue;
			}
		}
		else if ( oval(o)[0] )	/* D/Q: the value IS the model */
		{
			strncpy(vb, oval(o), sizeof(vb) - 1);
			vb[sizeof(vb) - 1] = 0;
		}
		else
			strcpy(vb, symtab[o->o_sym].sy_code);
		if ( dev == 'Q' || dev == 'D' )
			sporder(&symtab[o->o_sym], dev, ord, want);
		else
			for ( k = 0; k < want; k++ )
				ord[k] = k;
		ok = 1;
		for ( k = 0; k < want; k++ )
			if ( (nd[k] = spnode(i, ord[k], sheet, rid)) < 0 )
				ok = 0;
		if ( !ok )
		{
			sdrop(symtab[o->o_sym].sy_code);
			continue;
		}
		sprintf(card, "%.8s", o->o_name);
		for ( k = 0; k < want; k++ )
			sprintf(card + strlen(card), " %d", nd[k]);
		printf("%s %s\n", card, vb);
	}
	return 0;
}

static
dospiceend()
{
	register int i;

	printf(".END\n");
	if ( nsdrop )
	{
		fprintf(stderr, "vellum: dropped:");
		for ( i = 0; i < nsdrop; i++ )
			fprintf(stderr, "%s %d %s", i ? "," : "",
				sdct[i], sdnm[i]);
		fprintf(stderr, "\n");
	}
	return 0;
}

/* ================================================================== */
/* -symsheet: the library on paper (VELLUM.md sec. 49)                */
/*                                                                    */
/* Every stencil library deserves the reference card the vendor's data */
/* book would have.  It is .d OUT, so it prints on any of the five     */
/* backends and the shop's binder fills itself from make:              */
/*	vellum -symsheet /usr/vellum/sym/pid.sym | vellum -ps -fit -    */
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
		fprintf(stderr, "vellum: cannot load %s\n", path);
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
/* the v5 entry: velport's velxport() hands each mode its sheet here   */
/* ================================================================== */

/* the per-sheet half (velxport's file loop) */
velv5(mode, sheet, fn, pat)
char *mode, *fn, *pat;
{
	if ( strcmp(mode, "len") == 0 )
		return dolen(sheet);
	if ( strcmp(mode, "where") == 0 )
	{
		dchg += dowhere(fn, pat);
		return 0;
	}
	if ( strcmp(mode, "spice") == 0 )
		return dospice(sheet, fn);
	return 0;
}

/* the after-the-last-sheet half; returns the exit status */
velv5end(mode)
char *mode;
{
	if ( strcmp(mode, "len") == 0 )
	{
		dolenend();
		return 0;
	}
	if ( strcmp(mode, "where") == 0 )
		return dchg > 254 ? 254 : dchg;
	if ( strcmp(mode, "spice") == 0 )
	{
		dospiceend();
		return 0;
	}
	if ( strcmp(mode, "diff") == 0 )
		return dchg > 254 ? 254 : dchg;
	return 0;
}

/* -diff proper: old.d has been loaded and shadowed, new.d is live */
velv5diff(fn, mark)
char *fn;
{
	dfile = fn;
	return dodiff(mark);
}
