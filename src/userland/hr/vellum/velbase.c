/*
 * velbase.c - Vellum's MODEL layer: the object list, the symbol-library
 * pools, layer/sheet/selection state, and every piece of geometry that
 * never touches the screen (bboxes, symbol transforms, junction dots,
 * attachment points).  cl_*-free ON PURPOSE: the headless export binary
 * (velxport = velbase + velfile + velport) links this WITHOUT the gfx
 * shared library, so -print/-pic/-net run even on a machine with no
 * hi-res card.
 */
#include <stdio.h>
#include "vellum.h"

/* The object model, kinds, flags and pools are declared in vellum.h
 * (shared with velfile.c / velport.c); defined here. */
DOBJ	obj[MAXOBJ];
int	nobj;

/* polyline point pool: OT_POLY keeps ALL its points (x,y pairs) at
 * ppool[o_x2..]; o_sym is the count.  delobj() compacts. */
short	ppool[PPOOL];
int	ppuse;

/* ---- layers ---- */
int	curlayer;		/* new objects land here                  */
char	layvis[NLAYER]	= { 1, 1, 1, 1 };
char	layprn[NLAYER]	= { 1, 1, 1, 0 };   /* [3] stays 0: never prints */

/* One-level undo: the snapshot ARRAYS live in vellum.c now (editor-only
 * data; the headless exporter used to carry the 17 KB purely because
 * they sat next to the model).  Only the validity flag stays here --
 * velfile.c's loadfile clears it, and velfile links into BOTH images. */
int	uvalid;

int	gridstep = 1;		/* snap pitch, 1 or 2 grid units (Settings) */

/* The sheet is a Settings PRESET now (160x120, or A4-at-8-dots 120x168);
 * SHW/SHH (vellum.h) read the live size. */
int	v_shw	= 160;		/* sheet width,  grid units               */
int	v_shh	= 120;		/* sheet height, grid units               */

/* Sheet units (the "U 5 mm" header): one grid unit = unum uname.  Only
 * dimension LABELS multiply -- coordinates stay grid units. */
int	unum	= 1;
char	uname[UNAMEL];

int	gsc	= GRID;		/* px per grid unit ON SCREEN: the zoom.  */
				/* 4 / 8 / 16; grid coordinates never     */
				/* change, only this rendering scale      */

int	voxg, voyg;		/* pan: grid unit at the canvas origin    */
int	selobj	= -1;		/* PRIMARY selection (-1 = none): what    */
				/* the single-object actions (Rot, Name)  */
				/* work on -- valid only when nsel == 1   */
char	osel[MAXOBJ];		/* the SELECTION SET: Move, Delete and    */
int	nsel;			/* Dup work on every flagged object       */

int	modified;

/* ---- junction dots (recomputed after any wire change) ---- */
#define	MAXJUNC	128
short	juncx[MAXJUNC], juncy[MAXJUNC];
int	njunc;

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

/* ---- the TEXT pool (v3.3, VELLUM.md sec. 29): long T/S/D values.
 * A value longer than VALL-1 keeps a marker in the DOBJ (o_val[0] == 1,
 * pool offset as a short at o_val+2) and the string lives here; readers
 * go through oval()/ovalp(), writers through setoval(), and tvfree()
 * compacts on delete -- ppool's exact pattern.  (1 cannot begin a real
 * text: resttext never stores control bytes.) ---- */
char	tpool[TPOOL];
int	tpuse;

char *
ovalp(o, tp)
register DOBJ *o;
char *tp;
{
	if ( o->o_val[0] == 1 )
		return tp + *(short *)(o->o_val + 2);
	return o->o_val;
}

char *
oval(o)
DOBJ *o;
{
	return ovalp(o, tpool);
}

/* Drop o's pool block (delete / overwrite): compact, fix every offset. */
tvfree(o)
register DOBJ *o;
{
	register int i;
	int off, len;

	if ( o->o_val[0] != 1 )
		return 0;
	off = *(short *)(o->o_val + 2);
	len = strlen(&tpool[off]) + 1;
	for ( i = off; i + len <= tpuse; i++ )
		tpool[i] = tpool[i + len];
	tpuse -= len;
	for ( i = 0; i < nobj; i++ )
		if ( obj[i].o_val[0] == 1 &&
		     *(short *)(obj[i].o_val + 2) > off )
			*(short *)(obj[i].o_val + 2) -= len;
	o->o_val[0] = 0;
	return 0;
}

/* Store s as o's value: inline when it fits, else pooled (truncated to
 * the pool's TVMAX ceiling; a FULL pool falls back to the inline
 * truncation -- old behaviour, never an error). */
setoval(o, s)
register DOBJ *o;
register char *s;
{
	register int n;

	tvfree(o);
	n = strlen(s);
	if ( n < VALL )
	{
		strcpy(o->o_val, s);
		return 0;
	}
	if ( n > TVMAX - 1 )
		n = TVMAX - 1;
	if ( tpuse + n + 1 > TPOOL )
	{
		strncpy(o->o_val, s, VALL - 1);
		o->o_val[VALL - 1] = 0;
		return 0;
	}
	o->o_val[0] = 1;
	*(short *)(o->o_val + 2) = tpuse;
	strncpy(&tpool[tpuse], s, n);
	tpool[tpuse + n] = 0;
	tpuse += n + 1;
	return 0;
}

/* Integer square root -- radius math everywhere. */
long
isqrt(v)
long v;
{
	register long r, b;

	r = 0;
	b = 0x40000000L;
	while ( b > v )
		b >>= 2;
	while ( b )
	{
		if ( v >= r + b )
		{
			v -= r + b;
			r = (r >> 1) + b;
		}
		else
			r >>= 1;
		b >>= 2;
	}
	return r;
}

/* The dimension label: any text overrides; empty = AUTO, the measured
 * distance times the sheet unit, integer formatted ("85 mm").  A
 * horizontal or vertical dimension measures its axis, a diagonal one
 * point-to-point (isqrt). */
dimlbl(o, buf)
register DOBJ *o;
char *buf;
{
	long d, dx, dy;
	register char *v;

	v = oval(o);
	if ( v[0] )
	{
		strcpy(buf, v);
		return 0;
	}
	dx = o->o_x2 - o->o_x;	if ( dx < 0 ) dx = -dx;
	dy = o->o_y2 - o->o_y;	if ( dy < 0 ) dy = -dy;
	if ( dy == 0 )
		d = dx;
	else if ( dx == 0 )
		d = dy;
	else
		d = isqrt(dx * dx + dy * dy);
	d *= unum;
	if ( uname[0] )
		sprintf(buf, "%ld %s", d, uname);
	else
		sprintf(buf, "%ld", d);
	return 0;
}

/* Chorded quadratic B-spline through a polyline's DEVICE points (2n ints
 * in xy[]): the curve runs from the first point, through the midpoint of
 * each interior edge with the vertex as control, to the last -- chords
 * from integer midpoint subdivision (the flatarc trick generalized).
 * cl_-free: the editor's emit draws styled canvas lines, the exporters'
 * emit feeds a backend. */
static
bsseg(x0, y0, cx, cy, x1, y1, emit, dep)
int (*emit)();
{
	int mx0, my0, mx1, my1, mx, my;

	if ( dep <= 0 )
	{
		(*emit)(x0, y0, x1, y1);
		return 0;
	}
	mx0 = (x0 + cx) / 2;	my0 = (y0 + cy) / 2;
	mx1 = (cx + x1) / 2;	my1 = (cy + y1) / 2;
	mx = (mx0 + mx1) / 2;	my = (my0 + my1) / 2;
	bsseg(x0, y0, mx0, my0, mx, my, emit, dep - 1);
	bsseg(mx, my, mx1, my1, x1, y1, emit, dep - 1);
	return 0;
}

bspline(xy, n, emit)
register int *xy;
int (*emit)();
{
	register int i;
	int ax, ay, bx, by;

	if ( n < 3 )
	{
		if ( n == 2 )
			(*emit)(xy[0], xy[1], xy[2], xy[3]);
		return 0;
	}
	ax = xy[0];
	ay = xy[1];
	for ( i = 1; i < n - 1; i++ )
	{
		if ( i == n - 2 )
		{
			bx = xy[2 * n - 2];
			by = xy[2 * n - 1];
		}
		else
		{
			bx = (xy[2*i] + xy[2*i + 2]) / 2;
			by = (xy[2*i + 1] + xy[2*i + 3]) / 2;
		}
		bsseg(ax, ay, xy[2*i], xy[2*i + 1], bx, by, emit, 3);
		ax = bx;
		ay = by;
	}
	return 0;
}

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

/* grid <-> canvas pixels, through the zoom scale gsc */
gtopx(gx)
{
	return PALW + (gx - voxg) * gsc;
}

gtopy(gy)
{
	return CANY + (gy - voyg) * gsc;
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

/* Pixel bbox of symbol si at (rot,mir) around canvas grid point (gx,gy). */
sympbox(si, rot, mir, gx, gy, bx0, by0, bx1, by1)
int *bx0, *by0, *bx1, *by1;
{
	int ox, oy, x, y, i;
	int cx[4], cy[4];
	register SYMDEF *s;

	register int ppq;

	s = &symtab[si];
	ox = gtopx(gx);
	oy = gtopy(gy);
	ppq = gsc >= 4 ? gsc / 4 : 1;	/* the overview zoom floors at 1 */
	txq(s->sy_x0, s->sy_y0, rot, mir, ox, oy, ppq, &cx[0], &cy[0]);
	txq(s->sy_x1, s->sy_y0, rot, mir, ox, oy, ppq, &cx[1], &cy[1]);
	txq(s->sy_x0, s->sy_y1, rot, mir, ox, oy, ppq, &cx[2], &cy[2]);
	txq(s->sy_x1, s->sy_y1, rot, mir, ox, oy, ppq, &cx[3], &cy[3]);
	*bx0 = *bx1 = cx[0];
	*by0 = *by1 = cy[0];
	for ( i = 1; i < 4; i++ )
	{
		x = cx[i];
		y = cy[i];
		if ( x < *bx0 ) *bx0 = x;
		if ( x > *bx1 ) *bx1 = x;
		if ( y < *by0 ) *by0 = y;
		if ( y > *by1 ) *by1 = y;
	}
	return 0;
}

/* Longest '|'-separated line and line count of a label-block text. */
textdims(s, pnl)
char *s;
int *pnl;
{
	register int w, mw, nl;

	mw = w = 0;
	nl = 1;
	for ( ; *s; s++ )
	{
		if ( *s == '|' )
		{
			if ( w > mw ) mw = w;
			w = 0;
			nl++;
		}
		else
			w++;
	}
	if ( w > mw ) mw = w;
	*pnl = nl;
	return mw;
}

/* Pixel bbox of object *o whose polyline points live in pool pp and
 * whose long text lives in pool tp (the live list passes ppool/tpool;
 * the undo differ passes the snapshot's pools). */
objpbox2(o, pp, tp, bx0, by0, bx1, by1)
DOBJ *o;
short *pp;
char *tp;
int *bx0, *by0, *bx1, *by1;
{
	register int k;
	int x0, y0, x1, y1, t, nl;
	long dx, dy, r;

	switch ( o->o_type )
	{
	case OT_SYM:
		sympbox(o->o_sym, o->o_rot, o->o_mir, o->o_x, o->o_y,
			bx0, by0, bx1, by1);
		return 0;

	case OT_CIRC:
		dx = (long)(o->o_x2 - o->o_x) * gsc;
		dy = (long)(o->o_y2 - o->o_y) * gsc;
		r = isqrt(dx * dx + dy * dy);
		x0 = gtopx(o->o_x);
		y0 = gtopy(o->o_y);
		*bx0 = x0 - (int)r;
		*by0 = y0 - (int)r;
		*bx1 = x0 + (int)r;
		*by1 = y0 + (int)r;
		return 0;

	case OT_ARC:
		x0 = gtopx(o->o_x);
		y0 = gtopy(o->o_y);
		t = o->o_x2 * gsc;
		*bx0 = x0 - t;
		*by0 = y0 - t;
		*bx1 = x0 + t;
		*by1 = y0 + t;
		return 0;

	case OT_TEXT:
		/* fixed metrics, NOT hr_font(): this path also runs
		 * headless (exports), where the VRAM tail is unmapped */
		x0 = gtopx(o->o_x);
		y0 = gtopy(o->o_y);
		t = textdims(ovalp(o, tp), &nl);
		*bx0 = x0;
		*by0 = y0;
		if ( o->o_flags & OF_VERT )	/* turned 90: w and h swap */
		{
			*bx1 = x0 + nl * (o->o_rot <= 0 ? 8 : o->o_rot == 1
							? 15 : 16);
			*by1 = y0 + t * (o->o_rot <= 0 ? 6 : o->o_rot == 1
							? 8 : 9);
			return 0;
		}
		*bx1 = x0 + t * (o->o_rot <= 0 ? 6 : o->o_rot == 1 ? 8 : 9);
		*by1 = y0 + nl * (o->o_rot <= 0 ? 8 : o->o_rot == 1 ? 15
								    : 16);
		return 0;

	case OT_NNAME:
		x0 = gtopx(o->o_x);
		y0 = gtopy(o->o_y);
		*bx0 = x0 - 2;
		*by0 = y0 - 10;
		*bx1 = x0 + 4 + strlen(o->o_name) * 6;
		*by1 = y0 + 3;
		return 0;

	case OT_DIM:
		{
			int lw;

			/* generous: covers ticks, arrows and the label in
			 * either placement (above a horizontal line, right
			 * of a vertical one) without placement math.  A
			 * FIXED label allowance (the pool ceiling), so this
			 * path never resolves the text -- the undo differ
			 * passes snapshot objects whose pooled labels are
			 * not reachable through the live pool. */
			lw = TVMAX * 3 + 6;
			x0 = gtopx(o->o_x);   y0 = gtopy(o->o_y);
			x1 = gtopx(o->o_x2);  y1 = gtopy(o->o_y2);
			if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
			if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
			*bx0 = x0 - 5 - lw;
			*by0 = y0 - 18;
			*bx1 = x1 + 5 + 2 * lw;
			*by1 = y1 + 6;
		}
		return 0;

	case OT_POLY:
		x0 = x1 = gtopx(pp[o->o_x2]);
		y0 = y1 = gtopy(pp[o->o_x2 + 1]);
		for ( k = 1; k < o->o_sym; k++ )
		{
			t = gtopx(pp[o->o_x2 + 2 * k]);
			if ( t < x0 ) x0 = t;
			if ( t > x1 ) x1 = t;
			t = gtopy(pp[o->o_x2 + 2 * k + 1]);
			if ( t < y0 ) y0 = t;
			if ( t > y1 ) y1 = t;
		}
		*bx0 = x0;  *by0 = y0;  *bx1 = x1;  *by1 = y1;
		return 0;
	}
	x0 = gtopx(o->o_x);
	y0 = gtopy(o->o_y);
	x1 = gtopx(o->o_x2);
	y1 = gtopy(o->o_y2);
	if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
	if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
	*bx0 = x0;
	*by0 = y0;
	*bx1 = x1;
	*by1 = y1;
	return 0;
}

/* Pixel bbox of live object i. */
objpbox(i, bx0, by0, bx1, by1)
int *bx0, *by0, *bx1, *by1;
{
	return objpbox2(&obj[i], ppool, tpool, bx0, by0, bx1, by1);
}

/* Floor/ceil px -> grid conversions (negatives round correctly). */
gfloor(px, org)
{
	px -= org;
	return (px >= 0) ? px / gsc : -((-px + gsc - 1) / gsc);
}

gceil(px, org)
{
	px -= org;
	return (px >= 0) ? (px + gsc - 1) / gsc : -((-px) / gsc);
}

/* GRID-unit bbox of object i (from the px bbox, so labels and symbol
 * geometry are included) -- exports, zoom-to-fit and paste offsets. */
objgbox(i, gx0, gy0, gx1, gy1)
int *gx0, *gy0, *gx1, *gy1;
{
	int x0, y0, x1, y1;

	objpbox(i, &x0, &y0, &x1, &y1);
	*gx0 = voxg + gfloor(x0, PALW);
	*gy0 = voyg + gfloor(y0, CANY);
	*gx1 = voxg + gceil(x1, PALW);
	*gy1 = voyg + gceil(y1, CANY);
	return 0;
}

/* Is grid point (px,py) ON wire o (either leg, endpoints included)? */
onwire(o, px, py)
DOBJ *o;
{
	register int a, b;

	if ( o->o_x != o->o_x2 && py == o->o_y )	/* horizontal leg */
	{
		a = o->o_x;  b = o->o_x2;
		if ( a > b ) { a = o->o_x2;  b = o->o_x; }
		if ( px >= a && px <= b )
			return 1;
	}
	if ( o->o_y != o->o_y2 && px == o->o_x2 )	/* vertical leg */
	{
		a = o->o_y;  b = o->o_y2;
		if ( a > b ) { a = o->o_y2;  b = o->o_y; }
		if ( py >= a && py <= b )
			return 1;
	}
	return 0;
}

addjunc(px, py)
{
	register int k;

	for ( k = 0; k < njunc; k++ )
		if ( juncx[k] == px && juncy[k] == py )
			return 0;
	if ( njunc < MAXJUNC )
	{
		juncx[njunc] = px;
		juncy[njunc] = py;
		njunc++;
	}
	return 0;
}

/* Recompute the junction-dot list.  A dot marks a CONNECTION of three or
 * more conductors, the schematic convention:
 *   - a wire endpoint on the MIDDLE of another wire (a T);
 *   - three or more wire endpoints meeting at one point;
 *   - two wire endpoints meeting ON A SYMBOL PIN (wire + wire + pin).
 * Two wires merely continuing end-to-end stay dotless. */
rejunc()
{
	register int i, j, e;
	int px, py, cnt;
	DOBJ *o, *w;

	njunc = 0;
	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_WIRE )
			continue;
		for ( e = 0; e < 2; e++ )
		{
			px = e ? o->o_x2 : o->o_x;
			py = e ? o->o_y2 : o->o_y;
			cnt = 0;		/* wires ENDING here, self included */
			for ( j = 0; j < nobj; j++ )
			{
				w = &obj[j];
				if ( w->o_type != OT_WIRE )
					continue;
				if ( (px == w->o_x && py == w->o_y) ||
				     (px == w->o_x2 && py == w->o_y2) )
				{
					cnt++;
					continue;
				}
				if ( j != i && onwire(w, px, py) )
				{
					addjunc(px, py);	/* a T */
					break;
				}
			}
			if ( cnt >= 3 || (cnt >= 2 && pinat(px, py)) )
				addjunc(px, py);
		}
	}
	return 0;
}

selclear()
{
	register int i;

	for ( i = 0; i < nobj; i++ )
		osel[i] = 0;
	nsel = 0;
	selobj = -1;
	return 0;
}

/* Is any symbol pin at grid point (gx,gy)?  (For the junction rule: a pin
 * where two wires end is a three-way connection and gets a dot.) */
pinat(gx, gy)
{
	register DOBJ *o;
	register short *pp;
	int i, k, qx, qy;

	for ( i = 0; i < nobj; i++ )
	{
		o = &obj[i];
		if ( o->o_type != OT_SYM )
			continue;
		pp = symtab[o->o_sym].sy_pins;
		if ( pp == 0 )
			continue;
		for ( k = 0; k < pp[0]; k++ )
		{
			txq(pp[1 + 2*k], pp[2 + 2*k], o->o_rot, o->o_mir,
			    0, 0, 1, &qx, &qy);
			if ( o->o_x + qx / 4 == gx && o->o_y + qy / 4 == gy )
				return 1;
		}
	}
	return 0;
}

natt(i)
{
	register DOBJ *o;
	register short *pp;

	o = &obj[i];
	if ( o->o_type == OT_SHAPE )
		return 8;
	if ( o->o_type == OT_SYM )
	{
		pp = symtab[o->o_sym].sy_pins;
		return pp ? pp[0] : 0;
	}
	return 0;
}

/* Grid position of attachment point k of object i; -1 if out of range. */
attpos(i, k, gx, gy)
int *gx, *gy;
{
	register DOBJ *o;
	register short *pp;
	int x0, y0, x1, y1, t, qx, qy;

	o = &obj[i];
	if ( o->o_type == OT_SHAPE )
	{
		if ( k < 0 || k >= 8 )
			return -1;
		x0 = o->o_x;  x1 = o->o_x2;
		if ( x1 < x0 ) { t = x0; x0 = x1; x1 = t; }
		y0 = o->o_y;  y1 = o->o_y2;
		if ( y1 < y0 ) { t = y0; y0 = y1; y1 = t; }
		switch ( k )
		{
		case 0:	*gx = (x0 + x1) / 2;  *gy = y0;		break;
		case 1:	*gx = x1;  *gy = (y0 + y1) / 2;		break;
		case 2:	*gx = (x0 + x1) / 2;  *gy = y1;		break;
		case 3:	*gx = x0;  *gy = (y0 + y1) / 2;		break;
		case 4:	*gx = x0;  *gy = y0;			break;
		case 5:	*gx = x1;  *gy = y0;			break;
		case 6:	*gx = x1;  *gy = y1;			break;
		case 7:	*gx = x0;  *gy = y1;			break;
		}
		return 0;
	}
	if ( o->o_type == OT_SYM )
	{
		pp = symtab[o->o_sym].sy_pins;
		if ( pp == 0 || k < 0 || k >= pp[0] )
			return -1;
		txq(pp[1 + 2*k], pp[2 + 2*k], o->o_rot, o->o_mir,
		    0, 0, 1, &qx, &qy);
		*gx = o->o_x + qx / 4;
		*gy = o->o_y + qy / 4;
		return 0;
	}
	return -1;
}

/* Re-resolve a LOADED connector endpoint (saved as "@x,y"): the stored
 * grid point matched against every attachment point, nearest within one
 * unit -- object indices are not stable across saves, grid points are. */
resolveatt(ci, e, lo)
{
	register DOBJ *o;
	register int i, k;
	int n, gx, gy, tx, ty, dx, dy, d, best;

	o = &obj[ci];
	tx = e ? o->o_x2 : o->o_x;
	ty = e ? o->o_y2 : o->o_y;
	best = 999;
	ACOBJ(o, e) = -1;
	for ( i = lo; i < nobj; i++ )
	{
		if ( i == ci )
			continue;
		n = natt(i);
		for ( k = 0; k < n; k++ )
		{
			attpos(i, k, &gx, &gy);
			dx = gx - tx;	if ( dx < 0 ) dx = -dx;
			dy = gy - ty;	if ( dy < 0 ) dy = -dy;
			if ( dx > 1 || dy > 1 )
				continue;
			d = dx + dy;
			if ( d < best )
			{
				best = d;
				ACOBJ(o, e) = i;
				ACPT(o, e) = k;
			}
		}
	}
	return 0;
}
