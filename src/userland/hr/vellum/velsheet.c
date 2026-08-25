/*
 * velsheet.c - one SHEET of a command line: the pipe form, or a file.
 *
 * Every tool takes its sheets this way, so "-" means the same thing
 * everywhere and the cannot-open line is written once.  A libvellum
 * member of its OWN, velrept's reason exactly: the EDITOR opens files
 * through a dialog and never reads a sheet off a command line, so it
 * should not link this -- and a member is pulled only for a symbol
 * something actually references.
 */
#include <stdio.h>
#include "vellum.h"

/* ================================================================== */
/* The file name "-" is STDIN (sec. 49): it is what makes the tools    */
/* composable with EACH OTHER --                                       */
/*	velinfo -symsheet pid.sym | velplot -T ps -fit -                */
/* puts a generated drawing on paper with no temp file anywhere.  The  */
/* format unit already exposes the parser this needs -- parseobj has   */
/* been "shared by Open, clipboard Paste and any script that feeds     */
/* lines in" since v1.3 -- so the pipe form is loadfile's twin and     */
/* lives beside it, in the library every tool links.                   */
/* ================================================================== */

loadstdin()
{
	register int i;
	char lb[220];
	int e;

	selclear();
	nobj = 0;
	ppuse = 0;
	tpuse = 0;
	unum = 1;
	uname[0] = 0;
	parsereset();
	while ( fgets(lb, sizeof(lb), stdin) != 0 && nobj < MAXOBJ )
		parseobj(lb);
	for ( i = 0; i < nobj; i++ )
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

/* One SHEET of a command line: the pipe form or a file.  Every tool
 * takes its sheets this way, so "-" means the same thing everywhere
 * and the error message is written once. */
loadsheet(fn)
char *fn;
{
	if ( fn[0] == '-' && fn[1] == 0 )
		return loadstdin();
	if ( loadfile(fn) < 0 )
	{
		fprintf(stderr, "%s: cannot open %s\n", velprog, fn);
		return -1;
	}
	return 0;
}
