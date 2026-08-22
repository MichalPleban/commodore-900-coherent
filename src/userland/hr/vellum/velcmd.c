/*
 * velcmd.c - Vellum's editing commands: z-order (Front/Back), persistent
 * groups, align/distribute, the clipboard (Cut/Copy/Paste as .d text --
 * the plain-text format doing interop), the frame/title-block stamp, and
 * the Settings and Style dialogs.
 *
 * Everything here mutates the object list through vellum.c's services
 * (snapshot / dmg / moveobj / objmove / reroute), so the damage rules
 * hold: each command damages exactly what it touched.
 */
#include <stdio.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrdlg.h"
#include "vellum.h"

/* ------------------------------------------------------------------ */
/* z-order: the object list IS the z-order; Front/Back move the whole  */
/* selection to the list ends, keeping its internal order.             */
/* ------------------------------------------------------------------ */

/* Collect the selected indices, ascending; returns the count. */
static
selidx(idx)
short *idx;
{
	register int i, n;

	n = 0;
	for ( i = 0; i < nobj; i++ )
		if ( osel[i] )
			idx[n++] = i;
	return n;
}

tofront()
{
	short idx[MAXOBJ];
	register int j;
	int n;

	n = selidx(idx);
	if ( n == 0 || n == nobj )
		return 0;
	snapshot();
	for ( j = 0; j < n; j++ )
		objmove(idx[j] - j, nobj - 1);
	dmgsel();		/* the moved objects repaint in new order */
	return 1;
}

toback()
{
	short idx[MAXOBJ];
	register int j;
	int n;

	n = selidx(idx);
	if ( n == 0 || n == nobj )
		return 0;
	snapshot();
	for ( j = n - 1; j >= 0; j-- )
		objmove(idx[j] + (n - 1 - j), 0);
	dmgsel();
	return 1;
}

/* ------------------------------------------------------------------ */
/* persistent groups: contiguous runs sharing o_grp; a click on a      */
/* member selects the whole group (vellum.c selone), Save writes "G n" */
/* headers.  Grouping compacts the members so the run IS contiguous.   */
/* ------------------------------------------------------------------ */

dogroup()
{
	short idx[MAXOBJ];
	register int j;
	int n, g, f;

	n = selidx(idx);
	if ( n < 2 )
		return 0;
	snapshot();
	g = newgid();
	if ( g == 0 )
		return 0;
	f = idx[0];
	for ( j = 0; j < n; j++ )
		objmove(idx[j], f + j);
	for ( j = 0; j < n; j++ )
		obj[f + j].o_grp = g;
	modified = 1;
	dmgsel();		/* compaction can change stacking */
	statdirty = 1;
	return 1;
}

doungroup()
{
	register int i;
	int any;

	any = 0;
	for ( i = 0; i < nobj; i++ )
		if ( osel[i] && obj[i].o_grp )
		{
			obj[i].o_grp = 0;
			any = 1;
		}
	if ( any )
	{
		modified = 1;
		statdirty = 1;
	}
	return any;
}

/* ------------------------------------------------------------------ */
/* align / distribute (keys 1-6 with a multi-selection)                */
/* ------------------------------------------------------------------ */

doalign(op)
{
	short idx[MAXOBJ];
	register int j, k;
	int n, x0, y0, x1, y1, ux0, uy0, ux1, uy1, d, t;
	short w[MAXOBJ];

	n = selidx(idx);
	if ( n < 2 || (op >= 5 && n < 3) )
		return 0;
	snapshot();
	dmgsel();
	objgbox((int)idx[0], &ux0, &uy0, &ux1, &uy1);
	for ( j = 1; j < n; j++ )
	{
		objgbox((int)idx[j], &x0, &y0, &x1, &y1);
		if ( x0 < ux0 ) ux0 = x0;
		if ( y0 < uy0 ) uy0 = y0;
		if ( x1 > ux1 ) ux1 = x1;
		if ( y1 > uy1 ) uy1 = y1;
	}
	if ( op <= 4 )
	{
		for ( j = 0; j < n; j++ )
		{
			objgbox((int)idx[j], &x0, &y0, &x1, &y1);
			switch ( op )
			{
			case 1:	moveobj((int)idx[j], ux0 - x0, 0);	break;
			case 2:	moveobj((int)idx[j], ux1 - x1, 0);	break;
			case 3:	moveobj((int)idx[j], 0, uy0 - y0);	break;
			case 4:	moveobj((int)idx[j], 0, uy1 - y1);	break;
			}
		}
	}
	else
	{
		/* distribute evenly: sort by leading edge, keep the two
		 * extremes, spread the gaps equally between the rest */
		int span, total, at, gap, rem;

		for ( j = 0; j < n; j++ )
		{
			objgbox((int)idx[j], &x0, &y0, &x1, &y1);
			w[j] = (op == 5) ? x1 - x0 : y1 - y0;
		}
		for ( j = 1; j < n; j++ )	/* insertion sort by edge */
			for ( k = j; k > 0; k-- )
			{
				int ax0, ay0, ax1, ay1, bx0, by0, bx1, by1;

				objgbox((int)idx[k - 1], &ax0, &ay0,
					&ax1, &ay1);
				objgbox((int)idx[k], &bx0, &by0, &bx1, &by1);
				if ( (op == 5 ? bx0 < ax0 : by0 < ay0) )
				{
					t = idx[k];  idx[k] = idx[k - 1];
					idx[k - 1] = t;
					t = w[k];  w[k] = w[k - 1];
					w[k - 1] = t;
				}
				else
					break;
			}
		span = (op == 5) ? ux1 - ux0 : uy1 - uy0;
		total = 0;
		for ( j = 0; j < n; j++ )
			total += w[j];
		d = span - total;
		if ( d < 0 ) d = 0;
		gap = d / (n - 1);
		rem = d - gap * (n - 1);
		at = (op == 5) ? ux0 : uy0;
		for ( j = 0; j < n; j++ )
		{
			objgbox((int)idx[j], &x0, &y0, &x1, &y1);
			if ( op == 5 )
				moveobj((int)idx[j], at - x0, 0);
			else
				moveobj((int)idx[j], 0, at - y0);
			at += w[j] + gap + (j < rem ? 1 : 0);
		}
	}
	dmgsel();
	reroute();
	rejunc();
	statdirty = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* clipboard: the selection serialized as .d TEXT into the hr          */
/* clipboard -- pasteable into another Vellum, or into zedit as text.  */
/* ------------------------------------------------------------------ */

docopy()
{
	char lb[224];
	register int i;

	if ( nsel == 0 )
		return 0;
	if ( hr_clipopen() < 0 )
		return 0;
	hr_clipwrite("vellum1\n", 8);
	for ( i = 0; i < nobj; i++ )
	{
		if ( !osel[i] )
			continue;
		fmtobj(i, lb);
		if ( lb[0] == 0 )
			continue;
		strcat(lb, "\n");
		hr_clipwrite(lb, strlen(lb));
	}
	hr_clipclose();
	statdirty = 1;
	return 1;
}

docut()
{
	register int j;

	if ( nsel == 0 )
		return 0;
	if ( !docopy() )
		return 0;
	snapshot();
	for ( j = nobj - 1; j >= 0; j-- )
		if ( osel[j] )
		{
			dmgobj(j);
			delobj(j);
		}
	selclear();
	reroute();
	statdirty = 1;
	return 1;
}

/* Paste at grid (gx,gy): parse the lines (unknown ones skip -- pasting
 * arbitrary text is harmless), offset the new objects so their top-left
 * lands at the point, select them.  clip = 1 reads the CLIPBOARD (menu
 * Paste); clip = 0 the PRIMARY selection (middle click), falling back
 * to the clipboard when no selection is up. */
dopaste(gx, gy, clip)
{
	char lb[224];
	register int i;
	long len, off;
	int n0, ll, e, x0, y0, x1, y1, mx, my, got;
	char cb[128];
	int nc, ci;

	len = clip ? hr_cliplen() : hr_sellen();
	if ( len <= 0 && !clip )
	{
		clip = 1;
		len = hr_cliplen();
	}
	if ( len <= 0 )
		return 0;
	snapshot();
	n0 = nobj;
	parsereset();
	off = 0;
	ll = 0;
	nc = ci = 0;
	while ( off < len || ci < nc )
	{
		if ( ci >= nc )
		{
			nc = clip ? hr_clipread(off, cb, sizeof(cb))
				  : hr_selread(off, cb, sizeof(cb));
			if ( nc <= 0 )
				break;
			off += nc;
			ci = 0;
		}
		while ( ci < nc )
		{
			char c;

			c = cb[ci++];
			if ( c == '\n' || ll >= sizeof(lb) - 2 )
			{
				lb[ll] = '\n';
				lb[ll + 1] = 0;
				parseobj(lb);
				ll = 0;
			}
			else
				lb[ll++] = c;
		}
	}
	if ( ll > 0 )
	{
		lb[ll] = '\n';
		lb[ll + 1] = 0;
		parseobj(lb);
	}
	if ( nobj == n0 )
		return 0;
	got = 0;
	for ( i = n0; i < nobj; i++ )	/* union bbox of the pasted set */
	{
		objgbox(i, &x0, &y0, &x1, &y1);
		if ( !got )
		{
			mx = x0;  my = y0;
			got = 1;
		}
		else
		{
			if ( x0 < mx ) mx = x0;
			if ( y0 < my ) my = y0;
		}
	}
	for ( i = n0; i < nobj; i++ )
		moveobj(i, gx - mx, gy - my);
	for ( i = n0; i < nobj; i++ )	/* attach within the pasted set */
		if ( obj[i].o_type == OT_CONN )
			for ( e = 0; e < 2; e++ )
				if ( ACOBJ(&obj[i], e) == -2 )
					resolveatt(i, e, n0);
	selclear();
	nsel = 0;
	for ( i = n0; i < nobj; i++ )
	{
		osel[i] = 1;
		nsel++;
	}
	selobj = (nsel == 1) ? n0 : -1;
	modified = 1;
	rejunc();
	dmgsel();
	statdirty = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* the frame stamp: border + title block as ORDINARY objects on the    */
/* frame layer -- editable afterwards, nothing magic.                  */
/* ------------------------------------------------------------------ */

extern long	time();
extern char	*ctime();

dostamp()
{
	register int i;
	int n0, bx;
	long t;
	char db[16];

	if ( nobj + 7 > MAXOBJ )
		return 0;
	snapshot();
	n0 = nobj;
	i = addobj(OT_BOX, 1, 1, SHW - 1, SHH - 1);	/* sheet border */
	if ( i >= 0 )
		obj[i].o_flags = OF_BOLD;
	bx = SHW - 45;
	addobj(OT_BOX, bx, SHH - 7, SHW - 1, SHH - 1);	/* title block */
	addobj(OT_LINE, bx, SHH - 4, SHW - 1, SHH - 4);
	i = addobj(OT_TEXT, bx + 1, SHH - 7, 0, 0);	/* file name */
	if ( i >= 0 )
	{
		obj[i].o_rot = 0;
		strncpy(obj[i].o_val, fname[0] ? fname : "(untitled)",
			VALL - 1);
		obj[i].o_val[VALL - 1] = 0;
	}
	time(&t);
	strncpy(db, ctime(&t) + 4, 6);		/* "Aug 21" */
	db[6] = ' ';
	strncpy(db + 7, ctime(&t) + 20, 4);	/* year */
	db[11] = 0;
	i = addobj(OT_TEXT, bx + 1, SHH - 4, 0, 0);	/* date */
	if ( i >= 0 )
	{
		obj[i].o_rot = 0;
		strcpy(obj[i].o_val, db);
	}
	i = addobj(OT_TEXT, SHW - 9, SHH - 4, 0, 0);	/* revision */
	if ( i >= 0 )
	{
		obj[i].o_rot = 0;
		strcpy(obj[i].o_val, "Rev A");
	}
	/* a numbered-set member stamps "Sheet n/m" (sec. 19): m = the
	 * highest sheet the set holds on disk */
	{
		char pre[FNLEN], suf[8], nn[FNLEN + 4];
		int n, m, fd;

		if ( sheetsplit(pre, &n, suf) )
		{
			m = n;
			for ( i = 1; i <= 40; i++ )
			{
				sprintf(nn, "%s%d%s", pre, i, suf);
				if ( (fd = open(nn, 0)) >= 0 )
				{
					close(fd);
					if ( i > m )
						m = i;
				}
				else if ( i > n )
					break;
			}
			i = addobj(OT_TEXT, SHW - 20, SHH - 7, 0, 0);
			if ( i >= 0 )
			{
				obj[i].o_rot = 0;
				sprintf(obj[i].o_val, "Sheet %d/%d", n, m);
			}
		}
	}
	for ( i = n0; i < nobj; i++ )
	{
		obj[i].o_layer = 2;		/* the frame layer */
		dmgobj(i);
	}
	modified = 1;
	statdirty = 1;
	return 1;
}

/* ------------------------------------------------------------------ */
/* the Settings and Style dialogs: the WIDGETS live in the spawned     */
/* helper (veldlgm.c); here only the marshal-out / apply-back halves.  */
/* ------------------------------------------------------------------ */

extern char	*dlgspawn();	/* veldlg.c: run one helper dialog        */

settingsdlg()
{
	char ag[4], as[6], au[8], vv[6], pp[5];
	register char *p, *t;
	register int i;
	int vch, nv;

	sprintf(ag, "%d", gridstep);
	sprintf(as, "%d", SHW);
	sprintf(au, "%d", unum);
	for ( i = 0; i < 4; i++ )
		vv[i] = '0' + layvis[i];
	vv[4] = 0;
	for ( i = 0; i < 3; i++ )
		pp[i] = '0' + layprn[i];
	pp[3] = 0;
	p = dlgspawn("set", ag, as, au, uname[0] ? uname : "-", vv, pp);
	if ( p == (char *)0 )
		return 0;
	/* payload: GRID SHW UNUM UNAME VVVV PPP FRAME */
	if ( (t = tok(&p)) != 0 )
		gridstep = atoi(t) == 2 ? 2 : 1;
	if ( (t = tok(&p)) != 0 )
	{
		i = atoi(t);
		if ( i != 160 && SHW == 160 )
		{
			SHW = 120;		/* A4 at 8 dots per unit */
			SHH = 168;
			viewdirty();
		}
		else if ( i == 160 && SHW != 160 )
		{
			SHW = 160;
			SHH = 120;
			viewdirty();
		}
	}
	clampvo();
	if ( (t = tok(&p)) != 0 )
	{
		i = atoi(t);
		if ( i < 1 )
			i = 1;
		nv = 0;
		if ( (t = tok(&p)) != 0 && strcmp(t, "-") != 0 )
		{
			if ( strcmp(t, uname) != 0 )
				nv = 1;
			strncpy(uname, t, UNAMEL - 1);
			uname[UNAMEL - 1] = 0;
		}
		else if ( uname[0] )
		{
			uname[0] = 0;
			nv = 1;
		}
		if ( i != unum || nv )
		{
			unum = i;		/* AUTO dim labels change */
			modified = 1;
			viewdirty();
		}
	}
	vch = 0;
	if ( (t = tok(&p)) != 0 && strlen(t) >= 4 )
		for ( i = 0; i < NLAYER; i++ )
		{
			nv = t[i] == '1';
			if ( nv != layvis[i] )
				vch = 1;
			layvis[i] = nv;
		}
	if ( (t = tok(&p)) != 0 && strlen(t) >= 3 )
		for ( i = 0; i < 3; i++ )
			layprn[i] = t[i] == '1';
	/* construction never prints: layprn[3] stays 0 */
	if ( vch )
		viewdirty();		/* visibility is a view change */
	if ( (t = tok(&p)) != 0 && atoi(t) )
		dostamp();		/* the Frame button */
	statdirty = 1;
	return 1;
}

styledlg(oi)
{
	register DOBJ *o;
	register char *p, *t;
	char af[6], al[4], az[4], txt[VALL];
	int hastext;

	o = &obj[oi];
	sprintf(af, "%d", o->o_flags & 0xff);
	sprintf(al, "%d", o->o_layer);
	sprintf(az, "%d", o->o_type == OT_TEXT ? o->o_rot : 2);
	hastext = o->o_type == OT_TEXT || o->o_type == OT_SHAPE ||
		  o->o_type == OT_NNAME || o->o_type == OT_DIM;
	if ( o->o_type == OT_NNAME )
		strcpy(txt, o->o_name);
	else if ( hastext )
		strcpy(txt, o->o_val);
	else
		strcpy(txt, "-");
	p = dlgspawn("style", af, al, az, txt, (char *)0);
	if ( p == (char *)0 )
		return 0;
	/* payload: FLAGS LAYER SIZE<TAB>TEXT */
	for ( t = p; *t && *t != '\t'; t++ )
		;
	if ( *t == '\t' )
		*t++ = 0;
	else
		t = "";
	snapshot();
	dmgobj(oi);			/* the bbox may shrink */
	{
		register char *w;

		if ( (w = tok(&p)) != 0 )
			o->o_flags = atoi(w) & 0xff;
		if ( (w = tok(&p)) != 0 )
			o->o_layer = atoi(w) & (NLAYER - 1);
		if ( (w = tok(&p)) != 0 && o->o_type == OT_TEXT )
			o->o_rot = atoi(w);
	}
	if ( o->o_type == OT_NNAME )
	{
		strncpy(o->o_name, t, NAMEL - 1);
		o->o_name[NAMEL - 1] = 0;
	}
	else if ( hastext && (t[0] || o->o_type == OT_DIM) )
	{
		strncpy(o->o_val, t, VALL - 1);
		o->o_val[VALL - 1] = 0;	/* an emptied dim goes AUTO */
	}
	dmgobj(oi);
	modified = 1;
	statdirty = 1;
	return 1;
}
