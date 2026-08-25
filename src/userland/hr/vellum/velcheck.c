/*
 * velcheck.c - velcheck: prove the set before paper moves.
 *
 *	velcheck file.d ...		the drawings
 *	velcheck -sym lib.sym ...	the stencils they are drawn with
 *
 * One finding per line on stdout, the exit status the finding count,
 * so a Makefile gates on it the way cc gates on -Werror.  Drawing
 * findings: duplicate designators across the SET, a designated part
 * with no value, dangling wire ends, symbol codes no library defines,
 * sheets that disagree about the unit -- then the CONDUCTOR checks
 * (libvellum's net pass and its ERC rules), which run only on a sheet
 * that actually draws circuits.
 *
 * A wrong stencil is worse than a wrong drawing, because it is wrong
 * in EVERY drawing: -sym judges the libraries themselves, and reads
 * ONLY the ones it is given, so "defined in both" means these files.
 */
#include <stdio.h>
#include "vellum.h"

/* set-wide designators, for the across-sheets duplicate check */
#define	MAXDES	200
char	desnm[MAXDES][NAMEL];
char	dessh[MAXDES][14];
int	ndes;

/* per-sheet: duplicate designators (across the SET) and designated
 * parts with no value (the BOM's "-" rows, at the source) */
static
chkparts()
{
	register DOBJ *o;
	register int i, k;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
			continue;
		for ( k = 0; k < ndes; k++ )
			if ( strcmp(desnm[k], o->o_name) == 0 )
				break;
		if ( k < ndes )
			chk("duplicate designator %s (also in %s)",
			    o->o_name, dessh[k]);
		else if ( ndes < MAXDES )
		{
			strcpy(desnm[ndes], o->o_name);
			strncpy(dessh[ndes], chksheet, 13);
			dessh[ndes][13] = 0;
			ndes++;
		}
		if ( o->o_val[0] == 0 )
			chk("%s has no value", o->o_name, 0);
	}
	return 0;
}

/* per-sheet: dangling wire ends -- an endpoint touching no pin, no
 * other wire and no net-name marker is the classic drawing slip */
static
chkwires()
{
	register DOBJ *o;
	register int i, j;
	int e, px, py, ok;
	char pt[16];

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_WIRE )
			continue;
		for ( e = 0; e < 2; e++ )
		{
			px = e ? o->o_x2 : o->o_x;
			py = e ? o->o_y2 : o->o_y;
			ok = pinat(px, py);
			for ( j = 0; j < nobj && !ok; j++ )
			{
				if ( j == i )
					continue;
				if ( obj[j].o_type == OT_WIRE &&
				     xonwire(&obj[j], px, py) )
					ok = 1;
				else if ( obj[j].o_type == OT_NNAME &&
					  obj[j].o_x == px && obj[j].o_y == py )
					ok = 1;
			}
			if ( !ok )
			{
				sprintf(pt, "%d,%d", px, py);
				chk("dangling wire end at %s", pt, 0);
			}
		}
	}
	return 0;
}

/* per-sheet: symbol codes no loaded library defines.  loadfile SKIPS
 * unknown Y lines by design, so a hand-edited or generated file loses
 * parts silently -- this is where that surfaces.  Re-reads the raw
 * file: the skipped lines are, by definition, not in the model. */
static
chkcodes(fn)
char *fn;
{
	register FILE *fp;
	char lb[220];
	char *p, *t;

	if ( (fp = fopen(fn, "r")) == (FILE *)0 )
		return 0;
	while ( fgets(lb, sizeof(lb), fp) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 || t[0] != 'Y' || t[1] != 0 )
			continue;
		if ( (t = tok(&p)) == 0 )
			continue;
		if ( symbycode(t) < 0 )
			chk("unknown symbol %s (line dropped)", t, 0);
	}
	fclose(fp);
	return 0;
}

/* does this sheet draw CIRCUITS?  (the conductor checks' gate) */
static
haswires()
{
	register int k;

	for ( k = 0; k < nobj; k++ )
		if ( obj[k].o_type == OT_WIRE )
			return 1;
	return 0;
}

/* ================================================================== */
/* -sym: prove the library (sec. 56)                                  */
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
		fprintf(stderr, "velcheck: symcheck clean\n");
	return scn > 254 ? 254 : scn;
}


/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

main(argc, argv)
char **argv;
{
	register int i;
	int first;
	static int u0;
	static char un0[UNAMEL];

	velprog = "velcheck";
	if ( argc > 2 && strcmp(argv[1], "-sym") == 0 )
	{
		for ( i = 2; i < argc; i++ )
			dosymcheck(argv[i]);
		exit(dosymcheckend());
	}
	if ( argc < 2 || argv[1][0] == '-' && argv[1][1] )
	{
		fprintf(stderr, "usage: velcheck file.d ...\n");
		fprintf(stderr, "       velcheck -sym lib.sym ...\n");
		exit(2);
	}
	checkf = 1;			/* judging, not listing */
	nsheets = argc - 1;
	loadsyms();
	first = 1;
	for ( i = 1; i < argc; i++ )
	{
		if ( loadsheet(argv[i]) < 0 )
			exit(1);
		chksheet = argv[i];
		/* the whole pass: parts, wires, dropped Y lines, units
		 * agreement, then the net checks */
		if ( i == first )
		{
			u0 = unum;
			strcpy(un0, uname);
		}
		else if ( unum != u0 || strcmp(uname, un0) != 0 )
		{
			char ub[UNAMEL + 8];

			sprintf(ub, "%d %s", unum, uname);
			chk("units U %s disagree with the set's", ub, 0);
		}
		chkparts();
		chkcodes(argv[i]);
		/* The CONDUCTOR checks (dangling ends, nets, pins) apply
		 * only to a sheet that draws circuits -- one that contains
		 * WIRES.  A diagram sheet (flowchart, floor plan, structure
		 * chart) uses stencils whose pins are attachment points, not
		 * terminals, and flagging those is noise, not findings. */
		if ( haswires() )
		{
			chkwires();
			donet(i - first + 1);
		}
	}
	chksheet = "set";
	donetend();
	if ( chkn == 0 )
		fprintf(stderr, "velcheck: check clean\n");
	exit(chkn > 254 ? 254 : chkn);
}
