/*
 * velnet.c - velnet: what the drawing is CONNECTED as.
 *
 *	velnet file.d ...		the netlist
 *	velnet -bom file.d ...		the parts list
 *	velnet -spice file.d ...	a SPICE 2G6 deck
 *	velnet -renum [-base N] file.d	canonical designators
 *
 * The connectivity itself is libvellum's (velnetc: wires unioned by
 * endpoint, junctions and pins attached, nets named) -- this tool is
 * the four ways a shop asks for it in writing.  Every one of them
 * takes the sheet SET: named nets MERGE across sheets, and a design
 * gets ONE parts list.
 *
 * -renum is the odd one and takes ONE sheet: it re-emits the drawing
 * with its symbols numbered by (y, x) from -base N, so a set renumbers
 * sheet by sheet with -base carrying the count across.
 */
#include <stdio.h>
#include "vellum.h"

/* the shared connectivity's results (libvellum, velnetc.c) */
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
/* -bom: bill of materials -- designator, value, count, sorted; one    */
/* text walker over the Y objects, ACCUMULATED over the whole sheet    */
/* set so a design gets ONE parts list (secs. 18-19).                  */
/* ================================================================== */

#define	MAXBOM	96
char	bomcode[MAXBOM][8];	/* stencil code                           */
char	bomval[MAXBOM][VALL];	/* value ("" groups separately)           */
char	bomref[MAXBOM][60];	/* the designators, space separated       */
short	bomcnt[MAXBOM];
int	nbom;

/* fold this (loaded) sheet's designated symbols into the table */
static
bomsheet()
{
	register DOBJ *o;
	register int i, k;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
			continue;
		for ( k = 0; k < nbom; k++ )
			if ( strcmp(bomcode[k],
				    symtab[o->o_sym].sy_code) == 0 &&
			     strcmp(bomval[k], o->o_val) == 0 )
				break;
		if ( k == nbom )
		{
			if ( nbom >= MAXBOM )
				continue;
			strcpy(bomcode[k], symtab[o->o_sym].sy_code);
			strcpy(bomval[k], o->o_val);
			bomref[k][0] = 0;
			bomcnt[k] = 0;
			nbom++;
		}
		bomcnt[k]++;
		if ( strlen(bomref[k]) + strlen(o->o_name) + 2 <
		     sizeof(bomref[0]) )
		{
			if ( bomref[k][0] )
				strcat(bomref[k], " ");
			strcat(bomref[k], o->o_name);
		}
	}
	return 0;
}

/* sort (code, then value) and print */
static
dobomout()
{
	register int i, j;
	short ix[MAXBOM];
	int t, c;

	for ( i = 0; i < nbom; i++ )
		ix[i] = i;
	for ( i = 1; i < nbom; i++ )		/* insertion sort */
		for ( j = i; j > 0; j-- )
		{
			c = strcmp(bomcode[ix[j - 1]], bomcode[ix[j]]);
			if ( c == 0 )
				c = strcmp(bomval[ix[j - 1]], bomval[ix[j]]);
			if ( c <= 0 )
				break;
			t = ix[j];  ix[j] = ix[j - 1];  ix[j - 1] = t;
		}
	for ( i = 0; i < nbom; i++ )
	{
		j = ix[i];
		printf("%3d  %-7s %-15s %s\n", bomcnt[j], bomcode[j],
		       bomval[j][0] ? bomval[j] : "-", bomref[j]);
	}
	return 0;
}

/* ================================================================== */
/* -renum: canonical designators (VELLUM.md sec. 38) -- symbols sorted */
/* by (y, x), numbered per prefix from -base N (default 1), the whole  */
/* drawing re-emitted through fmtobj to stdout.  Designators live      */
/* nowhere else in the format (nets are positional), so the rewrite    */
/* cannot dangle a reference.  ONE sheet per run: a set renumbers      */
/* sheet by sheet with -base carrying the count across -- the          */
/* Makefile owns the set, as always.                                   */
/* ================================================================== */

static
dorenum(base)
{
	register DOBJ *o;
	register int i, j;
	short ix[MAXOBJ];
	int n, k, t;
	char lb[220];

	n = 0;
	for ( i = 0; i < nobj; i++ )
		if ( obj[i].o_type == OT_SYM && obj[i].o_name[0] )
			ix[n++] = i;
	for ( i = 1; i < n; i++ )		/* insertion sort by (y, x) */
		for ( j = i; j > 0; j-- )
		{
			register DOBJ *a, *b;

			a = &obj[ix[j - 1]];
			b = &obj[ix[j]];
			if ( a->o_y < b->o_y ||
			     (a->o_y == b->o_y && a->o_x <= b->o_x) )
				break;
			t = ix[j];  ix[j] = ix[j - 1];  ix[j - 1] = t;
		}
	for ( i = 0; i < n; i++ )
	{
		register char *pfx;
		int cnt;

		o = &obj[ix[i]];
		pfx = symtab[o->o_sym].sy_pfx;
		if ( pfx[0] == 0 )
			continue;
		cnt = 0;
		for ( j = 0; j < i; j++ )
			if ( strcmp(symtab[obj[ix[j]].o_sym].sy_pfx,
				    pfx) == 0 )
				cnt++;
		sprintf(o->o_name, "%.3s%d", pfx, base + cnt);
	}
	/* re-emit the whole drawing (velfile writefile's shape, stdout) */
	printf("vellum1\n");
	if ( unum != 1 || uname[0] )
		printf("U %d %s\n", unum, uname[0] ? uname : "-");
	for ( i = 0; i < nobj; i++ )
	{
		if ( obj[i].o_grp &&
		     (i == 0 || obj[i - 1].o_grp != obj[i].o_grp) )
		{
			k = 0;
			for ( j = i; j < nobj &&
				     obj[j].o_grp == obj[i].o_grp; j++ )
				k++;
			printf("G %d\n", k);
		}
		fmtobj(i, lb);
		if ( lb[0] )
			printf("%s\n", lb);
	}
	return 0;
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
		fprintf(stderr, "velnet: dropped:");
		for ( i = 0; i < nsdrop; i++ )
			fprintf(stderr, "%s %d %s", i ? "," : "",
				sdct[i], sdnm[i]);
		fprintf(stderr, "\n");
	}
	return 0;
}


/* ================================================================== */
/* entry                                                              */
/* ================================================================== */

static
usage()
{
	fprintf(stderr, "usage: velnet [-bom|-spice] file.d ...\n");
	fprintf(stderr, "       velnet -renum [-base N] file.d\n");
	return 2;
}

main(argc, argv)
char **argv;
{
	register int i;
	register char *mode;
	int rbase, r, first;

	velprog = "velnet";
	mode = "net";
	rbase = 1;
	for ( i = 1; i < argc && argv[i][0] == '-' && argv[i][1]; i++ )
	{
		if ( strcmp(argv[i], "-bom") == 0 )
			mode = "bom";
		else if ( strcmp(argv[i], "-spice") == 0 )
			mode = "spice";
		else if ( strcmp(argv[i], "-renum") == 0 )
			mode = "renum";
		else if ( strcmp(argv[i], "-base") == 0 && i + 1 < argc )
			rbase = atoi(argv[++i]);
		else
			exit(usage());
	}
	nsheets = argc - i;
	if ( nsheets < 1 || (strcmp(mode, "renum") == 0 && nsheets != 1) )
		exit(usage());
	loadsyms();
	if ( strcmp(mode, "spice") == 0 )
		printf("* velnet -spice\n");
	r = 0;
	first = i;
	for ( ; i < argc; i++ )
	{
		if ( loadsheet(argv[i]) < 0 )
			exit(1);
		if ( strcmp(mode, "bom") == 0 )
			bomsheet();
		else if ( strcmp(mode, "renum") == 0 )
			r |= dorenum(rbase);
		else if ( strcmp(mode, "spice") == 0 )
			dospice(i - first + 1, argv[i]);
		else
			donet(i - first + 1);
	}
	if ( strcmp(mode, "bom") == 0 )
		dobomout();
	else if ( strcmp(mode, "spice") == 0 )
		r = dospiceend();
	else if ( strcmp(mode, "net") == 0 )
		donetend();
	exit(r);
}
