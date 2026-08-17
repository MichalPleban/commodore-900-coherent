/*
 * zprint.c - a ZView print manager.
 *
 * The spooler it manages is the classical Coherent/V7 lpr one (cmd/lpr):
 * a job is a control file /usr/spool/lpd/cf<N> (N from unique()) whose lines
 * are  D<command line>  L<banner> (the 4th banner is the submitting user)
 * A<file to print>  U<spool file to unlink after>  M<user to notify>; data
 * copied at submission time lives beside it as df<N>.  The daemon /usr/lib/lpd
 * -- started by lpr itself after every submission, not from /etc/rc -- takes
 * the FIRST cf in directory order, prints it to /dev/lp, unlinks it, rescans,
 * and exits removing its lock file  dpid  (which holds its pid) when the
 * directory has no cf left.  So:  dpid present = the daemon is printing, and
 * the first job in directory order is the one coming out of the printer.
 * SIGTRAP makes the daemon abandon the current listing (what lpskip sends);
 * SIGREST makes it start the listing over (lpskip -r).
 *
 * zprint is the lpq/lprm/lpskip of that spooler with a window on it:
 *   - a row of BUTTONS: Print (file-name dialog -> /bin/lpr), Cancel
 *     (SIGTRAP for the job being printed, cf+df removal for a queued one),
 *     and Reprint (SIGREST) -- relabelled Start when the daemon is dead
 *     but jobs remain (a crash, a reboot), because then it starts
 *     /usr/lib/lpd instead -- with the printer state on the right;
 *   - a scrollable LIST of jobs (number, * on the one printing, owner, size,
 *     the submitting command), the selected one shown inverted;
 *   - a scrollable CONTENT pane describing the selected job: owner, when it
 *     was submitted, banners, notification, and every file with its size.
 * Both panes have the common vertical scrollbar (hrsbar) on the LEFT edge.
 * The layout, diff renderer and event loop are zmail's.
 *
 * The spool is polled on a SIGALRM tick, so jobs submitted from a terminal
 * window (or another machine's lpr, if such a thing ever exists) appear by
 * themselves, and finished jobs leave the list as the daemon eats them.
 *
 * Keys: ^P/^N select the previous/next job; ^Z/^V page the content pane;
 * p / c / r press the three buttons.
 */
#include <stdio.h>
#include <types.h>
#include <stat.h>
#include <dir.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrsbar.h"
#include "hrdlg.h"

extern char	*malloc();
extern char	*ctime();
extern long	time();

#define	SPOOLDIR	"/usr/spool/lpd"
#define	LOCKFILE	"/usr/spool/lpd/dpid"
#define	LPRCMD		"/bin/lpr"
#define	LPDCMD		"/usr/lib/lpd"

/* View grid ceilings: the biggest full-screen window at the 8x15 cell. */
#define	MAXROWS	52
#define	MAXCOLS	126

/* The job table. */
#define	MAXJOB	64
#define	NNAME	15		/* a cf file name (DIRSIZ + NUL)          */
#define	NUSER	12
#define	NDESC	64		/* the D (command) line                   */

/* The content pane (the selected job's description). */
#define	MAXCL	120
#define	MAXLL	200

/* Button-bar metrics (UI font, dialog-button chrome). */
#define	BARH	32		/* bar height incl. its bottom rule       */
#define	BTNY	3		/* button top                             */
#define	BTNPAD	10		/* label side padding inside a button     */
#define	BTNGAP	14		/* between buttons                        */
#define	MAXBTN	3

HRAPP	me = { "Printer", "printer.icn", 0, 0, HRF_STRETCH, 0, 0, 0 };

int	mywid;
int	cellw, cellh;		/* terminal-font cell (the two panes)     */
int	fcw, fch;		/* UI-font cell (buttons, bar text)       */
int	xpix;			/* text-grid offset right of the bars     */
int	contw, conth;		/* granted content size, px               */
int	ly0, lrows;		/* list pane: top px, visible rows        */
int	cy0, crows;		/* content pane: top px, visible rows     */
int	cols;			/* text columns in both panes             */

/* ---- the job table, rebuilt by rescan() ---- */
char	jname[MAXJOB][NNAME];	/* control file name "cf<N>"              */
long	jid[MAXJOB];		/* the <N>, for display                   */
char	juser[MAXJOB][NUSER];	/* 4th banner line = submitting user      */
char	jdesc[MAXJOB][NDESC];	/* D line: the submitting command         */
long	jsize[MAXJOB];		/* bytes to print (sum of the A files)    */
int	jnf[MAXJOB];		/* how many A files                       */
long	jtime[MAXJOB];		/* cf mtime = submission time             */
int	njob;
int	dpid;			/* daemon pid, 0 = not running            */
int	seljob = -1;		/* selected job, -1 = none                */
int	ltop;			/* first visible list row                 */
char	selname[NNAME];		/* keep the selection across rescans      */

/* ---- the content pane: the selected job's description ---- */
char	*ln[MAXCL];		/* malloc'd NUL-terminated lines          */
int	nln;			/* line count (always >= 1)               */
int	ctop;			/* first visible content line             */

/* ---- rendering (zmail's diff scheme over both panes) ---- */
char	disp[MAXROWS][MAXCOLS];	/* what is on screen; 0 = needs paint     */
int	hlon;			/* 1 = list highlight painted             */
int	hlrow;			/* list row it is on                      */
int	chromedirty = 1;	/* 1 = repaint bar + rules wholesale      */

HRSBAR	sbl;			/* list scrollbar                         */
HRSBAR	sbc;			/* content scrollbar                      */
int	sblforce = 1, sbcforce = 1;

/* the button bar as laid out by drawbar(), for hit-testing */
int	nbtn;
int	bx[MAXBTN], bw[MAXBTN];
char	*blab[MAXBTN];
int	barmed = -1;		/* armed (pressed) button, -1 = none      */
int	barin;			/* 1 = pointer currently inside it        */

int	pollflag;		/* SIGALRM: time to poll the spool        */

static char vbuf[MAXCOLS];	/* view-row expansion buffer              */

/* ------------------------------------------------------------------ */
/* line storage (zmail's, appending only)                             */
/* ------------------------------------------------------------------ */

static char *
lndup(s, n)
char *s;
{
	register char *p;
	register int i;

	if ( (p = malloc(n + 1)) == 0 )
		return 0;
	for ( i = 0; i < n; i++ )
		p[i] = s[i];
	p[n] = 0;
	return p;
}

static
freebuf()
{
	register int i;

	for ( i = 0; i < nln; i++ )
		free(ln[i]);
	nln = 0;
	return 0;
}

static
addline(s)
char *s;
{
	char *p;

	if ( nln >= MAXCL || (p = lndup(s, strlen(s))) == 0 )
		return -1;
	ln[nln++] = p;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the view: list rows + content rows -> one character grid           */
/* ------------------------------------------------------------------ */

/* Pixel y of view row r: rows 0..lrows-1 are the list, the rest content. */
static
rowy(r)
{
	if ( r < lrows )
		return ly0 + r * cellh;
	return cy0 + (r - lrows) * cellh;
}

/* Text of view row r (cols cells, blank-padded), zmail's vrow shape. */
static char *
vrow(r)
{
	register char *p;
	register int i, c;
	int li;
	char lbuf[MAXCOLS + 16];

	for ( i = 0; i < cols; i++ )
		vbuf[i] = ' ';
	if ( r < lrows )			/* a list line */
	{
		li = ltop + r;
		if ( li < 0 || li >= njob )
		{
			if ( li == 0 && njob == 0 )
			{
				p = "(no print jobs)";
				for ( i = 0; p[i] && i < cols; i++ )
					vbuf[i] = p[i];
			}
			return vbuf;
		}
		sprintf(lbuf, "%5ld %c %-10.10s %7ld  %.60s",
			jid[li],
			(li == 0 && dpid) ? '*' : ' ',
			juser[li], jsize[li], jdesc[li]);
		for ( i = 0; lbuf[i] && i < cols; i++ )
			vbuf[i] = lbuf[i];
		return vbuf;
	}
	li = ctop + (r - lrows);		/* a content line */
	if ( li < 0 || li >= nln )
		return vbuf;
	p = ln[li];
	for ( i = 0; p[i] && i < cols; i++ )
	{
		c = p[i] & 0xff;
		vbuf[i] = (c < 0x20 || c > 0x7e) ? '?' : c;
	}
	return vbuf;
}

static
invalidate()
{
	register int r, c;

	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;
	hlon = 0;
	sblforce = 1;
	sbcforce = 1;
	chromedirty = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the list highlight (XOR, zmail's scheme)                           */
/* ------------------------------------------------------------------ */

static
hldraw()
{
	int r;

	if ( seljob < 0 )
		return 0;
	r = seljob - ltop;
	if ( r < 0 || r >= lrows )
		return 0;
	cl_fillrect(xpix, ly0 + r * cellh, contw, ly0 + (r + 1) * cellh, 2);
	hlon = 1;  hlrow = r;
	return 0;
}

static
hlerase()
{
	if ( hlon )
	{
		cl_fillrect(xpix, ly0 + hlrow * cellh,
			    contw, ly0 + (hlrow + 1) * cellh, 2);
		hlon = 0;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* the button bar                                                     */
/* ------------------------------------------------------------------ */

/* Draw one button: white face, 1px border, 2px drop shadow, centred label
 * (the 9x16 UI glyphs sit 1px high-left in their cell, so centre +1,+1). */
static
drawbtn(i)
{
	int x, w, tx, ty;

	x = bx[i];  w = bw[i];
	cl_fillrect(x, BTNY, x + w, BTNY + DLG_BTNH, 1);
	cl_fillrect(x, BTNY, x + w, BTNY + 1, 0);
	cl_fillrect(x, BTNY + DLG_BTNH - 1, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x, BTNY, x + 1, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + w - 1, BTNY, x + w, BTNY + DLG_BTNH, 0);
	cl_fillrect(x + 2, BTNY + DLG_BTNH, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	cl_fillrect(x + w, BTNY + 2, x + w + 2, BTNY + DLG_BTNH + 2, 0);
	tx = x + (w - strlen(blab[i]) * fcw) / 2 + 1;
	ty = BTNY + (DLG_BTNH - fch) / 2 + 1;
	cl_ptext(SHM_FUI, tx, ty, blab[i]);
	if ( i == barmed && barin )
		cl_fillrect(x + 1, BTNY + 1, x + w - 1, BTNY + DLG_BTNH - 1, 2);
	return 0;
}

/* The whole bar: lay the buttons out, draw them, the printer state on the
 * right, the rule under the bar and the rule between the panes.  The third
 * button says what it will actually do: SIGREST reprints the current
 * listing when the daemon is alive, and with no daemon it starts one for
 * the held queue.  rescan() marks the chrome dirty when the daemon comes
 * or goes, so the label follows the state. */
static
drawbar()
{
	static char *btns[] = { "Print", "Cancel", 0 };
	char sbuf[40];
	register int i;
	int x;

	btns[2] = dpid ? "Reprint" : "Start";
	cl_fillrect(0, 0, contw, BARH - 1, 1);
	cl_fillrect(0, BARH - 1, contw, BARH, 0);
	nbtn = 3;
	x = 6;
	for ( i = 0; i < nbtn; i++ )
	{
		blab[i] = btns[i];
		bx[i] = x;
		bw[i] = strlen(btns[i]) * fcw + 2 * BTNPAD;
		drawbtn(i);
		x += bw[i] + BTNGAP;
	}
	if ( dpid )
		sprintf(sbuf, "printing, %d job%s", njob, njob == 1 ? "" : "s");
	else if ( njob > 0 )
		sprintf(sbuf, "%d job%s held", njob, njob == 1 ? "" : "s");
	else
		strcpy(sbuf, "printer idle");
	i = contw - 8 - strlen(sbuf) * fcw;
	if ( i > x )
		cl_ptext(SHM_FUI, i, BTNY + (DLG_BTNH - fch) / 2 + 1, sbuf);
	cl_fillrect(0, cy0 - 2, contw, cy0 - 1, 0);
	return 0;
}

/* Which button is under content pixel (x,y)?  -1 = none. */
static
btnhit(x, y)
{
	register int i;

	if ( y < BTNY || y >= BTNY + DLG_BTNH )
		return -1;
	for ( i = 0; i < nbtn; i++ )
		if ( x >= bx[i] && x < bx[i] + bw[i] )
			return i;
	return -1;
}

/* ------------------------------------------------------------------ */
/* painting                                                           */
/* ------------------------------------------------------------------ */

static
drawrun(r, c0, c1, vp)
char *vp;
{
	char buf[MAXCOLS + 1];
	int s, e, i, n, y;

	y = rowy(r);
	cl_fillrect(xpix + c0 * cellw, y,
		    xpix + c1 * cellw, y + cellh, 1);
	for ( s = c0; s < c1; s = e )
	{
		while ( s < c1 && vp[s] == ' ' )
			s++;
		if ( s >= c1 )
			break;
		for ( e = s; e < c1 && vp[e] != ' '; e++ )
			;
		n = 0;
		for ( i = s; i < e; i++ )
			buf[n++] = vp[i];
		buf[n] = 0;
		cl_ptext(SHM_FTERM, xpix + s * cellw, y, buf);
	}
	return 0;
}

static
clamptops()
{
	if ( ltop > njob - lrows )
		ltop = njob - lrows;
	if ( ltop < 0 )
		ltop = 0;
	if ( ctop > nln - crows )
		ctop = nln - crows;
	if ( ctop < 0 )
		ctop = 0;
	return 0;
}

static
flush()
{
	register int r, c;
	register char *vp;
	int c0, nrows;

	clamptops();
	cl_begin();
	hlerase();
	if ( chromedirty )
	{
		drawbar();
		chromedirty = 0;
	}
	nrows = lrows + crows;
	for ( r = 0; r < nrows; r++ )
	{
		vp = vrow(r);
		c = 0;
		while ( c < cols )
		{
			if ( vp[c] == disp[r][c] )
			{
				c++;
				continue;
			}
			c0 = c;
			while ( c < cols && vp[c] != disp[r][c] )
			{
				disp[r][c] = vp[c];
				c++;
			}
			drawrun(r, c0, c, vp);
		}
	}
	hldraw();
	sbl.sb_x = 0;
	sbl.sb_y = ly0;
	sbl.sb_h = lrows * cellh;
	sbl.sb_total = njob;
	sbl.sb_page = lrows;
	sbl.sb_pos = ltop;
	hr_sbdraw(&sbl, sblforce);
	sblforce = 0;
	sbc.sb_x = 0;
	sbc.sb_y = cy0;
	sbc.sb_h = crows * cellh;
	sbc.sb_total = nln;
	sbc.sb_page = crows;
	sbc.sb_pos = ctop;
	hr_sbdraw(&sbc, sbcforce);
	sbcforce = 0;
	cl_end();
	return 0;
}

/* Derive the pane layout from the granted content size: the bar, then up to
 * 8 list rows (fewer in a short window, but at least 2), then the content
 * pane in what is left. */
static
layout()
{
	ly0 = BARH + 2;
	lrows = 8;
	while ( lrows > 2 && ly0 + lrows * cellh + 3 + 3 * cellh > conth )
		lrows--;
	cy0 = ly0 + lrows * cellh + 3;
	crows = (conth - cy0) / cellh;
	cols = (contw - xpix) / cellw;
	if ( crows < 1 ) crows = 1;
	if ( lrows + crows > MAXROWS ) crows = MAXROWS - lrows;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( cols < 1 ) cols = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the spool scan                                                     */
/* ------------------------------------------------------------------ */

/* Is the daemon alive?  Its lock file holds its pid; the file can outlive a
 * crashed daemon, so probe the pid too (kill with signal 0 sends nothing). */
static
daemonpid()
{
	int fd, pid;

	pid = 0;
	if ( (fd = open(LOCKFILE, 0)) < 0 )
		return 0;
	if ( read(fd, (char *)&pid, sizeof(pid)) != sizeof(pid) )
		pid = 0;
	close(fd);
	if ( pid != 0 && kill(pid, 0) < 0 )
		pid = 0;
	return pid;
}

/* Parse control file cfname into job slot i.  Returns -1 when the file has
 * vanished (the daemon finished it between the directory read and here). */
static
parsecf(i, cfname)
char *cfname;
{
	char b[300];
	char path[320];
	register char *p;
	register FILE *fp;
	int nban;
	struct stat sb;

	sprintf(path, "%s/%s", SPOOLDIR, cfname);
	if ( (fp = fopen(path, "r")) == 0 )
		return -1;
	strcpy(jname[i], cfname);
	jid[i] = 0;
	for ( p = cfname + 2; *p >= '0' && *p <= '9'; p++ )
		jid[i] = jid[i] * 10 + (*p - '0');
	juser[i][0] = 0;
	jdesc[i][0] = 0;
	jsize[i] = 0;
	jnf[i] = 0;
	jtime[i] = 0;
	if ( fstat(fileno(fp), &sb) == 0 )
		jtime[i] = sb.st_mtime;
	nban = 0;
	while ( fgets(b, sizeof(b), fp) != 0 )
	{
		for ( p = b; *p && *p != '\n'; p++ )
			;
		*p = 0;
		switch ( b[0] )
		{
		case 'D':			/* the submitting command */
			sprintf(jdesc[i], "%.60s", b + 1);
			break;

		case 'L':			/* banners; the 4th is the user */
			if ( nban++ == 3 )
				sprintf(juser[i], "%.10s", b + 1);
			break;

		case 'A':			/* a file to print */
			jnf[i]++;
			if ( b[1] == '/' )
				strcpy(path, b + 1);
			else
				sprintf(path, "%s/%.20s", SPOOLDIR, b + 1);
			if ( stat(path, &sb) == 0 )
				jsize[i] += sb.st_size;
			break;
		}
	}
	fclose(fp);
	if ( juser[i][0] == 0 )
		strcpy(juser[i], "?");
	return 0;
}

/* Scan the spool directory into the job table, in DIRECTORY ORDER -- which
 * is the order the daemon takes them.  Selection is kept by control-file
 * name.  Returns 1 when anything the list shows has changed. */
static
rescan()
{
	static char oname[MAXJOB][NNAME];
	static int onjob = -1, odpid;
	char nm[NNAME];
	struct direct dir;
	register FILE *dirfile;
	register int i;
	int changed;

	odpid = dpid;
	for ( i = 0; i < njob; i++ )
		strcpy(oname[i], jname[i]);
	if ( onjob < 0 )
		onjob = 0;
	else
		onjob = njob;

	njob = 0;
	dpid = daemonpid();
	if ( (dirfile = fopen(SPOOLDIR, "r")) != 0 )
	{
		while ( njob < MAXJOB &&
			fread((char *)&dir, sizeof(dir), 1, dirfile) == 1 )
		{
			if ( dir.d_ino == 0
			  || dir.d_name[0] != 'c' || dir.d_name[1] != 'f' )
				continue;
			for ( i = 0; i < DIRSIZ; i++ )
				nm[i] = dir.d_name[i];
			nm[DIRSIZ] = 0;
			if ( parsecf(njob, nm) == 0 )
				njob++;
		}
		fclose(dirfile);
	}

	changed = (njob != onjob) || (dpid != 0) != (odpid != 0);
	for ( i = 0; !changed && i < njob; i++ )
		if ( strcmp(jname[i], oname[i]) != 0 )
			changed = 1;

	/* keep the selection on the same job if it is still there */
	seljob = -1;
	for ( i = 0; i < njob; i++ )
		if ( strcmp(jname[i], selname) == 0 )
		{
			seljob = i;
			break;
		}
	if ( seljob < 0 && njob > 0 )
	{
		seljob = 0;
		strcpy(selname, jname[0]);
	}
	if ( seljob < 0 )
		selname[0] = 0;
	if ( changed )
		chromedirty = 1;
	return changed;
}

/* Load the selected job's description into the content pane. */
static
viewjob(i)
{
	char b[300], o[MAXLL + 1];
	char path[320];
	register char *p;
	register FILE *fp;
	int nban;
	long sz;
	struct stat sb;

	freebuf();
	ctop = 0;
	if ( i < 0 || i >= njob )
	{
		addline("");
		return 0;
	}
	sprintf(o, "Job %ld  (%s)%s", jid[i], jname[i],
		(i == 0 && dpid) ? "  -- PRINTING" : "");
	addline(o);
	addline("");
	sprintf(o, "Owner:      %s", juser[i]);
	addline(o);
	if ( jtime[i] != 0 )
	{
		p = ctime(&jtime[i]);
		p[24] = 0;
		sprintf(o, "Submitted:  %s", p);
		addline(o);
	}
	if ( jdesc[i][0] )
	{
		sprintf(o, "Command:    %s", jdesc[i]);
		addline(o);
	}
	sprintf(path, "%s/%s", SPOOLDIR, jname[i]);
	if ( (fp = fopen(path, "r")) == 0 )
		return 0;
	nban = 0;
	while ( fgets(b, sizeof(b), fp) != 0 )
	{
		for ( p = b; *p && *p != '\n'; p++ )
			;
		*p = 0;
		switch ( b[0] )
		{
		case 'L':			/* the user's own banners */
			if ( ++nban > 4 && b[1] )
			{
				sprintf(o, "Banner:     %.40s", b + 1);
				addline(o);
			}
			break;

		case 'M':
			sprintf(o, "Notify:     %.20s (by mail when done)", b + 1);
			addline(o);
			break;
		}
	}
	addline("");
	sprintf(o, "%d file%s to print:", jnf[i], jnf[i] == 1 ? "" : "s");
	addline(o);
	rewind(fp);
	while ( fgets(b, sizeof(b), fp) != 0 )
	{
		for ( p = b; *p && *p != '\n'; p++ )
			;
		*p = 0;
		if ( b[0] != 'A' )
			continue;
		if ( b[1] == '/' )
			strcpy(path, b + 1);
		else
			sprintf(path, "%s/%.20s", SPOOLDIR, b + 1);
		sz = -1;
		if ( stat(path, &sb) == 0 )
			sz = sb.st_size;
		if ( b[1] == '/' )
			sprintf(o, "    %-40.40s %8ld bytes", b + 1, sz);
		else
			sprintf(o, "    (typed-in data) %-24.24s %8ld bytes",
				b + 1, sz);
		addline(o);
	}
	fclose(fp);
	return 0;
}

static
select(i)
{
	if ( i < 0 || i >= njob )
		return 0;
	seljob = i;
	strcpy(selname, jname[i]);
	if ( seljob < ltop )
		ltop = seljob;
	if ( seljob >= ltop + lrows )
		ltop = seljob - lrows + 1;
	viewjob(seljob);
	return 0;
}

/* ------------------------------------------------------------------ */
/* dialogs (zmail's notice/confirm)                                   */
/* ------------------------------------------------------------------ */

char	dmsg[44];

HRWIDGET mwg[] = {
    { DW_LABEL,   12,  16,   0,  0, dmsg },
    { DW_BUTTON, 150,  52,  70, DLG_BTNH, "OK", 0, 0, (char *)0, 0,
      DWF_DEF | DWF_CANCEL | DWF_END },
};
#define	NMWG	(sizeof(mwg) / sizeof(mwg[0]))

/* A one-line notice with an OK button. */
static
notice(msg)
char *msg;
{
	int w, h, r;

	strncpy(dmsg, msg, sizeof(dmsg) - 1);
	dmsg[sizeof(dmsg) - 1] = 0;
	w = strlen(dmsg) * 9 + 2 * DLG_MARG;
	if ( w < 240 ) w = 240;
	if ( w > 420 ) w = 420;
	h = 96;
	mwg[1].dw_x = (w - 70) / 2;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(mwg, NMWG);
	r = hr_dlgrun(mwg, NMWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return 0;
}

char	cfmsg[40];

HRWIDGET cwg[] = {
    { DW_LABEL,   12,  16,   0,  0, cfmsg },
    { DW_BUTTON,  40,  56,  90, DLG_BTNH, "OK",     0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 170,  56,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NCWG	(sizeof(cwg) / sizeof(cwg[0]))

static
confirm(msg)
char *msg;
{
	int w, h, r;

	strcpy(cfmsg, msg);
	w = 300;
	h = 100;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(cwg, NCWG);
	r = hr_dlgrun(cwg, NCWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	return r == 1;
}

/* The Print dialog: a file name and OK/Cancel. */
char	prfile[64];

HRWIDGET pwg[] = {
    { DW_LABEL,   12,  16,   0,  0, "File to print:" },
    { DW_TEXT,    12,  40, 356, 22, (char *)0, 0, 0, prfile, sizeof(prfile) },
    { DW_BUTTON, 100,  76,  90, DLG_BTNH, "Print",  0, 0, (char *)0, 0,
      DWF_DEF | DWF_END },
    { DW_BUTTON, 230,  76,  80, DLG_BTNH, "Cancel", 0, 0, (char *)0, 0,
      DWF_CANCEL | DWF_END },
};
#define	NPWG	(sizeof(pwg) / sizeof(pwg[0]))

/* ------------------------------------------------------------------ */
/* the commands (the buttons)                                         */
/* ------------------------------------------------------------------ */

/* Run a program and forget it: fork twice so init reaps the worker and
 * zprint never carries a zombie.  The worker drops the window fds. */
static
runcmd(cmd, arg)
char *cmd, *arg;
{
	register int fd;
	int pid, st;

	if ( (pid = fork()) == 0 )
	{
		if ( fork() == 0 )
		{
			for ( fd = 3; fd < _NFILE; fd++ )
				close(fd);
			execl(cmd, cmd, arg, (char *)0);
		}
		exit(0);
	}
	if ( pid > 0 )
		while ( wait(&st) >= 0 )
			;
	return 0;
}

static
doprint()
{
	int w, h, r;

	w = 380;
	h = 116;
	r = hr_dlgopen(&w, &h);
	if ( r == -2 )
		exit(0);
	if ( r < 0 )
		return 0;
	hr_dlgdraw(pwg, NPWG);
	r = hr_dlgrun(pwg, NPWG);
	hr_dlgclose();
	if ( r == -1 )
		exit(0);
	if ( r != 2 || prfile[0] == 0 )
		return 0;
	if ( prfile[0] != '/' )
	{
		notice("Give the full path name of the file");
		return 0;
	}
	if ( access(prfile, 4) != 0 )
	{
		notice("Cannot read that file");
		return 0;
	}
	runcmd(LPRCMD, prfile);
	pollflag = 1;			/* the new job shows up at once */
	return 0;
}

/* Remove a QUEUED job: unlink the spooled data the control file says to
 * unlink (its U lines), then the control file itself. */
static
rmjob(i)
{
	char b[300];
	char path[320];
	register char *p;
	register FILE *fp;

	sprintf(path, "%s/%s", SPOOLDIR, jname[i]);
	if ( (fp = fopen(path, "r")) != 0 )
	{
		while ( fgets(b, sizeof(b), fp) != 0 )
		{
			for ( p = b; *p && *p != '\n'; p++ )
				;
			*p = 0;
			if ( b[0] != 'U' )
				continue;
			if ( b[1] == '/' )
				strcpy(path, b + 1);
			else
				sprintf(path, "%s/%.20s", SPOOLDIR, b + 1);
			unlink(path);
		}
		fclose(fp);
	}
	sprintf(path, "%s/%s", SPOOLDIR, jname[i]);
	unlink(path);
	return 0;
}

static
docancel()
{
	if ( seljob < 0 )
		return 0;
	if ( seljob == 0 && dpid )
	{
		/* the job coming out of the printer: what lpskip sends */
		if ( !confirm("Stop printing this job?") )
			return 0;
		kill(dpid, SIGTRAP);
	}
	else
	{
		if ( !confirm("Remove this job?") )
			return 0;
		rmjob(seljob);
	}
	pollflag = 1;
	return 0;
}

static
dorestart()
{
	if ( dpid )
	{
		/* reprint the current listing from the top: lpskip -r */
		if ( !confirm("Reprint this job?") )
			return 0;
		kill(dpid, SIGREST);
	}
	else if ( njob > 0 )
	{
		/* jobs but no daemon: a crash or a reboot left them behind;
		 * lpr only starts the daemon on a NEW submission, so do it */
		if ( !confirm("Start the printer daemon?") )
			return 0;
		runcmd(LPDCMD, (char *)0);
	}
	else
	{
		notice("Nothing to print");
		return 0;
	}
	pollflag = 1;
	return 0;
}

static
dobutton(i)
{
	switch ( i )
	{
	case 0:	doprint();	break;
	case 1:	docancel();	break;
	case 2:	dorestart();	break;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static
pgmove(d)
{
	ctop += (crows > 1 ? crows - 1 : 1) * d;
	clamptops();
	return 0;
}

static
dokey(c)
{
	c &= 0xff;
	switch ( c )
	{
	case 'P'-0x40:	select(seljob - 1);	break;
	case 'N'-0x40:	select(seljob + 1);	break;
	case 'Z'-0x40:
	case 'B'-0x40:	pgmove(-1);		break;
	case 'V'-0x40:
	case 'F'-0x40:
	case ' ':	pgmove(1);		break;
	case 'p':	doprint();		break;
	case 'c':	docancel();		break;
	case 'r':	dorestart();		break;
	}
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

tick()
{
	pollflag = 1;
	signal(SIGALRM, tick);
	alarm(5);
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int need, i;

	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	fcw = hr_font(SHM_FUI)->cellw;
	fch = hr_font(SHM_FUI)->cellh;
	if ( fcw <= 0 ) fcw = 9;
	if ( fch <= 0 ) fch = 16;
	xpix = ((HRSB_W + cellw - 1) / cellw) * cellw;
	me.ha_w = xpix + 72 * cellw;
	me.ha_h = BARH + 2 + 8 * cellh + 3 + 14 * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	layout();

	rescan();
	viewjob(seljob);

	invalidate();
	need = 1;			/* flushed below, or by the first loop
					 * pass if a server overlay is up now */
	cl_refresh();
	if ( cl_mapped() && !cl_frozen() )
	{
		flush();
		need = 0;
	}

	signal(SIGALRM, tick);
	alarm(5);
	for (;;)
	{
		hr_evwait(mywid);
		while ( hr_evget(mywid, (short *)&e) )
		{
			switch ( e.wm_type )
			{
			case E_EXPOSE:
				invalidate();
				need = 1;
				break;

			case E_RESIZE:
				contw = e.wm_arg[0];
				conth = e.wm_arg[1];
				layout();
				clamptops();
				invalidate();
				need = 1;
				break;

			case E_KEY:
				dokey(e.wm_arg[0]);
				need = 1;
				break;

			case E_BUTTON:
				if ( e.wm_arg[2] & EB_LEFT )	/* press */
				{
					if ( (i = btnhit(e.wm_arg[0],
							 e.wm_arg[1])) >= 0 )
					{
						barmed = i;
						barin = 1;
						cl_begin();
						cl_fillrect(bx[i] + 1, BTNY + 1,
						    bx[i] + bw[i] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= ly0 &&
						  e.wm_arg[1] < ly0 + lrows * cellh )
					{
						if ( hr_sbpress(&sbl, e.wm_arg[1]) )
						{
							ltop = sbl.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[0] < xpix &&
						  e.wm_arg[1] >= cy0 )
					{
						if ( hr_sbpress(&sbc, e.wm_arg[1]) )
						{
							ctop = sbc.sb_pos;
							need = 1;
						}
					}
					else if ( e.wm_arg[1] >= ly0 &&
						  e.wm_arg[1] < ly0 + lrows * cellh )
					{
						i = ltop + (e.wm_arg[1] - ly0)
							   / cellh;
						if ( i >= 0 && i < njob )
						{
							select(i);
							need = 1;
						}
					}
				}
				else				/* release */
				{
					if ( barmed >= 0 )
					{
						i = barmed;
						barmed = -1;
						if ( barin )
						{
							cl_begin();
							cl_fillrect(bx[i] + 1,
							    BTNY + 1,
							    bx[i] + bw[i] - 1,
							    BTNY + DLG_BTNH - 1, 2);
							cl_end();
							dobutton(i);
							need = 1;
						}
					}
					else if ( sbl.sb_drag )
						hr_sbrelease(&sbl);
					else if ( sbc.sb_drag )
						hr_sbrelease(&sbc);
				}
				break;

			case E_MOTION:
				if ( barmed >= 0 )
				{
					i = (btnhit(e.wm_arg[0], e.wm_arg[1])
					     == barmed);
					if ( i != barin )
					{
						barin = i;
						cl_begin();
						cl_fillrect(bx[barmed] + 1,
						    BTNY + 1,
						    bx[barmed] + bw[barmed] - 1,
						    BTNY + DLG_BTNH - 1, 2);
						cl_end();
					}
				}
				else if ( sbl.sb_drag )
				{
					if ( hr_sbmotion(&sbl, e.wm_arg[1]) )
					{
						ltop = sbl.sb_pos;
						need = 1;
					}
				}
				else if ( sbc.sb_drag )
				{
					if ( hr_sbmotion(&sbc, e.wm_arg[1]) )
					{
						ctop = sbc.sb_pos;
						need = 1;
					}
				}
				break;

			case E_QUIT:
				exit(0);
			}
		}
		if ( hr_evover(mywid) )		/* fell behind: assume the worst */
		{
			invalidate();
			need = 1;
		}
		if ( pollflag )			/* poll the spool for changes */
		{
			pollflag = 0;
			if ( rescan() )
			{
				viewjob(seljob);
				invalidate();
				need = 1;
			}
		}
		cl_refresh();
		if ( !cl_frozen() && cl_mapped() )
		{
			if ( cl_dropped() )	/* a draw was lost against a freeze */
			{
				invalidate();
				need = 1;
			}
			if ( need )
			{
				flush();
				need = 0;
			}
		}
	}
}
