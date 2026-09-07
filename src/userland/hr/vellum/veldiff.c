/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * veldiff.c - veldiff: what changed between two revisions of a sheet.
 *
 *	veldiff old.d new.d		the change list, one per line
 *	veldiff -mark old.d new.d	... as a MARKED-UP DRAWING
 *
 * REVISIONS are files and the record is the directory (VELLUM.md sec.
 * 50): no SCCS, no hidden state, no format change.  The exit status is
 * the change count, so a Makefile's print: target will not put an
 * unreviewed sheet on paper.
 *
 * -mark writes ORDINARY OBJECTS any v1.4 binary displays -- the format
 * is not merely unbroken but untouched -- so the markup prints on every
 * backend the suite has.  It is the only tool that holds TWO drawings:
 * the old revision is shadowed whole, the new one stays live.
 */
#include <stdio.h>
#include "vellum.h"

/* ================================================================== */
/* The SECOND revision (sec. 45): a diff needs both drawings in memory */
/* at once, so the first one is copied here whole -- objects and both  */
/* pools.  It is THIS TOOL's data and nobody else's (sec. 13.1: a     */
/* tool's data is the cheap wall, and it still is) -- which is the     */
/* whole point of a tool per operation.                                */
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
/* what changed (VELLUM.md sec. 45)                                   */
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
/* -mark: the answer AS A DRAWING.  Everything unchanged on its        */
/* own layers, ADDED (and changed-to) objects BOLD, REMOVED (and       */
/* changed-from) objects re-emitted DASHED on the annotation layer.    */
/* No new viewer and no new UI: the markup is ordinary objects, so     */
/* velprev shows it, velplot prints it and the editor opens it -- the  */
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
/* entry                                                              */
/* ================================================================== */

main(argc, argv)
char **argv;
{
	register int i;
	int mark;

	velprog = "veldiff";
	mark = 0;
	for ( i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++ )
		if ( strcmp(argv[i], "-mark") == 0 )
			mark = 1;
		else
			break;
	if ( argc - i != 2 )
	{
		fprintf(stderr, "usage: veldiff [-mark] old.d new.d\n");
		exit(2);
	}
	nsheets = 2;
	loadsyms();
	if ( loadsheet(argv[i]) < 0 )		/* the OLD revision ... */
		exit(1);
	dsave();				/* ... shadowed whole   */
	if ( loadsheet(argv[i + 1]) < 0 )	/* the NEW one stays live */
		exit(1);
	dfile = argv[i + 1];
	dodiff(mark);
	exit(dchg > 254 ? 254 : dchg);
}
