/*
 * zdock.c - the desktop dock: the icon bar along the top of the screen.
 *
 * This is the "shell" half of the desktop's kernel-and-shell split.  The
 * window server (zview) manages windows and nothing else; WHAT the desktop
 * offers -- the row of application icons, launching, switching -- lives here,
 * in an ordinary replaceable GUI client.  The dock is started from
 * /usr/hr/etc/rc like any resident app, and the desktop survives without it
 * (the server's right-click menu remains as the fallback launcher).
 *
 * The dock's window is UNDECORATED (wire.h HRF_NODECOR): no title bar, no
 * frame, no drop shadow -- a bare strip at (0,0), which the server also
 * exempts from keyboard focus, the window menu and the "Switch to" list.
 *
 * One icon per entry of the dock's own catalog /usr/hr/etc/dock (the server's
 * menu keeps a separate list, /usr/hr/etc/apps -- the two are independent, so
 * the dock can carry fewer icons than the menu).  EVERY icon is ALWAYS visible --
 * unlike the old server-drawn desktop icons, which disappeared while their
 * app ran.  What changes is the look:
 *
 *   not running   the bare 48x48 .icn glyph, the app name beneath;
 *   running       the glyph DROPPED well down in the bar (same X), no
 *                 name -- sunk, like a held-down key.
 *
 * The convention (replacing the old "New <name>" placeholder labels):
 *   left click    none running -> launch it;
 *                 running      -> switch to the app's topmost window
 *                                 (C_ACTIVATE: the server raises it, or
 *                                 restores a hidden one);
 *   middle click  on a multi-instance app -> launch ANOTHER copy.
 *
 * Running-state comes from the shared window list (shmem.h SHM_WINLIST),
 * matched by title base (the " #N" instance suffix stripped) -- the catalog
 * name doubles as the app's declared title, which is the convention the whole
 * /usr/hr/etc/dock file keeps.  We subscribe with HRF_WLNOTIFY, so the server
 * sends E_WINCHG the moment the list changes and the press/release of an icon
 * follows the app's windows immediately.  The 2-second alarm poll remains as
 * the FALLBACK -- it is also what reaps a launch that died before it ever
 * made a window (no window, no list change, no event -- but the icon must
 * come back up); the poll is one shared-memory word, no system call.
 *
 * Launched apps are OUR children (they inherit the command pipe across
 * fork/exec, so the server needs nothing from us), and their corpses are ours
 * to reap: each tick probes them with kill(pid, 0) -- this kernel answers
 * ESRCH for a zombie -- and only then calls the wait() that cannot block.
 */
#include <signal.h>
#include "smgr.h"
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"

extern char	*strcpy(), *strncpy(), *strcat();
extern int	strlen(), strcmp(), atoi();

/* ---- geometry ------------------------------------------------------------ *
 * The bar: full screen width, DOCKH tall, a hairline along its bottom edge.
 * Cells of CELLP px hold a bare 48x48 glyph each -- no box, no shadow.  A
 * RUNNING app's glyph is dropped PRESSY px straight down (same X), which is
 * what reads as "pressed"; an idle app's sits high with its name beneath. */
#define DOCKW	1024		/* content width (the whole screen)     */
#define DOCKH	70		/* content height                       */
#define DOCKX0	10		/* left edge of the first glyph cell    */
#define ICONY	4		/* glyph top while idle                 */
#define PRESSY	10		/* extra Y drop while running           */
#define CELLP	64		/* column pitch                         */
#define LBLY	(ICONY + ICONW + 4)	/* label top (sail 6x8)         */
#define ICONW	48		/* .icn glyph ceiling                   */

HRAPP	me = { "Dock", "", DOCKW, DOCKH,
	       HRF_NODECOR | HRF_POS | HRF_WLNOTIFY, 0, 0, 0 };

/* ---- the catalog (/usr/hr/etc/dock) -------------------------------------- *
 * The dock's OWN file (the server's menu reads the separate /usr/hr/etc/apps):
 * one line per icon, name:execpath:multi:icon:X,Y:args -- or a WIDGET line,
 * @name:execpath[:args] (see the widget block below).  Fixed-size bss, like
 * the server's table. */
#define MAX_APPS	12
struct app {
	char	name[16];	/* catalog name == the app's title base   */
	char	path[40];	/* executable                             */
	int	multi;		/* 1 = middle click may start more copies */
	char	icon[20];	/* .icn under /usr/hr/icons               */
	int	px, py;		/* -P for a launch; -1,-1 = server places */
	char	args[40];	/* extra argv words, blank-separated      */
	int	run;		/* current pressed-state (windows or a    */
				/* launch in flight)                      */
	int	iw, ih;		/* loaded glyph size (0 = no artwork)     */
	int	*bits;		/* glyph rows, INVERTED to fb sense       */
} apps[MAX_APPS];
int	napps;

/* Glyph storage: 48x48 at 3 words/row = 144 words per app, one pool slice
 * each -- static, no heap. */
int	iconpool[MAX_APPS][ICONW * 3];

#define HR_DEFICON	"icon0.icn"

/* ---- widget cells (@ lines in the catalog) ------------------------------- *
 * A WIDGET is a small resident program (zwclock, zwmem, ...) that draws live
 * content in an icon-sized cell at the RIGHT end of the bar -- inside OUR
 * window.  We fork it with "-W wid,x0,y0,x1,y1,dockpid" and it enters clgfx
 * sub-surface mode (cl_subinit): its primitives clip to the intersection of
 * our published visible rects and the cell, so it can never paint outside.
 * Catalog line:  @name:execpath[:click][:args]  -- first @ line = rightmost
 * cell.  The click field makes the cell a BUTTON too:  "Title=/path" gives
 * a left click the dock-icon verb for that app (running -> switch to it,
 * else launch it -- so the app's icon can be taken OFF the bar), and "*"
 * asks the server for its "Switch to..." dialog (C_ACTIVATE, negative wid).
 * The repaint contract: repaint() paints the bar AROUND a live widget's
 * cell and never touches its pixels; the widget repaints its cell once a
 * second regardless, so any damage that does reach a cell (an uncover, a
 * failed save-under) heals within a tick.  We must NOT signal a widget to
 * redraw: this kernel's signal() is V7 one-shot (delivery resets the
 * handler to SIG_DFL, and the handler's first act is re-installing
 * itself), so a second, ASYNCHRONOUS SIGALRM source can land in that
 * unhandled window and silently KILL the widget -- cumulatively certain
 * under an interactive desktop.  A widget's only SIGALRM is its own
 * serialized alarm(); a dead widget's cell is whited like the rest. */
#define MAX_WIDG	4
#define WCELLW	60		/* widget cell width                     */
#define WPITCH	64		/* pitch, allocated right-to-left        */
#define WMARG	4		/* gap at the bar's right edge           */
#define WFAILS	5		/* launch attempts before giving up      */
#define WAGERST	10		/* ticks alive that earn a fresh set of  */
				/* attempts: a widget that RAN for a     */
				/* while and died is not a crash loop    */
struct widg {
	char	name[16];	/* catalog name (argv[0], diagnostics)   */
	char	path[40];	/* executable                            */
	char	args[40];	/* extra argv words, blank-separated     */
	char	clkname[16];	/* click: app title base ("" = none)     */
	char	clkpath[40];	/* click: its executable                 */
	int	clksw;		/* click: 1 = the Switch to... dialog    */
	int	pid;		/* 0 = not running                       */
	int	x0, x1;		/* cell, dock-content x range            */
	int	fails;		/* launches so far; give up at WFAILS    */
	int	age;		/* ticks alive since this launch         */
} widgs[MAX_WIDG];
int	nwidgs;

/* kids[].ai tag for a launch started by a widget CLICK (not a catalog app):
 * distinct per widget so pending-style double-click suppression works. */
#define WCLKAI(wi)	(-2 - (wi))

/* ---- children we launched ------------------------------------------------ */
#define NKIDS	(2 * MAX_APPS)
struct kid {
	int	pid;		/* 0 = slot free */
	int	ai;
} kids[NKIDS];

int	mywid;
int	tickflag;
int	lastseq = -1;		/* hr_winseq() we last acted on */

/* ---- catalog loading ----------------------------------------------------- */

/* Load one .icn (word0=w word1=h, then 1bpp rows, bit15 leftmost, 1 = ink)
 * into ap, INVERTING the rows: cl_blit is a straight copy and the framebuffer
 * is 1 = white, so the file's ink-is-1 becomes ink-is-0 (black strokes on a
 * white button), exactly what the server's old L_NSRC blit produced. */
static
loadicn(ap, name)
struct app *ap;
char *name;
{
	char path[64];
	int hdr[2];
	int fd, wpr, nb, i;

	ap->iw = 0;
	if ( !name || !*name || strlen(name) > 40 )
		return 0;
	strcpy(path, "/usr/hr/icons/");
	strcat(path, name);
	if ( (fd = open(path, 0)) < 0 )
		return 0;
	if ( read(fd, (char *)hdr, 4) != 4 ||
	     hdr[0] <= 0 || hdr[0] > ICONW || hdr[1] <= 0 || hdr[1] > ICONW )
	{
		close(fd);
		return 0;
	}
	wpr = (hdr[0] + 15) / 16;
	nb = wpr * hdr[1] * 2;
	if ( read(fd, (char *)ap->bits, nb) != nb )
	{
		close(fd);
		return 0;
	}
	close(fd);
	for ( i = 0; i < wpr * hdr[1]; i++ )
		ap->bits[i] = ~ap->bits[i];
	ap->iw = hdr[0];
	ap->ih = hdr[1];
	return 1;
}

/* One widget catalog line (past the '@') -> a widgs[] entry.  Cells are
 * allocated right-to-left from the bar's right edge; four widgets reach left
 * only to x=768, clear of the last app cell (ends 762). */
static
widgline(p)
char *p;
{
	char *fld[4];
	char *f;
	int nf;
	register struct widg *wg;

	if ( !*p || nwidgs >= MAX_WIDG )
		return;
	nf = 0;
	fld[nf++] = p;
	f = p;
	while ( nf < 4 )
	{
		while ( *f && *f != ':' )
			f++;
		if ( !*f )
			break;
		*f++ = 0;
		fld[nf++] = f;
	}
	if ( nf < 2 || !fld[1][0] )
		return;
	wg = &widgs[nwidgs];
	strncpy(wg->name, fld[0], sizeof(wg->name) - 1);
	strncpy(wg->path, fld[1], sizeof(wg->path) - 1);
	wg->clkname[0] = 0;
	wg->clkpath[0] = 0;
	wg->clksw = 0;
	if ( nf > 2 && fld[2][0] )
	{
		/* the click verb: "*" = Switch to... dialog,
		 * "Title=/path" = the dock-icon verb for that app */
		if ( fld[2][0] == '*' && fld[2][1] == 0 )
			wg->clksw = 1;
		else
		{
			for ( f = fld[2]; *f && *f != '='; f++ )
				;
			if ( *f )
			{
				*f++ = 0;
				strncpy(wg->clkname, fld[2],
					sizeof(wg->clkname) - 1);
				strncpy(wg->clkpath, f,
					sizeof(wg->clkpath) - 1);
			}
		}
	}
	wg->args[0] = 0;
	if ( nf > 3 )
		strncpy(wg->args, fld[3], sizeof(wg->args) - 1);
	wg->pid = 0;
	wg->fails = 0;
	wg->x1 = DOCKW - WMARG - nwidgs * WPITCH;
	wg->x0 = wg->x1 - WCELLW;
	nwidgs++;
}

/* One catalog line -> an apps[] entry (comments/blanks skipped). */
static
appline(p)
char *p;
{
	char *fld[6];
	char *f;
	int nf;
	struct app *a;

	if ( !*p || *p == '#' )
		return;
	if ( *p == '@' )
	{
		widgline(p + 1);
		return;
	}
	if ( napps >= MAX_APPS )
		return;
	nf = 0;
	fld[nf++] = p;
	f = p;
	while ( nf < 6 )
	{
		while ( *f && *f != ':' )
			f++;
		if ( !*f )
			break;
		*f++ = 0;
		fld[nf++] = f;
	}
	if ( nf < 2 )
		return;
	a = &apps[napps];
	strncpy(a->name, fld[0], sizeof(a->name) - 1);
	strncpy(a->path, fld[1], sizeof(a->path) - 1);
	a->multi = (nf > 2) ? atoi(fld[2]) : 0;
	a->px = a->py = -1;
	a->icon[0] = 0;
	a->args[0] = 0;
	if ( nf > 3 && fld[3][0] )
		strncpy(a->icon, fld[3], sizeof(a->icon) - 1);
	if ( nf > 4 )
	{
		char *q;

		a->px = atoi(fld[4]);
		for ( q = fld[4]; *q && *q != ','; q++ )
			;
		if ( *q )
			a->py = atoi(q + 1);
		if ( a->px < 0 || a->py < 0 )
			a->px = a->py = -1;
	}
	if ( nf > 5 )
		strncpy(a->args, fld[5], sizeof(a->args) - 1);
	a->run = 0;
	a->bits = iconpool[napps];
	if ( !loadicn(a, a->icon) )
		loadicn(a, HR_DEFICON);	/* missing artwork: the generic icon */
	napps++;
}

/* Stream the catalog a chunk at a time (same discipline as the server's
 * loadapps: a fixed whole-file buffer once silently truncated it). */
static
loadapps()
{
	int fd, nb, ll;
	char rb[256];
	char line[160];
	register int i;

	napps = 0;
	nwidgs = 0;
	fd = open("/usr/hr/etc/dock", 0);
	if ( fd < 0 )
		return;
	ll = 0;
	while ( (nb = read(fd, rb, sizeof(rb))) > 0 )
	{
		for ( i = 0; i < nb; i++ )
		{
			if ( rb[i] != '\n' )
			{
				if ( ll < sizeof(line) - 1 )
					line[ll++] = rb[i];
				continue;
			}
			line[ll] = 0;
			ll = 0;
			appline(line);
		}
	}
	if ( ll > 0 )
	{
		line[ll] = 0;
		appline(line);
	}
	close(fd);
}

/* ---- running-state from the shared window list --------------------------- */

/* Does window-list title `t' belong to the app named `name'?  The displayed
 * title is the app's base name, or "base #N" when several instances are open
 * -- strip at " #" and compare (catalog names and declared titles are the
 * same string by convention, both truncated to 15 chars). */
static
titlematch(t, name)
char *t, *name;
{
	char b[24];
	register char *p;

	strncpy(b, t, sizeof(b) - 1);
	b[sizeof(b) - 1] = 0;
	for ( p = b; *p; p++ )
		if ( p[0] == ' ' && p[1] == '#' )
		{
			*p = 0;
			break;
		}
	return strcmp(b, name) == 0;
}

/* Windows of the app named `name' in the snapshot wl[]: returns the count
 * and puts a representative window id in *pwid (any one; the server resolves
 * which of the app's windows is topmost when we C_ACTIVATE it). */
static
appwins(wl, name, pwid)
HRWIN wl[];
char *name;
int *pwid;
{
	register int w, n;

	n = 0;
	*pwid = -1;
	for ( w = 0; w < HRWL_N; w++ )
		if ( wl[w].ww_used && titlematch(wl[w].ww_title, name) )
		{
			if ( *pwid < 0 )
				*pwid = w;
			n++;
		}
	return n;
}

/* A launch of `ai' still in flight?  (Forked, alive, but no window yet --
 * clicks are ignored meanwhile, so a double-click starts one copy, not two.) */
static
pending(ai)
{
	register int i;

	for ( i = 0; i < NKIDS; i++ )
		if ( kids[i].pid > 0 && kids[i].ai == ai &&
		     kill(kids[i].pid, 0) >= 0 )
			return 1;
	return 0;
}

/* Probe our launched children and reap AT MOST one zombie per call (the
 * server's reapdead discipline): kill(pid, 0) fails for a dead child, and
 * only then is wait() called -- which then cannot block, though it may hand
 * back a different child of ours (an app OR a widget); drop whichever slot
 * in whichever table it names. */
static
reapkids()
{
	register int i;
	int st, w, dead;

	dead = 0;
	for ( i = 0; i < NKIDS && !dead; i++ )
		if ( kids[i].pid > 0 && kill(kids[i].pid, 0) < 0 )
			dead = kids[i].pid;
	for ( i = 0; i < nwidgs && !dead; i++ )
		if ( widgs[i].pid > 0 && kill(widgs[i].pid, 0) < 0 )
			dead = widgs[i].pid;
	if ( !dead )
		return;
	w = wait(&st);
	if ( w < 0 )
		w = dead;
	for ( i = 0; i < NKIDS; i++ )
		if ( kids[i].pid == w )
		{
			kids[i].pid = 0;
			return;
		}
	for ( i = 0; i < nwidgs; i++ )
		if ( widgs[i].pid == w )
		{
			widgs[i].pid = 0;
			if ( widgs[i].age >= WAGERST )
				widgs[i].fails = 0;	/* ran a while: not a
							 * crash loop, retry  */
			return;
		}
}

/* ---- drawing ------------------------------------------------------------- */

/* One button cell.  The cell column is cleared to the bar's white first, so a
 * state flip (shadow off, label off, glyph shifted) never leaves droppings. */
#define LBLMAX	10		/* label chars that fit a cell (sail is 6 wide) */

static
drawcell(i)
{
	register struct app *a;
	char lab[LBLMAX + 1];
	int bx, ix, iy, n, lx;

	a = &apps[i];
	bx = DOCKX0 + i * CELLP;
	/* Clear the cell column: wide enough to take a 10-char label centred
	 * on the glyph (6px past it either side), short of the neighbour's. */
	cl_fillrect(bx - 6, 0, bx + CELLP - 6, DOCKH - 1, 1);
	/* the glyph: high while idle, dropped straight down (same X) while
	 * running -- the whole of the pressed look */
	iy = ICONY + (a->run ? PRESSY : 0);
	if ( a->iw > 0 )
	{
		ix = bx + (ICONW - a->iw) / 2;
		iy += (ICONW - a->ih) / 2;
		cl_blit(ix, iy, ix + a->iw, iy + a->ih,
			a->bits, (a->iw + 15) / 16);
	}
	if ( !a->run )
	{
		/* the name, centred under the glyph (sail 6x8), truncated to
		 * the cell; the slight spill into the gaps is bar-white anyway */
		n = strlen(a->name);
		if ( n > LBLMAX )
			n = LBLMAX;
		strncpy(lab, a->name, n);
		lab[n] = 0;
		lx = bx + (ICONW - n * 6) / 2;
		cl_ptext(SHM_FICON, lx, LBLY, lab);
	}
}

static
repaint()
{
	register int i;
	int px;

	if ( cl_frozen() )	/* a server menu/overlay is up: don't paint over it */
		return;
	cl_begin();
	/* the bar, painted AROUND each live widget's cell (see the widget
	 * block comment: signalling a widget to redraw a whited cell could
	 * kill it, so its pixels are simply never disturbed).  widgs[] is
	 * rightmost-first, so walk it backwards for left-to-right spans; a
	 * dead widget breaks no span and its cell is whited like the rest. */
	px = 0;
	for ( i = nwidgs - 1; i >= 0; i-- )
	{
		if ( widgs[i].pid <= 0 )
			continue;
		cl_fillrect(px, 0, widgs[i].x0, DOCKH - 1, 1);
		px = widgs[i].x1;
	}
	cl_fillrect(px, 0, DOCKW, DOCKH - 1, 1);
	cl_fillrect(0, DOCKH - 1, DOCKW, DOCKH, 0);	/* bottom hairline */
	for ( i = 0; i < napps; i++ )
		drawcell(i);
	cl_end();
	cl_snapclip();
}

/* Recompute every entry's pressed-state from the window list + our launches;
 * redraw ONLY the cells that flipped -- a click must not flash the whole
 * bar (full=1 skips even those: repaint() is about to draw everything). */
static
syncstate(full)
{
	HRWIN wl[HRWL_N];
	char flip[MAX_APPS];
	int i, w, r, dirty;

	if ( hr_winlist(wl) < 0 )		/* no server session */
		return;
	dirty = 0;
	for ( i = 0; i < napps; i++ )
	{
		r = (appwins(wl, apps[i].name, &w) > 0) || pending(i);
		flip[i] = (r != apps[i].run);
		if ( flip[i] )
		{
			apps[i].run = r;
			dirty = 1;
		}
	}
	if ( full || !dirty )
		return;
	/* No freeze check here: under a server menu/overlay the primitives
	 * drop themselves and raise cl_dropped, so the main loop owes a full
	 * repaint the moment the overlay clears -- the flip is never lost. */
	cl_begin();
	for ( i = 0; i < napps; i++ )
		if ( flip[i] )
			drawcell(i);
	cl_end();
}

/* ---- actions ------------------------------------------------------------- */

/* Ask the server to bring window `wid's application forward. */
static
activate(wid)
{
	WMSG c;
	register int i;

	c.wm_type = C_ACTIVATE;
	c.wm_wid = mywid;
	for ( i = 0; i < WM_NARG; i++ )
		c.wm_arg[i] = 0;
	c.wm_arg[0] = wid;
	write(HR_CMDFD, (char *)&c, sizeof(c));
}

/* Append the catalog's args string to av[] as blank-separated words, starting
 * at av[n]; NULL-terminates av (room for `lim' entries in all).  Scribbles
 * NULs into `s' -- called between fork and exec, on the child's copy. */
static
addargs(s, av, n, lim)
char *s, **av;
{
	for ( ;; )
	{
		while ( *s == ' ' || *s == '\t' )
			s++;
		if ( !*s || n >= lim - 1 )
			break;
		av[n++] = s;
		while ( *s && *s != ' ' && *s != '\t' )
			s++;
		if ( *s )
			*s++ = 0;
	}
	av[n] = 0;
}

/* Start catalog entry `ai'.  The child inherits the command pipe on HR_CMDFD
 * across the exec (like a launch from the rc script); the catalog's X,Y goes
 * on the command line as -P, which every GUI app parses in hr_open(), and
 * the args field follows it, word by word. */
static
launch(ai)
{
	char pbuf[16];
	char *av[10];
	register struct app *a;
	int pid, i;

	a = &apps[ai];
	pid = fork();
	if ( pid == 0 )
	{
		int f, n;

		for ( f = 5; f < 20; f++ )
			close(f);
		n = 0;
		av[n++] = a->name;
		if ( a->px >= 0 )
		{
			sprintf(pbuf, "%d,%d", a->px, a->py);
			av[n++] = "-P";
			av[n++] = pbuf;
		}
		addargs(a->args, av, n, 10);
		execv(a->path, av);
		_exit(1);
	}
	if ( pid < 0 )
		return;
	for ( i = 0; i < NKIDS; i++ )
		if ( kids[i].pid <= 0 )
		{
			kids[i].pid = pid;
			kids[i].ai = ai;
			break;
		}
	/* pressed at once: the launch is in flight (pending), and the button
	 * reading "down" is the acknowledgement the click deserves */
	syncstate(0);
}

/* Start widget `wi'.  Like launch(), but the cell contract goes on the
 * command line as -W (hr_wopen parses it): our wid, the cell rect in our
 * content coords -- the bottom hairline row stays ours -- and our pid, the
 * widget's liveness check (this libc has no getppid).  Counts as one of the
 * WFAILS attempts whether or not the exec sticks. */
static
launchwidg(wi)
{
	char wbuf[40];
	char *av[10];
	register struct widg *wg;
	int pid;

	wg = &widgs[wi];
	wg->fails++;
	sprintf(wbuf, "%d,%d,%d,%d,%d,%d",
		mywid, wg->x0, 0, wg->x1, DOCKH - 1, getpid());
	pid = fork();
	if ( pid == 0 )
	{
		int f, n;

		for ( f = 5; f < 20; f++ )
			close(f);	/* fd 4 stays; hr_wopen drops it */
		n = 0;
		av[n++] = wg->name;
		av[n++] = "-W";
		av[n++] = wbuf;
		addargs(wg->args, av, n, 10);
		execv(wg->path, av);
		_exit(1);
	}
	if ( pid > 0 )
	{
		wg->pid = pid;
		wg->age = 0;
	}
}

/* Tend the widget table, once per tick: age the live ones (reapkids turns
 * a long-enough age into a fresh set of launch attempts), and (re)start
 * every widget that is not running and has attempts left -- the initial
 * launch after connect, and the restart of a crashed one.  A widget that
 * keeps dying young stops being restarted at WFAILS; the bar's white fill
 * keeps its dead cell clean. */
static
tendwidgs()
{
	register int i;

	for ( i = 0; i < nwidgs; i++ )
	{
		if ( widgs[i].pid > 0 )
			widgs[i].age++;
		else if ( widgs[i].fails < WFAILS )
			launchwidg(i);
	}
}

/* We are quitting: take the widgets with us (they would notice the window
 * going via hr_wlive within a tick anyway; this is just prompt). */
static
killwidgs()
{
	register int i;

	for ( i = 0; i < nwidgs; i++ )
		if ( widgs[i].pid > 0 )
			kill(widgs[i].pid, SIGTERM);
}

/* Which icon cell is content point (x,y) in, or -1.  The target spans the
 * glyph's idle AND pressed positions (plus a little slack), so a click lands
 * whichever state the icon is in. */
static
cellhit(x, y)
{
	register int i, bx;

	if ( y < ICONY || y >= ICONY + ICONW + PRESSY )
		return -1;
	for ( i = 0; i < napps; i++ )
	{
		bx = DOCKX0 + i * CELLP;
		if ( x >= bx - 2 && x < bx + ICONW + 2 )
			return i;
	}
	return -1;
}

/* Which widget cell is content point (x,y) in, or -1.  Widget cells span
 * the bar's full height (short of the hairline). */
static
widghit(x, y)
{
	register int i;

	if ( y < 0 || y >= DOCKH - 1 )
		return -1;
	for ( i = 0; i < nwidgs; i++ )
		if ( x >= widgs[i].x0 - 2 && x < widgs[i].x1 + 2 )
			return i;
	return -1;
}

/* A widget-click launch still in flight?  (The widget-cell twin of
 * pending(): keyed by the WCLKAI tag launchclick left in kids[].) */
static
wclkpending(wi)
{
	register int i;

	for ( i = 0; i < NKIDS; i++ )
		if ( kids[i].pid > 0 && kids[i].ai == WCLKAI(wi) &&
		     kill(kids[i].pid, 0) >= 0 )
			return 1;
	return 0;
}

/* Start the app a widget click names (no catalog entry: no -P, no args --
 * the server places the window).  Tagged WCLKAI in kids[] so the corpse is
 * reaped and a double click starts one copy. */
static
launchclick(wi)
{
	char *av[2];
	register struct widg *wg;
	int pid, i;

	wg = &widgs[wi];
	pid = fork();
	if ( pid == 0 )
	{
		int f;

		for ( f = 5; f < 20; f++ )
			close(f);
		av[0] = wg->clkname;
		av[1] = (char *)0;
		execv(wg->clkpath, av);
		_exit(1);
	}
	if ( pid < 0 )
		return;
	for ( i = 0; i < NKIDS; i++ )
		if ( kids[i].pid <= 0 )
		{
			kids[i].pid = pid;
			kids[i].ai = WCLKAI(wi);
			break;
		}
}

/* A left click in widget cell `wi': the catalog's click verb -- ask the
 * server for the Switch to... dialog ("*"), or the dock-icon verb for the
 * named app: switch to it when it runs, launch it when it does not. */
static
dowidgclick(wi)
{
	HRWIN wl[HRWL_N];
	register struct widg *wg;
	int w, n;

	wg = &widgs[wi];
	if ( wg->clksw )
	{
		activate(-1);		/* server: open Switch to... */
		return;
	}
	if ( !wg->clkpath[0] )
		return;
	if ( hr_winlist(wl) < 0 )
		return;
	n = appwins(wl, wg->clkname, &w);
	if ( n > 0 )
		activate(w);
	else if ( !wclkpending(wi) )
		launchclick(wi);
}

/* A button press in our content.  Left: launch, or switch to the running
 * app's topmost window.  Middle: another copy of a multi app (on a single
 * app it behaves like left -- there is nothing else it could mean).  A
 * click in a widget cell runs the widget's click verb instead. */
static
doclick(x, y, mid)
{
	HRWIN wl[HRWL_N];
	int i, w, n;

	if ( (i = cellhit(x, y)) < 0 )
	{
		if ( (i = widghit(x, y)) >= 0 )
			dowidgclick(i);
		return;
	}
	if ( hr_winlist(wl) < 0 )
		return;
	n = appwins(wl, apps[i].name, &w);
	if ( mid && apps[i].multi )
	{
		if ( n == 0 && pending(i) )
			return;			/* first copy still starting */
		launch(i);
		return;
	}
	if ( n > 0 )
		activate(w);
	else if ( !pending(i) )
		launch(i);
}

/* ---- main ---------------------------------------------------------------- */

static
tick()
{
	tickflag = 1;
	signal(SIGALRM, tick);
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int needfull;

	loadapps();
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* no window server */

	syncstate(1);			/* rc may have started siblings first */
	repaint();
	tendwidgs();			/* widgets need mywid on the -W line:
					 * only after the connect */

	signal(SIGALRM, tick);
	alarm(2);

	needfull = 0;
	for (;;)
	{
		/* Block on our event ring; SIGALRM breaks the wait for the
		 * window-list poll (the same shape as zclock's second hand). */
		hr_evwait(hr_wid());
		while ( hr_evget(hr_wid(), (short *)&e) )
		{
			if ( e.wm_type == E_EXPOSE )
				needfull = 1;
			else if ( e.wm_type == E_QUIT )
			{
				killwidgs();
				exit(0);
			}
			else if ( e.wm_type == E_WINCHG )
				tickflag = 1;	/* the server says the list
						 * changed: poll it NOW, not on
						 * the fallback alarm */
			else if ( e.wm_type == E_BUTTON )
			{
				if ( e.wm_arg[2] & e.wm_arg[3] & EB_LEFT )
					doclick(e.wm_arg[0], e.wm_arg[1], 0);
				else if ( e.wm_arg[2] & e.wm_arg[3] & EB_MID )
					doclick(e.wm_arg[0], e.wm_arg[1], 1);
			}
		}
		if ( hr_evover(hr_wid()) )	/* fell behind: assume the worst */
			needfull = 1;

		if ( tickflag )
		{
			tickflag = 0;
			reapkids();
			tendwidgs();	/* restart a crashed widget */
			if ( hr_winseq() != lastseq )
			{
				lastseq = hr_winseq();
				syncstate(needfull);
			}
			alarm(2);
		}

		cl_refresh();
		if ( cl_mapped() && !cl_frozen() &&
		     (needfull || cl_dropped() || cl_uncovered()) )
		{
			repaint();	/* paints around the live widget cells;
					 * a widget heals its own within 1s */
			needfull = 0;
		}
	}
}
