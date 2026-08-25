/*
 * snaptest.c - the SHIPPED undo pack/restore (velsnap.c, libvellum),
 * exercised without a window: the editor's snapshot is now a heap
 * block sized to the drawing, and the thing that could go wrong is the
 * pointer arithmetic that finds the two pools inside it.
 *
 * Loads real gallery drawings, packs, scribbles over the live model
 * the way an edit would, restores, and compares the whole model byte
 * for byte against a reference taken before the pack.  Then does the
 * UNDO/REDO toggle the editor does -- pack live, restore old, keep the
 * packed live as the new snapshot -- twice, which must land exactly
 * back where it started.
 */
#include <stdio.h>
#include "vellum.h"

/* the reference copy: plain arrays, the way the editor used to keep it */
DOBJ	robj[MAXOBJ];
int	rnobj;
short	rppool[PPOOL];
int	rppuse;
char	rtpool[TPOOL];
int	rtpuse;

int	fails;

static
refsave()
{
	register int i;

	for ( i = 0; i < nobj * sizeof(DOBJ); i++ )
		((char *)robj)[i] = ((char *)obj)[i];
	rnobj = nobj;
	for ( i = 0; i < ppuse * 2; i++ )
		((char *)rppool)[i] = ((char *)ppool)[i];
	rppuse = ppuse;
	for ( i = 0; i < tpuse; i++ )
		rtpool[i] = tpool[i];
	rtpuse = tpuse;
	return 0;
}

/* 0 = the live model equals the reference, byte for byte */
static
refcmp(what)
char *what;
{
	register int i;
	int bad;

	bad = 0;
	if ( nobj != rnobj || ppuse != rppuse || tpuse != rtpuse )
	{
		printf("  %s: COUNTS n %d/%d pp %d/%d tp %d/%d\n", what,
		       nobj, rnobj, ppuse, rppuse, tpuse, rtpuse);
		return 1;
	}
	for ( i = 0; i < nobj * sizeof(DOBJ); i++ )
		if ( ((char *)obj)[i] != ((char *)robj)[i] )
		{
			printf("  %s: OBJ byte %d (object %d)\n", what, i,
			       i / (int)sizeof(DOBJ));
			bad = 1;
			break;
		}
	for ( i = 0; i < ppuse * 2; i++ )
		if ( ((char *)ppool)[i] != ((char *)rppool)[i] )
		{
			printf("  %s: PPOOL byte %d\n", what, i);
			bad = 1;
			break;
		}
	for ( i = 0; i < tpuse; i++ )
		if ( tpool[i] != rtpool[i] )
		{
			printf("  %s: TPOOL byte %d\n", what, i);
			bad = 1;
			break;
		}
	return bad;
}

/* scribble over the live model the way an edit does: move everything,
 * restyle it, drop the last few objects and eat into both pools */
static
scribble()
{
	register int i;

	for ( i = 0; i < nobj; i++ )
	{
		obj[i].o_x += 3;
		obj[i].o_y -= 1;
		obj[i].o_flags ^= OF_BOLD;
	}
	for ( i = 0; i < ppuse; i++ )
		ppool[i] += 7;
	for ( i = 0; i < tpuse; i++ )
		if ( tpool[i] )
			tpool[i] ^= 0x20;
	if ( nobj > 3 )
		nobj -= 3;
	if ( ppuse > 4 )
		ppuse -= 4;
	if ( tpuse > 6 )
		tpuse -= 6;
	return 0;
}

static
one(fn)
char *fn;
{
	char *snap, *b, *old;
	int n, pp, tp, on, opp, otp;

	if ( loadfile(fn) < 0 )
	{
		printf("%s: cannot open\n", fn);
		fails++;
		return 1;
	}
	printf("%s: %d objects, pp %d, tp %d, block %d (was 19248)\n",
	       fn, nobj, ppuse, tpuse, usnapsize());

	/* --- 1: pack, scribble, restore --- */
	refsave();
	if ( (snap = usnappack()) == (char *)0 )
	{
		printf("  pack FAILED\n");
		fails++;
		return 1;
	}
	n = nobj;  pp = ppuse;  tp = tpuse;
	scribble();
	usnaprestore(snap, n, pp, tp);
	fails += refcmp("round trip");
	free(snap);

	/* --- 2: the editor's undo/redo toggle, twice --- */
	refsave();
	snap = usnappack();		/* the "snapshot" before the edit */
	on = nobj;  opp = ppuse;  otp = tpuse;
	scribble();			/* ... the edit */
	/* undo: pack live (redo block), restore old, keep the packed one */
	b = usnappack();
	n = nobj;  pp = ppuse;  tp = tpuse;
	old = snap;
	usnaprestore(old, on, opp, otp);
	free(old);
	snap = b;
	on = n;  opp = pp;  otp = tp;
	fails += refcmp("undo");
	/* redo: the same step again lands on the edited version */
	b = usnappack();
	n = nobj;  pp = ppuse;  tp = tpuse;
	old = snap;
	usnaprestore(old, on, opp, otp);
	free(old);
	snap = b;
	on = n;  opp = pp;  otp = tp;
	/* ... and once more is back to where the reference stands */
	b = usnappack();
	n = nobj;  pp = ppuse;  tp = tpuse;
	old = snap;
	usnaprestore(old, on, opp, otp);
	free(old);
	snap = b;
	fails += refcmp("redo, then undo");
	free(snap);

	/* --- 3: the pooled reads the damage pass makes THROUGH a block:
	 * objpbox2 against the block's own pools has to agree with the
	 * same object read from the live arrays --- */
	refsave();
	snap = usnappack();
	n = nobj;  pp = ppuse;  tp = tpuse;
	{
		register int i;
		int a0, b0, c0, d0, a1, b1, c1, d1;

		for ( i = 0; i < n; i++ )
		{
			objpbox2(&obj[i], ppool, tpool, &a0, &b0, &c0, &d0);
			objpbox2(&USOBJ(snap)[i], USPP(snap, n),
				 USTP(snap, n, pp), &a1, &b1, &c1, &d1);
			if ( a0 != a1 || b0 != b1 || c0 != c1 || d0 != d1 )
			{
				printf("  box %d: live %d,%d-%d,%d block %d,%d-%d,%d\n",
				       i, a0, b0, c0, d0, a1, b1, c1, d1);
				fails++;
				break;
			}
		}
	}
	free(snap);
	return 0;
}

main(argc, argv)
char **argv;
{
	register int i;

	velprog = "snaptest";
	loadsyms();
	for ( i = 1; i < argc; i++ )
		one(argv[i]);
	printf("snaptest: %d failure(s)\n", fails);
	exit(fails != 0);
}
