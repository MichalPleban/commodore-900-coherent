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
 * One icon per entry of /usr/hr/etc/apps (the same catalog the server's menu
 * reads; the icon and X,Y fields are ours).  EVERY icon is ALWAYS visible --
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
 * /usr/hr/etc/apps file keeps.  The list's seqlock generation is polled on a
 * 2-second alarm, so a press/release of an icon follows the app's windows by
 * at most that; the poll is one shared-memory word, no system call.
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
	       HRF_NODECOR | HRF_POS, 0, 0, 0 };

/* ---- the catalog (/usr/hr/etc/apps) -------------------------------------- *
 * Same file, same line format as the server: name:execpath:multi:icon:X,Y.
 * The server reads the first three fields (its menu); the icon and launch
 * position are ours.  Fixed-size bss, like the server's table. */
#define MAX_APPS	12
struct app {
	char	name[16];	/* catalog name == the app's title base   */
	char	path[40];	/* executable                             */
	int	multi;		/* 1 = middle click may start more copies */
	char	icon[20];	/* .icn under /usr/hr/icons               */
	int	px, py;		/* -P for a launch; -1,-1 = server places */
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

/* One catalog line -> an apps[] entry (comments/blanks skipped). */
static
appline(p)
char *p;
{
	char *fld[5];
	char *f;
	int nf;
	struct app *a;

	if ( !*p || *p == '#' || napps >= MAX_APPS )
		return;
	nf = 0;
	fld[nf++] = p;
	f = p;
	while ( nf < 5 )
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
	fd = open("/usr/hr/etc/apps", 0);
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

/* Does window-list title `t' belong to catalog entry `ai'?  The displayed
 * title is the app's base name, or "base #N" when several instances are open
 * -- strip at " #" and compare with the catalog name (they are the same
 * string by convention, both truncated to 15 chars). */
static
titlematch(t, ai)
char *t;
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
	return strcmp(b, apps[ai].name) == 0;
}

/* Windows of catalog entry `ai' in the snapshot wl[]: returns the count and
 * puts a representative window id in *pwid (any one; the server resolves
 * which of the app's windows is topmost when we C_ACTIVATE it). */
static
appwins(wl, ai, pwid)
HRWIN wl[];
int *pwid;
{
	register int w, n;

	n = 0;
	*pwid = -1;
	for ( w = 0; w < HRWL_N; w++ )
		if ( wl[w].ww_used && titlematch(wl[w].ww_title, ai) )
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
 * back a different child of ours; drop whichever slot it names. */
static
reapkids()
{
	register int i, w;
	int st;

	for ( i = 0; i < NKIDS; i++ )
		if ( kids[i].pid > 0 && kill(kids[i].pid, 0) < 0 )
		{
			w = wait(&st);
			if ( w < 0 )
				w = kids[i].pid;
			for ( i = 0; i < NKIDS; i++ )
				if ( kids[i].pid == w )
				{
					kids[i].pid = 0;
					break;
				}
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

	if ( cl_frozen() )	/* a server menu/overlay is up: don't paint over it */
		return;
	cl_begin();
	cl_fillrect(0, 0, DOCKW, DOCKH - 1, 1);		/* the bar */
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
		r = (appwins(wl, i, &w) > 0) || pending(i);
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

/* Start catalog entry `ai'.  The child inherits the command pipe on HR_CMDFD
 * across the exec (like a launch from the rc script); the catalog's X,Y goes
 * on the command line as -P, which every GUI app parses in hr_open(). */
static
launch(ai)
{
	char pbuf[16];
	register struct app *a;
	int pid, i;

	a = &apps[ai];
	pid = fork();
	if ( pid == 0 )
	{
		int f;

		for ( f = 5; f < 20; f++ )
			close(f);
		if ( a->px >= 0 )
		{
			sprintf(pbuf, "%d,%d", a->px, a->py);
			execl(a->path, a->name, "-P", pbuf, (char *)0);
		}
		else
			execl(a->path, a->name, (char *)0);
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

/* A button press in our content.  Left: launch, or switch to the running
 * app's topmost window.  Middle: another copy of a multi app (on a single
 * app it behaves like left -- there is nothing else it could mean). */
static
doclick(x, y, mid)
{
	HRWIN wl[HRWL_N];
	int i, w, n;

	if ( (i = cellhit(x, y)) < 0 )
		return;
	if ( hr_winlist(wl) < 0 )
		return;
	n = appwins(wl, i, &w);
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
				exit(0);
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
			repaint();
			needfull = 0;
		}
	}
}
