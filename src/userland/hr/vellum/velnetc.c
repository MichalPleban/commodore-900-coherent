/*
 * velnetc.c - the CONNECTIVITY, and nothing said about it: wires
 * unioned by endpoint, junctions and pins attached, nets named.  It
 * is asked its questions by more than one tool -- velnet reports the
 * nets, velcheck judges them, velnet -spice translates them and
 * velinfo -len measures them -- so it is a libvellum member and not
 * any one tool's file (VELLUM.md sec. 44 rule 1: every new verb is
 * machinery that already ships).
 *
 * chk() lives here too: a finding is "file: message" on stdout with
 * the count as the exit status, and the net pass raises findings of
 * its own (donetend's pin count, erc1's driver rules) whether it was
 * velcheck or velnet that started the walk.
 */
#include <stdio.h>
#include "vellum.h"

int	checkf;			/* judging: report findings, list no nets */
int	chkn;			/* finding count = the exit status        */
char	*chksheet = "";		/* file name for the report lines         */

/* One finding: "file: message".  a/b ride printf %s/%d holes. */
chk(msg, a, b)
char *msg, *a, *b;
{
	printf("%s: ", chksheet);
	printf(msg, a, b);
	printf("\n");
	chkn++;
	return 0;
}
/* ================================================================== */
/* -net: union-find over wire endpoints, junctions and pins           */
/* ================================================================== */

short	wnet[MAXOBJ];		/* wire object -> net root (union-find)   */

/* nfind/nunion/xonwire/xpinpos and netbuild() below are NOT static: the
 * v5 asking modes (-len, -spice) are the same question asked twice, and
 * a second union-find would be a second answer (VELLUM.md sec. 46). */
nfind(i)
{
	while ( wnet[i] != i )
		i = wnet[i] = wnet[wnet[i]];
	return i;
}

nunion(a, b)
{
	a = nfind(a);
	b = nfind(b);
	if ( a != b )
		wnet[a] = b;
	return 0;
}

/* Is grid point (px,py) ON wire o (either leg)? */
xonwire(o, px, py)
DOBJ *o;
{
	register int a, b;

	if ( o->o_x != o->o_x2 && py == o->o_y )
	{
		a = o->o_x;  b = o->o_x2;
		if ( a > b ) { a = o->o_x2;  b = o->o_x; }
		if ( px >= a && px <= b )
			return 1;
	}
	if ( o->o_y != o->o_y2 && px == o->o_x2 )
	{
		a = o->o_y;  b = o->o_y2;
		if ( a > b ) { a = o->o_y2;  b = o->o_y; }
		if ( py >= a && py <= b )
			return 1;
	}
	if ( px == o->o_x && py == o->o_y )
		return 1;
	if ( px == o->o_x2 && py == o->o_y2 )
		return 1;
	return 0;
}

/* device-independent pin position of pin k of symbol object i */
xpinpos(i, k, gx, gy)
int *gx, *gy;
{
	register DOBJ *o;
	register short *pp;
	int qx, qy;

	o = &obj[i];
	pp = symtab[o->o_sym].sy_pins;
	xtxq(pp[1 + 2*k], pp[2 + 2*k], (int)o->o_rot, (int)o->o_mir,
	     0, 0, 1, &qx, &qy);
	*gx = o->o_x + qx / 4;
	*gy = o->o_y + qy / 4;
	return 0;
}

#define	MAXNPIN	256
#define	MAXNNAME (MAXOBJ / 4)	/* named nets one sheet can carry         */

/* Named nets MERGE across the sheet set (sec. 19): a net named VBUS on
 * sheet 1 is the same conductor as VBUS on sheet 3.  Their pins pool
 * here and print after the last sheet; unnamed nets stay sheet-local. */
#define	MAXMNET	24
char	mnname[MAXMNET][10];
char	mnpins[MAXMNET][120];
short	mncnt[MAXMNET];
short	mno[MAXMNET], mnp[MAXMNET];	/* ERC (v4.4): typed-pin counts   */
short	mni[MAXMNET], mnt[MAXMNET];	/* over the whole merged net      */
int	nmnet;

/* The ERC verdicts on one net's typed-pin counts (VELLUM.md sec. 38):
 * two outputs conflict, an output on a power net drives a rail, a net
 * whose typed pins are all inputs is undriven.  The rules fire ONLY
 * between typed pins -- an old library produces silence, not noise,
 * and a half-typed one checks exactly as far as it is typed. */
static
erc1(nm, no, npw, ni, nt)
char *nm;
{
	if ( no >= 2 )
		chk("net %s: driver conflict (%d outputs)", nm, no);
	if ( no >= 1 && npw >= 1 )
		chk("net %s: output drives a power rail", nm, 0);
	if ( nt >= 1 && ni == nt )
		chk("net %s: undriven (typed pins all inputs)", nm, 0);
	return 0;
}

/* one pin as " REF.PIN" (pin NAMES when the library has them) */
static
pinstr(o, pin, buf)
register DOBJ *o;
char *buf;
{
	register int e;

	sprintf(buf, " %s.", o->o_name[0] ? o->o_name : "?");
	e = pinnm[PINSLOT(&symtab[o->o_sym], pin)];
	if ( e )
		strcat(buf, &pnmpool[e]);
	else
		sprintf(buf + strlen(buf), "%d", pin + 1);
	return 0;
}

/* After the last sheet: the merged named nets, and their pin check.
 * In -check mode nothing lists; a named net with fewer than two pins
 * across the WHOLE set is the finding -- which is also the bus rule
 * (sec. 28): a bus bit name appearing exactly once is a typo. */
donetend()
{
	register int i;

	for ( i = 0; i < nmnet; i++ )
	{
		if ( checkf )
		{
			if ( mncnt[i] < 2 )
				chk("named net %s has fewer than 2 pins in the set",
				    mnname[i], 0);
			erc1(mnname[i], (int)mno[i], (int)mnp[i],
			     (int)mni[i], (int)mnt[i]);
			continue;
		}
		printf("%s:%s\n", mnname[i], mnpins[i]);
		if ( mncnt[i] < 2 )
			fprintf(stderr,
				"vellum: warning: net %s has %d pin\n",
				mnname[i], mncnt[i]);
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* netbuild(): the CONNECTIVITY, with nothing said about it -- wires    */
/* unioned, pins attached, nets named.  -net reports it, -check judges  */
/* it, and the v5 modes -len and -spice ask it their own questions      */
/* (VELLUM.md sec. 44 rule 1: every new verb is machinery that ships).  */
/* The results are FILE-SCOPE, not donet's locals, exactly so.          */
/* ------------------------------------------------------------------ */

short	pobj[MAXNPIN], ppin[MAXNPIN], pnet[MAXNPIN];
int	np;			/* pins found on this sheet               */
char	nnm[MAXNNAME][10];	/* net root -> name (GND/VCC/N)           */
short	nroot[MAXNNAME];
int	nnames;

netbuild()
{
	register DOBJ *o;
	register int i, j;
	int k, gx, gy, px, py;

	for ( i = 0; i < nobj; i++ )
		wnet[i] = i;
	/* wires: endpoints on another wire join the nets */
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_WIRE )
			continue;
		for ( j = 0; j < nobj; j++ )
		{
			if ( j == i || obj[j].o_type != OT_WIRE )
				continue;
			if ( xonwire(&obj[j], (int)o->o_x, (int)o->o_y) ||
			     xonwire(&obj[j], (int)o->o_x2, (int)o->o_y2) )
				nunion(i, j);
		}
	}
	/* pins: each pin joins the net of any wire through its point;
	 * pins sharing a point join each other */
	np = 0;
	for ( i = 0; i < nobj && np < MAXNPIN; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM )
			continue;
		if ( symtab[o->o_sym].sy_pins == 0 )
			continue;
		for ( k = 0; k < symtab[o->o_sym].sy_pins[0] &&
			     np < MAXNPIN; k++ )
		{
			xpinpos(i, k, &gx, &gy);
			pobj[np] = i;
			ppin[np] = k;
			pnet[np] = -1;
			for ( j = 0; j < nobj; j++ )
				if ( obj[j].o_type == OT_WIRE &&
				     xonwire(&obj[j], gx, gy) )
				{
					pnet[np] = nfind(j);
					break;
				}
			if ( pnet[np] < 0 )
			{
				/* no wire: pins stacked on the same
				 * point still connect */
				for ( j = 0; j < np; j++ )
				{
					int ox, oy;

					xpinpos((int)pobj[j], (int)ppin[j],
						&ox, &oy);
					if ( ox == gx && oy == gy &&
					     pnet[j] >= 0 )
					{
						pnet[np] = pnet[j];
						break;
					}
				}
			}
			np++;
		}
	}
	/* re-root every pin net after the unions above */
	for ( i = 0; i < np; i++ )
		if ( pnet[i] >= 0 )
			pnet[i] = nfind((int)pnet[i]);
	/* GND/VCC symbols and N objects NAME their nets */
	nnames = 0;
	for ( i = 0; i < np && nnames < MAXOBJ / 4; i++ )
	{
		register char *c;

		c = symtab[obj[pobj[i]].o_sym].sy_code;
		if ( strcmp(c, "GND") != 0 && strcmp(c, "VCC") != 0 )
			continue;
		if ( pnet[i] < 0 )
			continue;
		for ( j = 0; j < nnames; j++ )
			if ( nroot[j] == pnet[i] )
				break;
		if ( j == nnames )
		{
			nroot[nnames] = pnet[i];
			strcpy(nnm[nnames], c);
			nnames++;
		}
	}
	for ( i = 0; i < nobj && nnames < MAXOBJ / 4; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_NNAME )
			continue;
		px = o->o_x;
		py = o->o_y;
		for ( j = 0; j < nobj; j++ )
			if ( obj[j].o_type == OT_WIRE &&
			     xonwire(&obj[j], px, py) )
			{
				int r;

				r = nfind(j);
				for ( k = 0; k < nnames; k++ )
					if ( nroot[k] == r )
						break;
				if ( k == nnames )
				{
					nroot[nnames] = r;
					strncpy(nnm[nnames], o->o_name, 9);
					nnm[nnames][9] = 0;
					nnames++;
				}
				break;
			}
	}
	return 0;
}

donet(sheet)
{
	register DOBJ *o;
	register int i, j;
	int k;
	int to, tp, ti, tt;		/* ERC typed-pin counts, per net */
	int nid, cnt;
	short outed[MAXOBJ];

	netbuild();
	/* output: one line per net with pins on it.  A NAMED net is not
	 * printed here: its pins pool into the set-wide merge table and
	 * print after the last sheet (donetend); unnamed nets print now,
	 * qualified NET-<sheet>.<n> when the set has several sheets. */
	for ( i = 0; i < nobj; i++ )
		outed[i] = 0;
	nid = 0;
	for ( i = 0; i < np; i++ )
	{
		int r, m;
		char pb[24];

		if ( pnet[i] < 0 )
			continue;
		r = pnet[i];
		if ( outed[r] )
			continue;
		outed[r] = 1;
		nid++;
		for ( k = 0; k < nnames; k++ )
			if ( nroot[k] == r )
				break;
		m = -1;
		if ( k < nnames )
		{
			for ( m = 0; m < nmnet; m++ )
				if ( strcmp(mnname[m], nnm[k]) == 0 )
					break;
			if ( m == nmnet )
			{
				if ( nmnet >= MAXMNET )
					m = -1;
				else
				{
					strcpy(mnname[m], nnm[k]);
					mnpins[m][0] = 0;
					mncnt[m] = 0;
					mno[m] = mnp[m] = 0;
					mni[m] = mnt[m] = 0;
					nmnet++;
				}
			}
		}
		else if ( !checkf )
		{
			if ( nsheets > 1 )
				printf("NET-%d.%d:", sheet, nid);
			else
				printf("NET-%d:", nid);
		}
		cnt = 0;
		to = tp = ti = tt = 0;
		for ( j = 0; j < np; j++ )
		{
			if ( pnet[j] != r )
				continue;
			o = &obj[pobj[j]];
			cnt++;		/* EVERY pin counts toward the
					 * single-pin test: a feeder segment
					 * from a breaker to a designator-less
					 * bus tap is two connections, not a
					 * loose end */
			if ( checkf )
			{
				/* ERC counts EVERY pin's type -- a GND
				 * stencil's 'p' names the rail even though
				 * it is never a listed pin */
				register int pt;

				pt = pintyp[PINSLOT(&symtab[o->o_sym],
						    (int)ppin[j])];
				if ( pt )
				{
					tt++;
					if ( pt == 'o' )	to++;
					else if ( pt == 'p' )	tp++;
					else if ( pt == 'i' )	ti++;
				}
			}
			if ( symtab[o->o_sym].sy_pfx[0] == 0 )
				continue;	/* ... but a designator-less
						 * stencil (rails, taps,
						 * off-page markers) only NAMES
						 * or carries the net -- it is
						 * never a LISTED pin */
			pinstr(o, (int)ppin[j], pb);
			if ( k < nnames )
			{
				if ( m >= 0 &&
				     strlen(mnpins[m]) + strlen(pb) <
				     sizeof(mnpins[0]) )
					strcat(mnpins[m], pb);
			}
			else if ( !checkf )
				printf("%s", pb);
		}
		if ( k < nnames )
		{
			if ( m >= 0 )
			{
				mncnt[m] += cnt;
				mno[m] += to;
				mnp[m] += tp;
				mni[m] += ti;
				mnt[m] += tt;
			}
		}
		else if ( checkf )
		{
			sprintf(pb, "NET-%d", nid);
			if ( cnt < 2 )
				chk("net %s has fewer than 2 pins", pb, 0);
			erc1(pb, to, tp, ti, tt);
		}
		else
		{
			printf("\n");
			if ( cnt < 2 )
				fprintf(stderr,
				    "vellum: warning: NET-%d has %d pin\n",
				    nid, cnt);
		}
	}
	/* unconnected pins are the classic drawing error: say so --
	 * except on designator-less stencils, whose spare pins are by
	 * design (an off-page marker uses one of its two) */
	for ( i = 0; i < np; i++ )
		if ( pnet[i] < 0 )
		{
			o = &obj[pobj[i]];
			if ( symtab[o->o_sym].sy_pfx[0] == 0 )
				continue;
			if ( checkf )
			{
				char nb[8];

				sprintf(nb, "%d", ppin[i] + 1);
				chk("%s pin %s unconnected",
				    o->o_name[0] ? o->o_name : "?", nb);
			}
			else
				fprintf(stderr,
					"vellum: warning: %s pin %d unconnected\n",
					o->o_name[0] ? o->o_name : "?",
					ppin[i] + 1);
		}
	return 0;
}
