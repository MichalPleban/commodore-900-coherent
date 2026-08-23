/*
 * veldlg.c - Vellum's dialog STUBS: every modal dialog runs in the
 * spawned helper /usr/vellum/lib/veldlg (veldlgm.c) on this window --
 * the velxport pattern applied to the dialogs (VELLUM.md sec. 21's
 * escape hatch), which is what keeps the editor's 64 K text segment
 * under the tripwire's 95% with the whole v2 feature set in.
 *
 * The stub forks, points the child's fd 1 at a pipe, execs the helper
 * with this window's id and the dialog's current values in argv, and
 * BLOCKS reading the pipe: while it waits, the helper adopts the window
 * (hr_attach), runs the dialog through the ordinary hrdlg kit, prints
 * one result line and exits.  Result: "" = cancelled, "q" = E_QUIT
 * arrived (the window is gone: exit like the in-process dialogs did),
 * "=..." = the payload.  The APPLY halves (savefile, loadlib, object
 * mutation) stay here -- the helper has no model.
 */
#include <stdio.h>
#include <types.h>
#include <dir.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "vellum.h"

#define	VELDLG	"/usr/vellum/lib/veldlg"

char	nbuf[NAMEL];
char	vbuf[TVMAX];	/* the text dialog carries long labels (v3.3) */

static char	dlres[128];	/* the helper's one result line           */
static char	widarg[8];

/* Spawn the helper with (kind, a1..a3 -- unused tail args 0) and read its
 * result.  Returns the payload after '=', or 0 on cancel; on "q" (the
 * window died mid-dialog) exits, like the in-process dialogs did.  The
 * autosave alarm is parked across the spawn so it cannot interrupt the
 * pipe read. */
char *
dlgspawn(kind, a1, a2, a3, a4, a5, a6, a7, a8, a9)
char *kind, *a1, *a2, *a3, *a4, *a5, *a6, *a7, *a8, *a9;
{
	char *av[14];
	int p[2], pid, w, n, k, st;
	unsigned left;

	sprintf(widarg, "%d", mywid);
	av[0] = "veldlg";
	av[1] = widarg;
	av[2] = kind;
	av[3] = a1;  av[4] = a2;  av[5] = a3;
	av[6] = a4;  av[7] = a5;  av[8] = a6;
	av[9] = a7;  av[10] = a8;  av[11] = a9;
	av[12] = (char *)0;
	for ( n = 3; n < 12; n++ )
		if ( av[n] == (char *)0 )
			break;
	av[n] = (char *)0;
	dlres[0] = 0;
	if ( pipe(p) < 0 )
		return (char *)0;
	left = alarm(0);
	if ( (pid = fork()) == 0 )
	{
		close(p[0]);
		close(1);
		dup(p[1]);
		close(p[1]);
		execv(VELDLG, av);
		_exit(1);
	}
	close(p[1]);
	n = 0;
	if ( pid > 0 )
	{
		while ( n < sizeof(dlres) - 1 &&
			(k = read(p[0], dlres + n, sizeof(dlres) - 1 - n)) > 0 )
			n += k;
		close(p[0]);
		while ( (w = wait(&st)) >= 0 && w != pid )
			;
	}
	else
		close(p[0]);
	alarm(left ? left : 300);
	while ( n > 0 && (dlres[n - 1] == '\n' || dlres[n - 1] == '\r') )
		n--;
	dlres[n] = 0;
	if ( dlres[0] == 'q' && dlres[1] == 0 )
	{
		vpkill();		/* E_QUIT under the dialog */
		exit(0);
	}
	if ( dlres[0] != '=' && dlres[0] != 'y' )
		return (char *)0;
	return dlres + 1;
}

/* The file-name dialog, shared by Open and Save: a refused attempt
 * respawns the dialog with the error on its message line, so the UX of
 * the old in-dialog retry survives the split. */
filedlg(save)
{
	register char *p;
	register int i;
	char msg[28];

	msg[0] = 0;
	for (;;)
	{
		p = dlgspawn("file", save ? "1" : "0", fname, msg, (char *)0);
		if ( p == (char *)0 )
		{
			statdirty = 1;
			return 0;
		}
		if ( (save ? savefile(p) : loadfile(p)) == 0 )
			break;
		strcpy(msg, save ? "Cannot write that file"
				 : "Cannot open that file");
	}
	for ( i = 0; (fname[i] = p[i]) != 0; i++ )
		;
	statdirty = 1;
	return 1;
}

/* "Discard unsaved changes?" -- guards New and Open when modified. */
confirm(msg)
char *msg;
{
	return dlgspawn("confirm", msg, (char *)0) != (char *)0;
}

/* Ask for a text string; 1 = OK with vbuf filled. */
textdlg(init)
char *init;
{
	register char *p;

	p = dlgspawn("text", init, (char *)0);
	if ( p == (char *)0 )
		return 0;
	strncpy(vbuf, p, TVMAX - 1);
	vbuf[TVMAX - 1] = 0;
	return vbuf[0] != 0;
}

/* Edit the selected object's properties: a symbol gets the name/value
 * dialog, everything else the STYLE dialog (velcmd.c styledlg). */
propdlg()
{
	register DOBJ *o;
	register char *p, *t;

	if ( selobj < 0 )
		return 0;
	o = &obj[selobj];
	if ( o->o_type != OT_SYM )
	{
		styledlg(selobj);
		return 1;
	}
	p = dlgspawn("prop", o->o_name[0] ? o->o_name : "-",
		     o->o_val[0] ? o->o_val : "-", (char *)0);
	if ( p == (char *)0 )
		return 1;
	for ( t = p; *t && *t != '\t'; t++ )
		;
	if ( *t == '\t' )
		*t++ = 0;
	snapshot();
	dmgobj(selobj);
	strncpy(o->o_name, p, NAMEL - 1);
	o->o_name[NAMEL - 1] = 0;
	strncpy(o->o_val, t, VALL - 1);
	o->o_val[VALL - 1] = 0;
	dmgobj(selobj);
	modified = 1;
	return 1;
}

/* Help: the MANUAL PAGE is the help -- open it in the zman browser
 * (the old key-list dialog duplicated the page and drifted). */
dohelp()
{
	static char *av[] = { "/usr/hr/bin/zman", "vellum", (char *)0 };

	spawn(av);
	return 0;
}

/* The Lib button: the helper shows the scrollable chooser and returns a
 * path; the LOAD stays here (the symbol pools are the model's).  A load
 * that still fails respawns the chooser with the message shown. */
libdlg()
{
	register char *p;
	register int r;
	char msg[28];

	msg[0] = 0;
	for (;;)
	{
		p = dlgspawn("lib", msg, (char *)0);
		if ( p == (char *)0 )
			break;
		if ( (r = loadlib(p)) >= 0 )
		{
			curlib = r;
			palview();
			cursym = -1;
			vpkill();	/* the helper's library list is */
			vpspawn();	/* stale: respawn with ours     */
			ddpal = 1;
			break;
		}
		strcpy(msg, "Cannot load that library");
	}
	statdirty = 1;
	return 1;
}

/* ---- Find (VELLUM.md sec. 27): a case-blind substring over
 * designators, values, text and net names; each Find steps to the NEXT
 * hit from the current selection, selects it and pans it to centre.
 * The dialog's second button lists the numbered sheet set and jumps. */

static char	fstr[VALL];	/* the remembered search string */

/* case-blind substring: needle n anywhere in haystack h? */
static
cifind(h, n)
char *h;
register char *n;
{
	register int j;
	int i, c, d;

	for ( i = 0; h[i]; i++ )
	{
		/* case-blind by masking bit 5: folds letters exactly, and
		 * the couple of symbol pairs it also equates ('[' with '{')
		 * are harmless in a drawing search */
		for ( j = 0; n[j]; j++ )
			if ( ((h[i + j] ^ n[j]) & 0xdf) != 0 )
				break;
		if ( n[j] == 0 )
			return 1;
	}
	return 0;
}

static
objmatch(i)
{
	register DOBJ *o;

	o = &obj[i];
	switch ( o->o_type )
	{
	case OT_SYM:
	case OT_NNAME:
		if ( cifind(o->o_name, fstr) )
			return 1;
		if ( o->o_type == OT_NNAME )
			return 0;
	case OT_TEXT:
	case OT_SHAPE:
	case OT_DIM:
		return cifind(oval(o), fstr);
	}
	return 0;
}

finddlg()
{
	register char *p;
	register int i, n;
	int from, x0, y0, x1, y1;
	char pre[FNLEN], suf[8], ns[8];

	/* the helper's Sheets... list needs the set's name parts */
	if ( sheetsplit(pre, &i, suf) )
		sprintf(ns, "%d", i);
	else
	{
		pre[0] = 0;
		suf[0] = 0;
		ns[0] = '0';
		ns[1] = 0;
	}
	p = dlgspawn("find", fstr[0] ? fstr : "-", pre[0] ? pre : "-", ns,
		     suf[0] ? suf : "-", (char *)0);
	if ( p == (char *)0 )
		return 0;
	/* payload: "s N" = go to sheet N; "f STR" = search for STR */
	if ( p[0] == 's' && p[1] == ' ' )
		return sheetto(atoi(p + 2), 0);
	if ( p[0] != 'f' || p[1] != ' ' || p[2] == 0 )
		return 0;
	strncpy(fstr, p + 2, VALL - 1);
	fstr[VALL - 1] = 0;
	if ( nobj == 0 )
		return 0;
	from = (nsel == 1 && selobj >= 0) ? selobj : nobj - 1;
	for ( n = 1; n <= nobj; n++ )
	{
		i = (from + n) % nobj;
		if ( !objmatch(i) )
			continue;
		dmgsel();			/* the old rings come off */
		selclear();
		osel[i] = 1;
		nsel = 1;
		selobj = i;
		objgbox(i, &x0, &y0, &x1, &y1);	/* pan the hit to centre  */
		voxg = (x0 + x1) / 2 - (contw - SBW - PALW) / (2 * gsc);
		voyg = (y0 + y1) / 2 - (conth - STH - SBW - CANY) / (2 * gsc);
		clampvo();
		viewdirty();
		statdirty = 1;
		return 1;
	}
	return 0;
}

/* Launch a worker, zfile's pattern -- fork twice so init reaps it and no
 * zombie is carried; fds 0-4 stay open, 4 being the shared command pipe
 * (wire.h HR_CMDFD) that lets a GUI worker connect and get a window.
 * av is a NULL-terminated argv.  Shared: vellum.c's velpal spawn and the
 * Help/Edit/Print launches all go through here. */
spawn(av)
char **av;
{
	register int fd;
	int pid, st;

	if ( (pid = fork()) == 0 )
	{
		if ( fork() == 0 )
		{
			for ( fd = 5; fd < 20; fd++ )
				close(fd);
			execv(av[0], av);
			_exit(1);
		}
		exit(0);
	}
	if ( pid > 0 )
		while ( wait(&st) >= 0 )
			;
	return 0;
}

/* The Edit button: open the CURRENT library in the symbol editor. */
doedit()
{
	char *av[3];

	av[0] = "/usr/vellum/bin/symedit";
	av[1] = (nlib && curlib < nlib) ? libpath[curlib] : USERLIB;
	av[2] = (char *)0;
	spawn(av);
	return 0;
}

/* Array duplicate (sec. 30): the helper asks nx / ny / pitch, the
 * doarray loop in vellum.c does the copying. */
arraydlg()
{
	register char *p, *t;
	int nx, ny, pt;

	if ( nsel == 0 )
		return 0;
	p = dlgspawn("array", (char *)0);
	if ( p == (char *)0 )
		return 0;
	{
		int v[3];
		register int i;

		v[0] = v[1] = 1;
		v[2] = 2;
		for ( i = 0; i < 3 && (t = tok(&p)) != 0; i++ )
			v[i] = atoi(t);
		if ( v[2] < 1 )
			v[2] = 1;
		return doarray(v[0], v[1], v[2]);
	}
}

/* Print from the board (VELLUM.md sec. 25): the helper's Print dialog
 * (scale, wide, Preview / Print / Cancel), then the doedit-shaped spawn:
 * Print pipes the SNAPSHOT (the live drawing written to a /tmp file, so
 * an unsaved buffer prints as shown) through velxport into lpr -- zprint
 * owns the job from there; Preview opens velprev on the same snapshot. */
printdlg()
{
	register char *p, *t;
	static char psc[6] = "8";	/* the dialog remembers its last run */
	static int pwide;
	char tmp[24], cmd[128];
	char *av[5];
	int act;

	p = dlgspawn("print", psc, pwide ? "1" : "0", (char *)0);
	if ( p == (char *)0 )
		return 0;
	/* payload: SCALE WIDE ACT (1 = Print, 2 = Preview) */
	act = 0;
	if ( (t = tok(&p)) != 0 && t[0] )
	{
		strncpy(psc, t, 5);
		psc[5] = 0;
	}
	if ( (t = tok(&p)) != 0 )
		pwide = atoi(t) != 0;
	if ( (t = tok(&p)) != 0 )
		act = atoi(t);
	if ( act == 0 )
		return 0;
	sprintf(tmp, "/tmp/.velpr%d.d", getpid());
	if ( writefile(tmp) < 0 )
		return 0;
	av[0] = "/bin/sh";
	av[1] = "-c";
	av[2] = cmd;
	av[3] = (char *)0;
	if ( act == 2 )
		sprintf(cmd, "/usr/vellum/lib/velprev %s; rm -f %s", tmp, tmp);
	else
		sprintf(cmd,
		    "/usr/vellum/lib/velxport -print%s -scale %s %s | /bin/lpr; rm -f %s",
		    pwide ? " -wide" : "", psc, tmp, tmp);
	spawn(av);
	statdirty = 1;
	return 1;
}
