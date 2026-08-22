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
char	vbuf[VALL];

static char	dlres[128];	/* the helper's one result line           */
static char	widarg[8];

/* Spawn the helper with (kind, a1..a3 -- unused tail args 0) and read its
 * result.  Returns the payload after '=', or 0 on cancel; on "q" (the
 * window died mid-dialog) exits, like the in-process dialogs did.  The
 * autosave alarm is parked across the spawn so it cannot interrupt the
 * pipe read. */
char *
dlgspawn(kind, a1, a2, a3, a4, a5, a6)
char *kind, *a1, *a2, *a3, *a4, *a5, *a6;
{
	char *av[11];
	int p[2], pid, w, n, k, st;
	unsigned left;

	sprintf(widarg, "%d", mywid);
	av[0] = "veldlg";
	av[1] = widarg;
	av[2] = kind;
	av[3] = a1;  av[4] = a2;  av[5] = a3;
	av[6] = a4;  av[7] = a5;  av[8] = a6;
	av[9] = (char *)0;
	for ( n = 3; n < 9; n++ )
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
		exit(0);		/* E_QUIT under the dialog */
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
		p = dlgspawn("file", save ? "1" : "0",
			     save ? fname : "", msg, (char *)0);
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
	strncpy(vbuf, p, VALL - 1);
	vbuf[VALL - 1] = 0;
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

/* The Help dialog: the key list lives in the helper. */
dohelp()
{
	dlgspawn("help", (char *)0);
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
			ddpal = 1;
			break;
		}
		strcpy(msg, "Cannot load that library");
	}
	statdirty = 1;
	return 1;
}

/* The Edit button: open the CURRENT library in the symbol editor (zsym).
 * zfile's launch pattern -- fork twice so init reaps the worker and no
 * zombie is carried; fds 0-4 stay open, 4 being the shared command pipe
 * (wire.h HR_CMDFD) that lets zsym connect and get a window. */
doedit()
{
	register int fd;
	int pid, st;
	char *path;

	path = (nlib && curlib < nlib) ? libpath[curlib] : USERLIB;
	if ( (pid = fork()) == 0 )
	{
		if ( fork() == 0 )
		{
			for ( fd = 5; fd < 20; fd++ )
				close(fd);
			execl("/usr/vellum/bin/symedit", "symedit", path, (char *)0);
			_exit(1);
		}
		exit(0);
	}
	if ( pid > 0 )
		while ( wait(&st) >= 0 )
			;
	return 0;
}
