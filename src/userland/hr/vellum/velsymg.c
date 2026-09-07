/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * velsymg.c - the SYMBOL half of the model: the parsed stencil
 * libraries and the geometry of symbol space.
 *
 * Split out of velbase.c (Aug 2026) because a client can want the
 * stencils without wanting a DRAWING.  The case that forced it was the
 * palette painter velpal (since retired back into the editor): holding
 * no drawing, no selection and touching no pool, it still linked the
 * whole model and carried 16 000 bytes of obj[] plus the text and
 * polyline pools, because every model global lived in one object file
 * and ld pulls a member whole.
 *
 * So the rule for this file: nothing here may touch the drawing table,
 * the pools, the selection or the view.  sympbox lives on the drawing
 * side even though it reads symtab, because it scales by the zoom.
 */
#include <stdio.h>
#include "vellum.h"

/* EVERY symbol is parsed from a library FILE (velfile.c loadlib) --
 * nothing is compiled in.  /usr/vellum/etc/libs names the libraries
 * loaded at start-up, the user scratch library is always tried after
 * them, and the toolbar Lib button loads more.  ALL loaded symbols stay
 * resolvable at once; the palette shows ONE group at a time. */
SYMDEF	symtab[MAXSYM];
int	nsym;

char	libname[MAXLIB][12];	/* palette-header name (file basename)    */
char	libpath[MAXLIB][44];	/* the file itself, for the Edit button   */
int	nlib;
int	curlib;			/* group the palette shows                */

/* Pools the parsed symbol data lives in (linear; a library that would
 * overflow them is truncated, never overrun). */
short	symops[SYMOPS];
short	sympin[SYMPINS];
char	symcode[MAXSYM][8];
char	symprefix[MAXSYM][4];
int	opuse, pinuse;		/* pool cursors                           */

/* Pin NAMES (v1.4, for netlists: Q1.B instead of Q1.2) -- see vellum.h. */
char	pnmpool[PNMPOOL];
int	pnmuse	= 1;		/* [0] reserved: 0 means unnamed          */
short	pinnm[SYMPINS / 2];
char	pintyp[SYMPINS / 2];	/* pin TYPES (v4.4): 'i' 'o' 'p' 'b' / 0  */

/* Transform a symbol-space q point (mirror, then rot quarter turns) and
 * scale it onto the screen: ppq px per q unit around (ox,oy). */
txq(x, y, rot, mir, ox, oy, ppq, px, py)
int *px, *py;
{
	register int t;

	if ( mir )
		x = -x;
	switch ( rot & 3 )
	{
	case 1:	t = x;  x = -y;  y = t;  break;
	case 2:	x = -x;  y = -y;  break;
	case 3:	t = x;  x = y;  y = -t;  break;
	}
	*px = ox + x * ppq;
	*py = oy + y * ppq;
}

/* Compute every symbol's q bbox once, from its op list (ST counts as a
 * 3x4 q char cell). */
symbounds()
{
	register short *p;
	register int i;
	int x0, y0, x1, y1;

	for ( i = 0; i < nsym; i++ )
	{
		x0 = y0 = 999;
		x1 = y1 = -999;
		p = symtab[i].sy_ops;
		while ( *p != SEND )
		{
			if ( *p == SE )
			{
				if ( p[1] < x0 ) x0 = p[1];
				if ( p[3] < x0 ) x0 = p[3];
				if ( p[1] > x1 ) x1 = p[1];
				if ( p[3] > x1 ) x1 = p[3];
				if ( p[2] < y0 ) y0 = p[2];
				if ( p[4] < y0 ) y0 = p[4];
				if ( p[2] > y1 ) y1 = p[2];
				if ( p[4] > y1 ) y1 = p[4];
				p += 5;
			}
			else if ( *p == SC || *p == SA )
			{
				if ( p[1] - p[3] < x0 ) x0 = p[1] - p[3];
				if ( p[1] + p[3] > x1 ) x1 = p[1] + p[3];
				if ( p[2] - p[3] < y0 ) y0 = p[2] - p[3];
				if ( p[2] + p[3] > y1 ) y1 = p[2] + p[3];
				p += (*p == SA) ? 6 : 4;
			}
			else
			{
				if ( p[1] < x0 ) x0 = p[1];
				if ( p[1] + 3 > x1 ) x1 = p[1] + 3;
				if ( p[2] < y0 ) y0 = p[2];
				if ( p[2] + 4 > y1 ) y1 = p[2] + 4;
				p += 4;
			}
		}
		if ( x0 > x1 )			/* an empty (custom) symbol */
		{
			x0 = y0 = 0;
			x1 = y1 = 4;
		}
		symtab[i].sy_x0 = x0;
		symtab[i].sy_y0 = y0;
		symtab[i].sy_x1 = x1;
		symtab[i].sy_y1 = y1;
	}
	return 0;
}
