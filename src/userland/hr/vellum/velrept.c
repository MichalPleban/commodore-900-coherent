/*
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * velrept.c - naming an object the way a REPORT names it.
 *
 * veldiff's change lines and velinfo -where's hits describe the same
 * objects in the same words, so the description is the model's and not
 * either tool's.  It is a libvellum member of its OWN, small on
 * purpose: a member is pulled only for a symbol something references,
 * so the editor -- which prints no reports -- links none of this.
 */
#include <stdio.h>
#include "vellum.h"

/* One object, named the way a REPORT names it -- veldiff's change
 * lines and velinfo -where's hits are the same naming, so it is the
 * model that names an object and not either tool. */
ddesc(o, tp, buf)
register DOBJ *o;
char *tp, *buf;
{
	static char lt[] = " YWLBCTSKPAND";

	switch ( o->o_type )
	{
	case OT_SYM:
		if ( o->o_name[0] )
			strcpy(buf, o->o_name);
		else
			sprintf(buf, "Y %s", symtab[o->o_sym].sy_code);
		break;
	case OT_NNAME:
		sprintf(buf, "N %s", o->o_name);
		break;
	case OT_TEXT:
	case OT_SHAPE:
	case OT_DIM:
		sprintf(buf, "%c \"%.40s\"", lt[o->o_type], ovalp(o, tp));
		break;
	case OT_POLY:
		sprintf(buf, "P %d pts at %d,%d", o->o_sym, o->o_x, o->o_y);
		break;
	case OT_ARC:
		sprintf(buf, "A %d,%d r%d", o->o_x, o->o_y, o->o_x2);
		break;
	default:
		sprintf(buf, "%c %d,%d-%d,%d", lt[o->o_type], o->o_x,
			o->o_y, o->o_x2, o->o_y2);
		break;
	}
	return 0;
}

/* "(R 47k)" -- the part detail a symbol carries into a report. */
ddet(o, tp, buf)
register DOBJ *o;
char *tp, *buf;
{
	register char *v;

	buf[0] = 0;
	if ( o->o_type != OT_SYM || o->o_name[0] == 0 )
		return 0;	/* ddesc already said "Y GND": no echo */
	v = ovalp(o, tp);
	if ( v[0] )
		sprintf(buf, " (%s %.16s)", symtab[o->o_sym].sy_code, v);
	else
		sprintf(buf, " (%s)", symtab[o->o_sym].sy_code);
	return 0;
}
