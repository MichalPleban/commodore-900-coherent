/*
 * velfile.c - Vellum's plain-text drawing format and symbol libraries.
 *
 * Everything that reads or writes the "vellum1" .d format lives here:
 * the tokenizer, the per-type line parsers (shared by Open, clipboard
 * Paste and any script that feeds lines in), the writer (shared by
 * Save, autosave and the clipboard Copy serializer) and the symbol
 * LIBRARY loader.  Split from vellum.c: one module outgrew the
 * assembler's fix-up tables (phase errors), and the format code is the
 * natural seam.
 */
#include <stdio.h>
#include "vellum.h"

/* ------------------------------------------------------------------ */
/* load / save                                                        */
/* ------------------------------------------------------------------ */

/* The S-line shape kind names and K-line connector style names (the
 * palette labels use them too). */
char	*shname[] = { "box", "rbox", "diamond", "oval", "par",
		      "drum", "doc", "circle", "ellipse" };
char	*csname[] = { "hv", "line", "arrow", "harrow" };

/* Next blank-separated token out of *pp (modifies the buffer); 0 at end. */
char *
tok(pp)
char **pp;
{
	register char *p, *s;

	p = *pp;
	while ( *p == ' ' || *p == '\t' )
		p++;
	if ( *p == 0 || *p == '\n' )
		return 0;
	s = p;
	while ( *p && *p != ' ' && *p != '\t' && *p != '\n' )
		p++;
	if ( *p )
		*p++ = 0;
	*pp = p;
	return s;
}

/* One blank-separated NUMBER into a short; 0 at end of line.  The
 * per-type parsers below are little more than runs of this. */
static
pnum(pp, ps)
char **pp;
short *ps;
{
	register char *t;

	if ( (t = tok(pp)) == 0 )
		return 0;
	*ps = atoi(t);
	return 1;
}

symbycode(c)
char *c;
{
	register int i;

	for ( i = 0; i < nsym; i++ )
		if ( strcmp(symtab[i].sy_code, c) == 0 )
			return i;
	return -1;
}

/* n numbers into the op pool after the opcode at symops[opuse]. */
static
oprd(pp, n)
char **pp;
{
	register int i;

	for ( i = 1; i <= n; i++ )
		if ( !pnum(pp, &symops[opuse + i]) )
			return 0;
	return 1;
}

/* Load ONE symbol library file, appending its symbols as a new palette
 * group named after the file (basename, extension dropped).  Returns the
 * group index, or -1 (unreadable / empty / tables full).  A path already
 * loaded is not re-read: its group index is returned, so the Lib button
 * doubles as a switch.  The format is the plain text zsym writes:
 *	symbol CODE PFX     start a symbol ('-' = no designator prefix)
 *	s x0 y0 x1 y1       segment, quarter-grid units
 *	c cx cy r           circle
 *	t x y C             one small-font character
 *	p x y               pin (whole grid units: multiples of 4)
 *	end                 finish it (pins go before it)
 * '#' lines are comments; anything malformed is skipped. */
loadlib(path)
char *path;
{
	register FILE *fp;
	char lb[80];
	char *p, *t;
	register SYMDEF *s;
	int i, in, n0, o0, p0;

	for ( i = 0; i < nlib; i++ )
		if ( strcmp(libpath[i], path) == 0 )
			return i;
	if ( nlib >= MAXLIB || strlen(path) >= sizeof(libpath[0]) )
		return -1;
	if ( (fp = fopen(path, "r")) == (FILE *)0 )
		return -1;
	n0 = nsym;
	o0 = opuse;
	p0 = pinuse;
	in = 0;
	s = (SYMDEF *)0;
	while ( fgets(lb, sizeof(lb), fp) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 || t[0] == '#' )
			continue;
		if ( strcmp(t, "symbol") == 0 )
		{
			in = 0;
			if ( nsym >= MAXSYM ||
			     opuse + 1 >= SYMOPS || pinuse >= SYMPINS )
				continue;
			if ( (t = tok(&p)) == 0 || t[0] == 0 )
				continue;
			s = &symtab[nsym];
			strncpy(symcode[nsym], t, 7);
			symcode[nsym][7] = 0;
			symprefix[nsym][0] = 0;
			if ( (t = tok(&p)) != 0 && strcmp(t, "-") != 0 )
			{
				strncpy(symprefix[nsym], t, 3);
				symprefix[nsym][3] = 0;
			}
			s->sy_code = symcode[nsym];
			s->sy_pfx = symprefix[nsym];
			s->sy_ops = &symops[opuse];
			s->sy_pins = &sympin[pinuse];
			s->sy_lib = nlib;
			sympin[pinuse++] = 0;	/* pin count, grows below */
			in = 1;
			continue;
		}
		if ( !in )
			continue;
		if ( strcmp(t, "end") == 0 )
		{
			symops[opuse++] = SEND;
			nsym++;
			in = 0;
			continue;
		}
		if ( strcmp(t, "s") == 0 && opuse + 6 < SYMOPS )
		{
			symops[opuse] = SE;
			if ( oprd(&p, 4) )
				opuse += 5;
		}
		else if ( strcmp(t, "c") == 0 && opuse + 5 < SYMOPS )
		{
			symops[opuse] = SC;
			if ( oprd(&p, 3) )
				opuse += 4;
		}
		else if ( strcmp(t, "t") == 0 && opuse + 5 < SYMOPS )
		{
			symops[opuse] = ST;
			if ( oprd(&p, 2) && (t = tok(&p)) != 0 )
			{
				symops[opuse + 3] = t[0];
				opuse += 4;
			}
		}
		else if ( strcmp(t, "a") == 0 && opuse + 7 < SYMOPS )
		{
			symops[opuse] = SA;
			if ( oprd(&p, 5) )
				opuse += 6;
		}
		else if ( strcmp(t, "p") == 0 && pinuse + 2 < SYMPINS &&
			  s->sy_pins[0] < 8 )
		{
			if ( (t = tok(&p)) == 0 )
				continue;
			sympin[pinuse] = atoi(t);
			if ( (t = tok(&p)) == 0 )
				continue;
			sympin[pinuse + 1] = atoi(t);
			pinnm[(pinuse + 1) / 2] = 0;
			pintyp[(pinuse + 1) / 2] = 0;
			if ( (t = tok(&p)) != 0 )
			{
				if ( t[0] != 0 && strcmp(t, "-") != 0 &&
				     pnmuse + (int)strlen(t) + 1 < PNMPOOL )
				{
					pinnm[(pinuse + 1) / 2] = pnmuse;
					strcpy(&pnmpool[pnmuse], t);
					pnmuse += strlen(t) + 1;
				}
				/* the FOURTH token: the pin type (v4.4) */
				if ( (t = tok(&p)) != 0 && t[0] && t[1] == 0 )
					pintyp[(pinuse + 1) / 2] = t[0];
			}
			pinuse += 2;
			s->sy_pins[0]++;
		}
	}
	fclose(fp);
	if ( nsym == n0 )		/* nothing in it: forget the group */
	{
		opuse = o0;
		pinuse = p0;
		return -1;
	}
	t = path;			/* group name = basename, no extension */
	for ( p = path; *p; p++ )
		if ( *p == '/' )
			t = p + 1;
	for ( i = 0; t[i] && t[i] != '.' && i < 11; i++ )
		libname[nlib][i] = t[i];
	libname[nlib][i] = 0;
	strcpy(libpath[nlib], path);
	symbounds();
	return nlib++;
}

/* Start-up: the libraries LIBLIST names, then the user scratch file. */
loadsyms()
{
	register FILE *fp;
	char lb[60];
	register int i;

	if ( (fp = fopen(LIBLIST, "r")) != (FILE *)0 )
	{
		while ( fgets(lb, sizeof(lb), fp) != 0 )
		{
			for ( i = 0; lb[i] && lb[i] != '\n'; i++ )
				;
			lb[i] = 0;
			if ( lb[0] == 0 || lb[0] == '#' )
				continue;
			loadlib(lb);
		}
		fclose(fp);
	}
	loadlib(USERLIB);
	curlib = 0;
	return 0;
}

/* Append the optional " /flags.layer" attribute token (old readers read
 * their fixed fields and ignore it). */
static
attrcat(o, lb)
register DOBJ *o;
char *lb;
{
	if ( o->o_flags || o->o_layer )
		sprintf(lb + strlen(lb), " /%d.%d", o->o_flags & 0xff,
			o->o_layer);
	return 0;
}

/* Format object i as one .d line (no newline) into lb (>= 220 bytes).
 * Shared by Save, the clipboard serializer and the autosave. */
fmtobj(i, lb)
char *lb;
{
	register DOBJ *o;
	register int k;

	o = &obj[i];
	lb[0] = 0;
	switch ( o->o_type )
	{
	case OT_SYM:
		sprintf(lb, "Y %s %d %d %d %d %s %s",
			symtab[o->o_sym].sy_code, o->o_x, o->o_y,
			o->o_rot, o->o_mir,
			o->o_name[0] ? o->o_name : "-",
			o->o_val[0] ? o->o_val : "-");
		attrcat(o, lb);
		break;
	case OT_WIRE:
	case OT_LINE:
	case OT_BOX:
	case OT_CIRC:
		sprintf(lb, "%c %d %d %d %d",
			o->o_type == OT_WIRE ? 'W' :
			o->o_type == OT_LINE ? 'L' :
			o->o_type == OT_BOX ? 'B' : 'C',
			o->o_x, o->o_y, o->o_x2, o->o_y2);
		attrcat(o, lb);
		break;
	case OT_TEXT:
		sprintf(lb, "T %d %d s%d", o->o_x, o->o_y, o->o_rot);
		attrcat(o, lb);
		sprintf(lb + strlen(lb), " %s", oval(o));
		break;
	case OT_SHAPE:
		sprintf(lb, "S %s %d %d %d %d", shname[o->o_sym],
			o->o_x, o->o_y, o->o_x2, o->o_y2);
		attrcat(o, lb);
		if ( oval(o)[0] )
			sprintf(lb + strlen(lb), " %s", oval(o));
		break;
	case OT_CONN:
		{
			char ab[2][14];
			int e;

			for ( e = 0; e < 2; e++ )
			{
				if ( ACOBJ(o, e) >= 0 )
					sprintf(ab[e], "@%d,%d",
						e ? o->o_x2 : o->o_x,
						e ? o->o_y2 : o->o_y);
				else
					strcpy(ab[e], "-");
			}
			sprintf(lb, "K %s %d %d %d %d %s %s",
				csname[o->o_sym], o->o_x, o->o_y,
				o->o_x2, o->o_y2, ab[0], ab[1]);
			attrcat(o, lb);
		}
		break;
	case OT_POLY:
		sprintf(lb, "P %d", o->o_sym);
		for ( k = 0; k < o->o_sym; k++ )
			sprintf(lb + strlen(lb), " %d %d",
				ppool[o->o_x2 + 2*k],
				ppool[o->o_x2 + 2*k + 1]);
		attrcat(o, lb);
		break;
	case OT_ARC:
		sprintf(lb, "A %d %d %d %d %d", o->o_x, o->o_y, o->o_x2,
			OA0(o), OA1(o));
		attrcat(o, lb);
		break;
	case OT_NNAME:
		sprintf(lb, "N %d %d %s", o->o_x, o->o_y, o->o_name);
		break;
	case OT_DIM:
		sprintf(lb, "D %d %d %d %d", o->o_x, o->o_y,
			o->o_x2, o->o_y2);
		attrcat(o, lb);
		if ( oval(o)[0] )
			sprintf(lb + strlen(lb), " %s", oval(o));
		break;
	}
	return 0;
}

/* Write the whole drawing to fn; does NOT touch `modified' (the autosave
 * writes the .bak without claiming the file is saved). */
writefile(fn)
char *fn;
{
	register FILE *fp;
	register int i;
	int n, j;
	char lb[220];

	if ( (fp = fopen(fn, "w")) == (FILE *)0 )
		return -1;
	fprintf(fp, "vellum1\n");
	if ( unum != 1 || uname[0] )	/* sheet units header */
		fprintf(fp, "U %d %s\n", unum, uname[0] ? uname : "-");
	for ( i = 0; i < nobj; i++ )
	{
		/* a persistent group saves as "G n" heading its (contiguous)
		 * members */
		if ( obj[i].o_grp &&
		     (i == 0 || obj[i - 1].o_grp != obj[i].o_grp) )
		{
			n = 0;
			for ( j = i; j < nobj &&
				     obj[j].o_grp == obj[i].o_grp; j++ )
				n++;
			fprintf(fp, "G %d\n", n);
		}
		fmtobj(i, lb);
		if ( lb[0] )
			fprintf(fp, "%s\n", lb);
	}
	fclose(fp);
	sync();		/* a drawing SAVED should survive a power cut */
	return 0;
}

savefile(fn)
char *fn;
{
	if ( writefile(fn) < 0 )
		return -1;
	modified = 0;
	return 0;
}

/* ---- the line parser, shared by Open, clipboard Paste and scripts ---- */

int	pgrem, pgid;		/* pending "G n" group header state       */

parsereset()
{
	pgrem = 0;
	pgid = 0;
	return 0;
}

/* First unused group id (1..127). */
newgid()
{
	register int i, g;
	char used[128];

	for ( g = 0; g < 128; g++ )
		used[g] = 0;
	for ( i = 0; i < nobj; i++ )
		used[obj[i].o_grp & 127] = 1;
	for ( g = 1; g < 128; g++ )
		if ( !used[g] )
			return g;
	return 0;
}

/* "/flags.layer" attribute token. */
static
parseattr(t, o)
char *t;
DOBJ *o;
{
	register char *q;

	if ( t[0] != '/' )
		return 0;
	o->o_flags = atoi(t + 1) & 0xff;	/* v2: hatch/smooth/vert bits
						 * ride the upper bits (a v1.4
						 * binary re-saving drops them) */
	for ( q = t + 1; *q && *q != '.'; q++ )
		;
	if ( *q == '.' )
		o->o_layer = atoi(q + 1) & (NLAYER - 1);
	return 1;
}

/* If the next token is an OPTION (an attribute, or -- when wantsz -- a
 * text-size "sN"), consume and return it; otherwise leave *pp alone, the
 * rest of the line is free text. */
static char *
opttok(pp, wantsz)
char **pp;
{
	register char *p;

	p = *pp;
	while ( *p == ' ' || *p == '\t' )
		p++;
	if ( p[0] == '/' && p[1] >= '0' && p[1] <= '9' )
		;
	else if ( wantsz && p[0] == 's' && p[1] >= '0' && p[1] <= '2' &&
		  (p[2] == ' ' || p[2] == '\t' || p[2] == '\n' ||
		   p[2] == 0) )
		;
	else
		return (char *)0;
	*pp = p;
	return tok(pp);
}

/* Rest-of-line into dst (n bytes); returns its length. */
static
resttext(p, dst, n)
char *p, *dst;
{
	register int i;

	while ( *p == ' ' || *p == '\t' )
		p++;
	for ( i = 0; p[i] && p[i] != '\n' && i < n - 1; i++ )
		dst[i] = p[i];
	dst[i] = 0;
	return i;
}

/* ---- per-type line parsers (kept separate so no single function grows
 * past the assembler's short-branch reach) ---- */

/* The four coordinates every two-point object line carries. */
static
p4(pp, o)
char **pp;
register DOBJ *o;
{
	return pnum(pp, &o->o_x) && pnum(pp, &o->o_y) &&
	       pnum(pp, &o->o_x2) && pnum(pp, &o->o_y2);
}

static
p_sym(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;
	int si;
	short w;

	if ( (t = tok(pp)) == 0 || (si = symbycode(t)) < 0 )
		return 0;
	o->o_type = OT_SYM;
	o->o_sym = si;
	if ( !pnum(pp, &o->o_x) || !pnum(pp, &o->o_y) )
		return 0;
	if ( !pnum(pp, &w) ) return 0;
	o->o_rot = w & 3;
	if ( !pnum(pp, &w) ) return 0;
	o->o_mir = w & 1;
	if ( (t = tok(pp)) != 0 && strcmp(t, "-") != 0 )
	{
		strncpy(o->o_name, t, NAMEL - 1);
		o->o_name[NAMEL - 1] = 0;
	}
	if ( (t = tok(pp)) != 0 && strcmp(t, "-") != 0 )
	{
		strncpy(o->o_val, t, VALL - 1);
		o->o_val[VALL - 1] = 0;
	}
	if ( (t = tok(pp)) != 0 )
		parseattr(t, o);
	return 1;
}

static
p_text(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;

	o->o_type = OT_TEXT;
	o->o_rot = 2;			/* pre-size files drew the UI font */
	if ( !pnum(pp, &o->o_x) || !pnum(pp, &o->o_y) )
		return 0;
	while ( (t = opttok(pp, 1)) != 0 )
	{
		if ( t[0] == 's' )
			o->o_rot = t[1] - '0';
		else
			parseattr(t, o);
	}
	{
		char tb[TVMAX];

		if ( resttext(*pp, tb, TVMAX) == 0 )
			return 0;
		setoval(o, tb);
	}
	return 1;
}

static
p_shape(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;
	register int si;

	if ( (t = tok(pp)) == 0 )
		return 0;
	for ( si = 0; si < NSHAPE; si++ )
		if ( strcmp(t, shname[si]) == 0 )
			break;
	if ( si >= NSHAPE )
		return 0;
	o->o_type = OT_SHAPE;
	o->o_sym = si;
	if ( !p4(pp, o) )
		return 0;
	while ( (t = opttok(pp, 0)) != 0 )
		parseattr(t, o);
	{
		char tb[TVMAX];

		resttext(*pp, tb, TVMAX);
		setoval(o, tb);
	}
	return 1;
}

static
p_conn(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;
	register int si;

	if ( (t = tok(pp)) == 0 )
		return 0;
	for ( si = 0; si < NCONNS; si++ )
		if ( strcmp(t, csname[si]) == 0 )
			break;
	if ( si >= NCONNS )
		return 0;
	o->o_type = OT_CONN;
	o->o_sym = si;
	if ( !p4(pp, o) )
		return 0;
	ACOBJ(o, 0) = -1;
	ACOBJ(o, 1) = -1;
	if ( (t = tok(pp)) == 0 )
		return 1;
	if ( t[0] == '@' )
		ACOBJ(o, 0) = -2;	/* resolve after the load */
	if ( (t = tok(pp)) == 0 )
		return 1;
	if ( t[0] == '@' )
		ACOBJ(o, 1) = -2;
	if ( (t = tok(pp)) != 0 )
		parseattr(t, o);
	return 1;
}

static
p_poly(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;
	register int i;
	int np;

	if ( (t = tok(pp)) == 0 )
		return 0;
	np = atoi(t);
	if ( np < 2 || np > PMAXPT || ppuse + 2 * np > PPOOL )
		return 0;
	o->o_type = OT_POLY;
	o->o_sym = np;
	o->o_x2 = ppuse;
	for ( i = 0; i < 2 * np; i++ )
		if ( !pnum(pp, &ppool[ppuse + i]) )
			return 0;
	ppuse += 2 * np;
	o->o_x = ppool[o->o_x2];
	o->o_y = ppool[o->o_x2 + 1];
	if ( (t = tok(pp)) != 0 )
		parseattr(t, o);
	return 1;
}

static
p_arc(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;

	o->o_type = OT_ARC;
	if ( !pnum(pp, &o->o_x) || !pnum(pp, &o->o_y) ||
	     !pnum(pp, &o->o_x2) ||
	     !pnum(pp, &ON(o)[0]) || !pnum(pp, &ON(o)[1]) )
		return 0;
	if ( (t = tok(pp)) != 0 )
		parseattr(t, o);
	return 1;
}

static
p_dim(pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;

	o->o_type = OT_DIM;
	if ( !p4(pp, o) )
		return 0;
	while ( (t = opttok(pp, 0)) != 0 )
		parseattr(t, o);
	{
		char tb[TVMAX];

		resttext(*pp, tb, TVMAX);
		setoval(o, tb);
	}
	return 1;
}

static
p_fix(c, pp, o)
char **pp;
register DOBJ *o;
{
	register char *t;

	if ( c == 'N' )
	{
		o->o_type = OT_NNAME;
		if ( !pnum(pp, &o->o_x) || !pnum(pp, &o->o_y) )
			return 0;
		if ( (t = tok(pp)) == 0 )
			return 0;
		strncpy(o->o_name, t, NAMEL - 1);
		o->o_name[NAMEL - 1] = 0;
		return 1;
	}
	o->o_type = (c == 'W') ? OT_WIRE :
		    (c == 'L') ? OT_LINE :
		    (c == 'B') ? OT_BOX : OT_CIRC;
	if ( !p4(pp, o) )
		return 0;
	if ( (t = tok(pp)) != 0 )
		parseattr(t, o);
	return 1;
}

static
initobj(o)
register DOBJ *o;
{
	register int i;

	o->o_sym = 0;
	o->o_rot = o->o_mir = 0;
	o->o_x2 = o->o_y2 = 0;
	o->o_flags = 0;
	o->o_layer = 0;
	o->o_grp = 0;
	for ( i = 0; i < NAMEL; i++ )
		o->o_name[i] = 0;
	o->o_val[0] = 0;
	return 0;
}

static
pdisp(c, pp, o)
char **pp;
DOBJ *o;
{
	switch ( c )
	{
	case 'Y':	return p_sym(pp, o);
	case 'T':	return p_text(pp, o);
	case 'S':	return p_shape(pp, o);
	case 'K':	return p_conn(pp, o);
	case 'P':	return p_poly(pp, o);
	case 'A':	return p_arc(pp, o);
	case 'D':	return p_dim(pp, o);
	case 'W':
	case 'L':
	case 'B':
	case 'C':
	case 'N':	return p_fix(c, pp, o);
	}
	return 0;
}

/* Parse ONE drawing line into obj[nobj]; 1 when an object was added.
 * Unknown lines are skipped (the format's forward-compatibility rule). */
parseobj(lb)
char *lb;
{
	register DOBJ *o;
	char *p, *t;
	int i;

	p = lb;
	if ( (t = tok(&p)) == 0 || t[0] == '#' || t[0] == 'v' ||
	     t[0] == 'z' )		/* comments and the header lines */
		return 0;
	if ( t[0] == 'G' && t[1] == 0 )
	{
		if ( (t = tok(&p)) != 0 && (i = atoi(t)) > 0 )
		{
			pgrem = i;
			pgid = newgid();
		}
		return 0;
	}
	if ( t[0] == 'U' && t[1] == 0 )		/* sheet units header */
	{
		if ( (t = tok(&p)) != 0 && (i = atoi(t)) > 0 )
			unum = i;
		if ( (t = tok(&p)) != 0 && strcmp(t, "-") != 0 )
		{
			strncpy(uname, t, UNAMEL - 1);
			uname[UNAMEL - 1] = 0;
		}
		return 0;
	}
	if ( nobj >= MAXOBJ || t[1] != 0 )
		return 0;
	o = &obj[nobj];
	initobj(o);
	if ( !pdisp(t[0], &p, o) )
		return 0;
	if ( pgrem > 0 )
	{
		o->o_grp = pgid;
		pgrem--;
	}
	nobj++;
	return 1;
}

loadfile(fn)
char *fn;
{
	register FILE *fp;
	register int i;
	char lb[220];
	int e;

	if ( (fp = fopen(fn, "r")) == (FILE *)0 )
		return -1;
	selclear();
	nobj = 0;
	ppuse = 0;
	tpuse = 0;
	unum = 1;			/* until a U header says otherwise */
	uname[0] = 0;
	parsereset();
	while ( fgets(lb, sizeof(lb), fp) != 0 && nobj < MAXOBJ )
		parseobj(lb);
	fclose(fp);
	for ( i = 0; i < nobj; i++ )	/* re-resolve "@x,y" attachments */
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

