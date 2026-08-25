/*
 * vellib.c - the SYMBOL LIBRARIES: the .sym files, parsed into the
 * pools velsymg.c holds.
 *
 * Split from velfile.c (Aug 2026) for the same reason velsymg was
 * split from velbase: a client can want the stencils without wanting a
 * DRAWING.  velpal paints the editor's palette bank and calls exactly
 * one thing here -- loadlib -- but velfile.o also holds the .d format,
 * whose parsers write the object table, and Coherent's ld pulls a
 * member whole.  So loading a library used to cost 20 000 bytes of
 * drawing table that was never read.
 *
 * The rule for this file: nothing here may touch the drawing table,
 * the pools or the selection.  tok() lives here rather than beside the
 * .d parsers because it is the leaf of the two -- velfile.c may
 * reference this file, and this file may not reference velfile.c.
 */
#include <stdio.h>
#include "vellum.h"

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

/* One blank-separated NUMBER into a short; 0 at end of line.  Shared
 * with velfile.c, whose per-type .d parsers are little more than runs
 * of it -- which is why it is not static. */
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

/* Why loadlib DROPPED something, for `velcheck -sym' (sec. 56):
 * the loader has always dropped silently when a pool filled, and the
 * load order then decides which stencil a drawing gets.  libdrop counts
 * the symbols lost in the last loadlib call and libpool names the pool
 * that filled -- the caller clears them; nothing else pays. */
int	libdrop;
char	*libpool = "";

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
	int i, in, n0, o0, p0, cut;

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
	cut = 0;
	s = (SYMDEF *)0;
	while ( fgets(lb, sizeof(lb), fp) != 0 )
	{
		p = lb;
		if ( (t = tok(&p)) == 0 || t[0] == '#' )
			continue;
		if ( strcmp(t, "symbol") == 0 )
		{
			in = 0;
			cut = 0;
			if ( nsym >= MAXSYM ||
			     opuse + 1 >= SYMOPS || pinuse >= SYMPINS )
			{
				libdrop++;
				libpool = nsym >= MAXSYM ? "MAXSYM (80)" :
					  opuse + 1 >= SYMOPS ?
					  "SYMOPS (4800)" : "SYMPINS (600)";
				continue;
			}
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
			s->sy_nfile = 0;
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
			if ( cut )		/* geometry lost: a part */
				libdrop++;	/* that draws wrong      */
			cut = 0;
			continue;
		}
		if ( strcmp(t, "s") == 0 )
		{
			if ( opuse + 6 >= SYMOPS )
			{
				cut = 1;
				libpool = "SYMOPS (4800)";
				continue;
			}
			symops[opuse] = SE;
			if ( oprd(&p, 4) )
				opuse += 5;
		}
		else if ( strcmp(t, "c") == 0 )
		{
			if ( opuse + 5 >= SYMOPS )
			{
				cut = 1;
				libpool = "SYMOPS (4800)";
				continue;
			}
			symops[opuse] = SC;
			if ( oprd(&p, 3) )
				opuse += 4;
		}
		else if ( strcmp(t, "t") == 0 )
		{
			if ( opuse + 5 >= SYMOPS )
			{
				cut = 1;
				libpool = "SYMOPS (4800)";
				continue;
			}
			symops[opuse] = ST;
			if ( oprd(&p, 2) && (t = tok(&p)) != 0 )
			{
				symops[opuse + 3] = t[0];
				opuse += 4;
			}
		}
		else if ( strcmp(t, "a") == 0 )
		{
			if ( opuse + 7 >= SYMOPS )
			{
				cut = 1;
				libpool = "SYMOPS (4800)";
				continue;
			}
			symops[opuse] = SA;
			if ( oprd(&p, 5) )
				opuse += 6;
		}
		else if ( strcmp(t, "p") == 0 )
		{
			/* the file's pin count, kept or not: velcheck
			 * says so when the loader keeps only the first 8 */
			s->sy_nfile++;
			if ( pinuse + 2 >= SYMPINS )
			{
				cut = 1;
				libpool = "SYMPINS (600)";
				continue;
			}
			if ( s->sy_pins[0] >= 8 )
				continue;
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

