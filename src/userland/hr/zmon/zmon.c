/*
 * zmon.c - a ZView system monitor.
 *
 * One window, two panes:
 *   - a MEMORY pane on top: a horizontal bar of user memory (used filled
 *     black, free left white) with /bin/mem's full breakdown spelled out
 *     around it -- the totals above the bar (System font), and below it the
 *     rest of mem's figures (kernel/arena/buffers, segments/stacks/system,
 *     shared and what it saves, free/holes/largest; machine and used are
 *     already in the totals line and the bar) as a terminal-font grid of
 *     label/value columns with the values vertically aligned.
 *     The data is /bin/mem's too: walk the kernel's in-core segment queue
 *     (segmq) through a /dev/kmem snapshot of the kalloc arena; the queue
 *     is kept in memory-address order, so the gaps between consecutive
 *     segments are exactly the free holes.  One click is 1 Kb here, so
 *     click counts are Kb directly.
 *   - a scrollable PROCESS list below: pid, user, in-core size, tty, state,
 *     cpu time, the desktop window the process owns (if any) and the command
 *     line.  The kernel data is ps's: walk the process queue (procq) in the
 *     same arena snapshot, and fish each command line out of the process's
 *     own memory through /dev/mem (or /dev/swap when it is swapped out) via
 *     its u-area.  The WINDOW column is the desktop's own view of who is
 *     who: the server-published window list in the shared VRAM tail
 *     (shmem.h SHM_WINLIST, hr_winlist), matched to the walk by pid -- a
 *     minimised window's title carries a trailing '*'.
 * The list has the common vertical scrollbar (hrsbar) on the LEFT edge;
 * the renderer is the zmail/zprint diff scheme, so a 3-second SIGALRM
 * refresh repaints only the cells that actually changed.
 *
 * Keys: ^P/^N scroll the list a line, ^Z/^V (or ^B/^F, space) a page.
 *
 * All of it is read-only: no kill button, no dialogs, nothing to break.
 */
#include <stdio.h>
#include <ctype.h>
#include <const.h>
#include <param.h>		/* NBUF: the disk-buffer figure, like mem */
#include <l.out.h>
#include <pwd.h>
#include <proc.h>
#include <sched.h>
#include <seg.h>
#include <stat.h>
#include <uproc.h>
#include <romconf.h>
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"
#include "hrsbar.h"

extern char	*malloc();

/* View grid ceilings: the biggest full-screen window at the 8x15 cell. */
#define	MAXROWS	52
#define	MAXCOLS	126

#define	ARGSIZE	512		/* argv fished out of the process image   */
#define	MAXP	96		/* process table rows we keep             */
#define	NCMD	64		/* command-line column storage            */

/* The memory pane, px.  The totals line above the bar is the 9x16 System/UI
 * font; the breakdown grid below it is the 8x15 terminal font (label/value
 * columns so the figures line up vertically). */
#define	MEMT1Y	4		/* the totals line (9x16)                 */
#define	MBARY0	24		/* the bar                                */
#define	MBARY1	40
#define	MBARX	8		/* bar inset from both edges              */
#define	MEMT2Y	45		/* the breakdown lines (8x15, 16px apart) */
#define	MEMT3Y	61
#define	MEMT4Y	77
#define	MEMH	96		/* whole pane incl. its bottom rule       */

/*
 * Mapping kernel pointers into our snapshot of the kalloc arena
 * (ps.c's range()/map(), shared by the segment and process walks).
 */
#define	range(p)	((char *)(p) >= (char *)aend && \
			 (char *)(p) < (char *)aend + casize)
#define	map(p)		(&allp[(char *)(p) - (char *)aend])

#define	aprocq		nl[0].n_value
#define	astime		nl[1].n_value
#define	aasize		nl[2].n_value
#define	aend		nl[3].n_value
#define	asegmq		nl[4].n_value
#define	acorebot	nl[5].n_value
#define	acoretop	nl[6].n_value
#define	aromconf	nl[7].n_value

struct nlist nl[] ={
	"procq_",	0,	0,
	"stimer_",	0,	0,
	"asize_",	0,	0,
	"end_",		0,	0,
	"segmq_",	0,	0,
	"corebot_",	0,	0,
	"coretop_",	0,	0,
	"romconf_",	0,	0,
	/* The terminator must be a COMPLETE initializer group: the z8001 PCC
	 * drops a trailing partial group, so a bare "" would leave the array
	 * one element short and nlist() would scan past its end. */
	"",		0,	0
};

HRAPP	me = { "Monitor", "monitor.icn", 0, 0, HRF_STRETCH, 0, 0, 0 };

int	mywid;
int	cellw, cellh;		/* terminal-font cell (the list)          */
int	xpix;			/* list-grid offset right of the bar      */
int	contw, conth;		/* granted content size, px               */
int	hdry;			/* header row top px                      */
int	ly0, lrows;		/* list pane: top px, visible rows        */
int	cols;			/* text columns in the list               */

/* ---- kernel access ---- */
int	kfd = -1;		/* /dev/kmem; -1 = no data (see errmsg)  */
int	mfd = -1;		/* /dev/mem                               */
int	dfd = -1;		/* /dev/swap                              */
char	*allp;			/* arena snapshot                         */
unsigned casize;		/* arena size                             */
PROC	cprocq;			/* process queue head                     */
struct	uproc u;		/* one u-area, for the command line       */
char	argp[ARGSIZE];		/* one argv, ditto                        */
saddr_t	corebot, coretop;	/* user memory bounds, clicks (= Kb)      */
struct	romconf rc;		/* whole-machine RAM bounds               */
char	errmsg[64];		/* why there is no data                   */

/* ---- the process table, rebuilt by snap() ---- */
int	prpid[MAXP];
char	pruser[MAXP][10];
long	prsize[MAXP];		/* in-core Kb; -1 = unreadable            */
char	prtty[MAXP][6];
char	prst[MAXP];
long	prtim[MAXP];		/* utime+stime, HZ ticks                  */
char	prcmd[MAXP][NCMD];
char	prwin[MAXP][14];	/* window title + optional '*', or ""     */
int	nproc;
int	ptop;			/* first visible list row                 */

/* ---- the memory figures, rebuilt by snap() ---- */
unsigned mtotal, mused, mfree, mbig;	/* user Kb: total/used/free/largest */
unsigned mshared, msaved;		/* shared-text Kb and Kb saved      */
unsigned mstack, msyst;			/* stack-segment Kb, system Kb      */
unsigned mmach, mkern;			/* machine Kb, kernel reservation   */
unsigned marena, mbufs;			/* kalloc arena Kb, disk-buffer Kb  */
int	mnseg, mnshr, mnhole;		/* segment / shared / hole counts   */

/* ---- rendering (the zmail/zprint diff scheme) ---- */
char	disp[MAXROWS][MAXCOLS];	/* what is on screen; 0 = needs paint     */
int	chromedirty = 1;	/* 1 = repaint memory pane + header       */
int	memdirty;		/* 1 = the figures changed: redraw only   */
				/* the value lines + bar fill, no backfill */
int	lastuw = -1;		/* bar fill width on screen; -1 = unknown */
HRSBAR	sbl;
int	sblforce = 1;
int	pollflag;		/* SIGALRM: time to resample              */

static char vbuf[MAXCOLS];	/* view-row expansion buffer              */

/* ------------------------------------------------------------------ */
/* reading the kernel                                                  */
/* ------------------------------------------------------------------ */

static
nodata(s)
char *s;
{
	if ( errmsg[0] == 0 )
		sprintf(errmsg, "(%s)", s);
	kfd = -1;
	return 0;
}

/* Read from /dev/kmem: kernel data addresses are 16-bit offsets. */
static
kread(s, bp, n)
long s;
char *bp;
{
	unsigned x;

	x = s;
	s = x;
	if ( kfd < 0 )
		return -1;
	lseek(kfd, s, 0);
	if ( read(kfd, bp, n) != n )
	{
		nodata("kernel memory read error");
		return -1;
	}
	return 0;
}

static
mread(s, bp, n)
long s;
char *bp;
{
	lseek(mfd, s, 0);
	return read(mfd, bp, n) == n ? 0 : -1;
}

static
dread(s, bp, n)
long s;
char *bp;
{
	lseek(dfd, s, 0);
	return read(dfd, bp, n) == n ? 0 : -1;
}

/* Read n bytes at offset s of the segment sp points at (in kernel space),
 * from core or from swap, whichever holds it.  ps.c's segread. */
static
segread(sp, s, bp, n)
SEG *sp;
unsigned s;
char *bp;
{
	register SEG *sp1;

	if ( range((char *)sp) == 0 )
		return 0;
	sp1 = (SEG *) map(sp);
	if ( (sp1->s_flags & SFCORE) != 0 )
	{
		if ( mfd < 0 || mread(ctob((long)sp1->s_mbase) + s, bp, n) < 0 )
			return 0;
	}
	else
	{
		if ( dfd < 0 || dread((long)sp1->s_dbase*BSIZE + s, bp, n) < 0 )
			return 0;
	}
	return 1;
}

/* One-time set-up: namelist, the device files, the arena buffer and the
 * boot-fixed figures.  On failure errmsg says why and snap() never runs. */
static
initdata()
{
	nlist("/coherent", nl);
	if ( nl[0].n_type == 0 )
		return nodata("bad namelist in /coherent");
	if ( (kfd = open("/dev/kmem", 0)) < 0 )
		return nodata("cannot open /dev/kmem");
	mfd = open("/dev/mem", 0);	/* command lines only; optional */
	dfd = open("/dev/swap", 0);
	if ( kread((long)aasize, (char *)&casize, sizeof(casize)) < 0 )
		return 0;
	if ( (allp = malloc(casize)) == NULL )
		return nodata("out of memory");
	kread((long)acorebot, (char *)&corebot, sizeof(corebot));
	kread((long)acoretop, (char *)&coretop, sizeof(coretop));
	kread((long)aromconf, (char *)&rc, sizeof(rc));
	mmach = rc.rom_eram - rc.rom_bram;
	mkern = corebot - rc.rom_bram;
	marena = ((long)casize + 1023) / 1024;
	mbufs = (NBUF * (long)BSIZE) / 1024;
	return 0;
}

/* uid -> name, cached: getpwuid() rereads /etc/passwd every call and this
 * runs for every process every tick. */
static char *
uid2nm(uid)
{
	static struct { int uid; char nm[10]; } uc[16];
	static int nuc;
	register int i;
	register struct passwd *pwp;

	for ( i = 0; i < nuc; i++ )
		if ( uc[i].uid == uid )
			return uc[i].nm;
	if ( nuc >= 16 )
		nuc = 0;
	i = nuc++;
	uc[i].uid = uid;
	if ( (pwp = getpwuid(uid)) != NULL )
		sprintf(uc[i].nm, "%.8s", pwp->pw_name);
	else
		sprintf(uc[i].nm, "%d", uid);
	return uc[i].nm;
}

/* In-core size of a process in Kb (clicks are 1 Kb): ps's psize, without
 * the u-area and auxiliary segments, same as plain ps. */
static long
sizek(pp)
register PROC *pp;
{
	register SEG *sp;
	register int n;
	long k;

	k = 0;
	for ( n = 0; n < NUSEG+1; n++ )
	{
		if ( n == SIUSERP || n == SIAUXIL )
			continue;
		if ( (sp = pp->p_segp[n]) == NULL )
			continue;
		if ( range((char *)sp) == 0 )
			return -1;
		sp = (SEG *) map(sp);
		k += sp->s_size;
	}
	return k;
}

/* ps's state letter: Running, Sleeping, Waiting (on itself), sTopped, Zombie. */
static
statec(pp1, pp2)
register PROC *pp1, *pp2;
{
	register int s;

	s = pp1->p_state;
	if ( s == PSSLEEP )
	{
		if ( (PROC *)pp1->p_event == pp2 )
			return 'W';
		if ( (pp1->p_flags & PFSTOP) != 0 )
			return 'T';
		return 'S';
	}
	if ( s == PSRUN )
		return 'R';
	if ( s == PSDEAD )
		return 'Z';
	return '?';
}

static
ttystr(pp, out)
register PROC *pp;
char *out;
{
	register int d;

	if ( (d = pp->p_ttdev) == NODEV )
	{
		strcpy(out, "-");
		return 0;
	}
	if ( minor(d) < 10 )
		sprintf(out, "%o%d", major(d), minor(d));
	else
		sprintf(out, "%o%c", major(d), 'a'-10+minor(d));
	return 0;
}

/* The command line, fished out of the process image via its u-area:
 * ps's printl, writing into a buffer instead of stdout. */
static
getcmd(pp, out, m)
register PROC *pp;
char *out;
int m;
{
	register char *cp;
	register int c;
	int argc, n, o;
	register SEG *sp;

	out[0] = 0;
	if ( pp->p_state == PSDEAD )
	{
		strcpy(out, "<defunct>");
		return 0;
	}
	if ( pp->p_pid == 0 )
	{
		strcpy(out, "<idle>");
		return 0;
	}
	if ( pp->p_event == astime )
	{
		strcpy(out, "<swap>");
		return 0;
	}
	if ( (pp->p_flags & PFSLIB) != 0 )
	{
		strcpy(out, "<slib>");
		return 0;
	}
	if ( segread(pp->p_segp[SIUSERP], 0, (char *)&u, sizeof(u)) == 0 )
		return 0;
	if ( (sp = pp->p_segp[SISTACK]) == NULL )
		sp = pp->p_segp[SIPDATA];
	n = segread(sp, (unsigned)(u.u_argp - u.u_segl[SISTACK].sr_base),
		argp, sizeof(argp));
	if ( n == 0 || (argc = u.u_argc) <= 0 )
		return 0;
	o = 0;
	cp = argp;
	while ( argc-- )
	{
		while ( (c = *cp++) != '\0' )
		{
			if ( !isascii(c) || !isprint(c) || o >= m-1 )
			{
				out[o] = 0;
				return 0;
			}
			out[o++] = c;
		}
		if ( argc != 0 )
		{
			if ( o >= m-1 )
				break;
			out[o++] = ' ';
		}
	}
	out[o] = 0;
	return 0;
}

/* Resample everything: one arena snapshot, then the process walk (ps) and
 * the segment walk (mem).  Returns 1 when the memory pane's figures moved
 * (the list repaints itself through the diff renderer either way). */
static
snap()
{
	register PROC *pp1, *pp2;
	register SEG *sp;
	SEG shead;
	unsigned ototal, oused, obig, oshared, ostack, osyst;
	int onp, onseg;
	saddr_t prev;
	unsigned gap;

	if ( kfd < 0 )
		return 0;
	ototal = mtotal;  oused = mused;  obig = mbig;  oshared = mshared;
	ostack = mstack;  osyst = msyst;
	onp = nproc;  onseg = mnseg;

	if ( kread((long)aend, allp, (int)casize) < 0 )
		return 1;
	kread((long)aprocq, (char *)&cprocq, sizeof(cprocq));
	kread((long)asegmq, (char *)&shead, sizeof(shead));

	nproc = 0;
	pp1 = &cprocq;
	while ( (pp2 = pp1->p_nback) != (PROC *)aprocq && nproc < MAXP )
	{
		if ( range((char *)pp2) == 0 )
			break;		/* mid-flight change: show what we have */
		pp1 = (PROC *) map(pp2);
		prpid[nproc] = pp1->p_pid;
		sprintf(pruser[nproc], "%.8s", uid2nm(pp1->p_ruid));
		prsize[nproc] = sizek(pp1);
		ttystr(pp1, prtty[nproc]);
		prst[nproc] = statec(pp1, pp2);
		prtim[nproc] = pp1->p_utime + pp1->p_stime;
		getcmd(pp1, prcmd[nproc], NCMD);
		nproc++;
	}

	/* The WINDOW column: one seqlocked snapshot of the server's published
	 * list, then match by pid.  No kernel walk -- hr_winlist reads the
	 * shared tail directly (and answers -1 with no server, leaving every
	 * field blank, so zmon still works started from a bare console). */
	{
		HRWIN wl[HRWL_N];
		register int i, w, o;
		int nwin;

		nwin = hr_winlist(wl);
		for ( i = 0; i < nproc; i++ )
		{
			prwin[i][0] = 0;
			if ( nwin <= 0 )
				continue;
			for ( w = 0; w < HRWL_N; w++ )
				if ( wl[w].ww_used && wl[w].ww_pid == prpid[i] )
				{
					for ( o = 0; o < 12 && wl[w].ww_title[o]; o++ )
						prwin[i][o] = wl[w].ww_title[o];
					if ( wl[w].ww_min )
						prwin[i][o++] = '*';
					prwin[i][o] = 0;
					break;
				}
		}
	}

	mtotal = coretop - corebot;
	mused = mshared = msaved = mstack = msyst = 0;
	mnseg = mnshr = mnhole = 0;
	mbig = 0;
	mfree = 0;
	prev = corebot;
	for ( sp = shead.s_forw; sp != (SEG *)asegmq; sp = sp->s_forw )
	{
		if ( range((char *)sp) == 0 )
			break;
		sp = (SEG *) map(sp);
		mnseg++;
		mused += sp->s_size;
		if ( sp->s_flags & SFSHRX )
		{
			mnshr++;
			mshared += sp->s_size;
			if ( sp->s_urefc > 1 )
				msaved += (sp->s_urefc - 1) * sp->s_size;
		}
		if ( sp->s_flags & SFDOWN )
			mstack += sp->s_size;
		if ( sp->s_flags & SFSYST )
			msyst += sp->s_size;
		if ( sp->s_mbase > prev )
		{
			gap = sp->s_mbase - prev;
			mnhole++;
			if ( gap > mbig )
				mbig = gap;
		}
		prev = sp->s_mbase + sp->s_size;
	}
	if ( coretop > prev )
	{
		gap = coretop - prev;
		mnhole++;
		if ( gap > mbig )
			mbig = gap;
	}
	mfree = mtotal - mused;

	return ototal != mtotal || oused != mused || obig != mbig
	    || oshared != mshared || ostack != mstack || osyst != msyst
	    || onp != nproc || onseg != mnseg;
}

/* ------------------------------------------------------------------ */
/* the view: one text grid over the process list                      */
/* ------------------------------------------------------------------ */

static char hdr[] =
	"  PID USER      SIZE TTY  S  TIME  WINDOW        COMMAND";

/* Text of view row r (cols cells, blank-padded). */
static char *
vrow(r)
{
	register char *p;
	register int i;
	int li;
	long sec, mn;
	char zb[8], tb[8];
	char lbuf[MAXCOLS + NCMD];

	for ( i = 0; i < cols; i++ )
		vbuf[i] = ' ';
	li = ptop + r;
	if ( li < 0 || li >= nproc )
	{
		if ( li == 0 && nproc == 0 )
		{
			p = errmsg[0] ? errmsg : "(no processes)";
			for ( i = 0; p[i] && i < cols; i++ )
				vbuf[i] = p[i];
		}
		return vbuf;
	}
	if ( prsize[li] < 0 )
		strcpy(zb, "   ?K");
	else if ( prsize[li] == 0 )
		strcpy(zb, "    -");
	else
		sprintf(zb, "%4ldK", prsize[li]);
	sec = (prtim[li] + HZ/2) / HZ;
	if ( sec == 0 )
		strcpy(tb, "     ");
	else if ( (mn = sec/60) >= 100 )
		sprintf(tb, "%5ld", mn);
	else
		sprintf(tb, "%2ld:%02ld", mn, sec%60);
	sprintf(lbuf, "%5d %-8.8s %s %-4.4s %c %s  %-13.13s %.63s",
		prpid[li], pruser[li], zb, prtty[li], prst[li], tb,
		prwin[li], prcmd[li]);
	for ( i = 0; lbuf[i] && i < cols; i++ )
		vbuf[i] = lbuf[i];
	return vbuf;
}

static
invalidate()
{
	register int r, c;

	for ( r = 0; r < MAXROWS; r++ )
		for ( c = 0; c < MAXCOLS; c++ )
			disp[r][c] = 0;
	sblforce = 1;
	chromedirty = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* the memory pane + list header (the "chrome")                       */
/* ------------------------------------------------------------------ */

/* Pad string t with trailing blanks to n characters (caller's buffer must
 * hold n+1).  A padded line printed with cl_ptext overwrites the whole band
 * of glyph cells opaquely, so the previous text vanishes under it WITHOUT a
 * white backfill first -- no flash on the 3-second tick. */
static
padline(t, n)
char *t;
{
	register int i;

	for ( i = 0; t[i]; i++ )
		;
	while ( i < n )
		t[i++] = ' ';
	t[i] = 0;
}

/* The value lines and the bar FILL -- everything a resample can change.
 * Text is printed padded to the pane width (opaque glyph cells, see padline);
 * the bar repaints only the strip between the old and new fill widths.  The
 * static chrome (pane background, bar border, header, rules) is drawmem's and
 * is not touched here, so a tick never flashes the pane white. */
static
drawvals()
{
	char t[132];
	register int bx0, bx1, uw;
	int tw, gw;

	tw = (contw - 2 * MBARX) / 9;		/* title cells (9x16 FUI)     */
	gw = (contw - 2 * MBARX) / 8;		/* breakdown cells (8x15 term) */
	if ( tw > 128 ) tw = 128;
	if ( gw > 128 ) gw = 128;
	if ( kfd < 0 )
	{
		strcpy(t, "Memory: no data");
		padline(t, tw);
		cl_ptext(SHM_FUI, MBARX, MEMT1Y, t);
		strcpy(t, errmsg);
		padline(t, gw);
		cl_ptext(SHM_FTERM, MBARX, MEMT2Y, t);
	}
	else
	{
		sprintf(t, "Memory: %uK of %uK used,  %uK free,  %d processes",
			mused, mtotal, mfree, nproc);
		padline(t, tw);
		cl_ptext(SHM_FUI, MBARX, MEMT1Y, t);
		/* mem's figures, as a grid of label/value columns: every value
		 * is a right-justified %5u at the same offset in its column, so
		 * the numbers line up under each other tick after tick (fixed
		 * width, so they need no padding).  machine and used are NOT
		 * here: the totals line and the bar already show them. */
		sprintf(t, "kernel   %5uK   segments %5u    shared   %5uK   free     %5uK",
			mkern, mnseg, mshared, mfree);
		cl_ptext(SHM_FTERM, MBARX, MEMT2Y, t);
		sprintf(t, "arena    %5uK   stacks   %5uK   shr segs %5u    holes    %5u",
			marena, mstack, mnshr, mnhole);
		cl_ptext(SHM_FTERM, MBARX, MEMT3Y, t);
		sprintf(t, "buffers  %5uK   system   %5uK   saving   %5uK   largest  %5uK",
			mbufs, msyst, msaved, mbig);
		cl_ptext(SHM_FTERM, MBARX, MEMT4Y, t);
	}

	/* the bar fill, black from the left: paint only the delta strip */
	bx0 = MBARX;
	bx1 = contw - MBARX;
	if ( bx1 <= bx0 + 2 )
		return 0;
	uw = 0;
	if ( mtotal != 0 )
		uw = (long)(bx1 - bx0 - 2) * mused / mtotal;
	if ( lastuw < 0 )		/* fresh interior (after drawmem) */
	{
		if ( uw > 0 )
			cl_fillrect(bx0 + 1, MBARY0 + 1, bx0 + 1 + uw, MBARY1 - 1, 0);
	}
	else if ( uw > lastuw )
		cl_fillrect(bx0 + 1 + lastuw, MBARY0 + 1, bx0 + 1 + uw, MBARY1 - 1, 0);
	else if ( uw < lastuw )
		cl_fillrect(bx0 + 1 + uw, MBARY0 + 1, bx0 + 1 + lastuw, MBARY1 - 1, 1);
	lastuw = uw;
	return 0;
}

static
drawmem()
{
	register int bx0, bx1;

	cl_fillrect(0, 0, contw, MEMH - 1, 1);
	cl_fillrect(0, MEMH - 1, contw, MEMH, 0);

	/* the bar's 1px border; its fill and all the text are drawvals()'s */
	bx0 = MBARX;
	bx1 = contw - MBARX;
	if ( bx1 > bx0 + 2 )
	{
		cl_fillrect(bx0, MBARY0, bx1, MBARY0 + 1, 0);
		cl_fillrect(bx0, MBARY1 - 1, bx1, MBARY1, 0);
		cl_fillrect(bx0, MBARY0, bx0 + 1, MBARY1, 0);
		cl_fillrect(bx1 - 1, MBARY0, bx1, MBARY1, 0);
	}
	lastuw = -1;			/* interior is freshly white */
	drawvals();

	/* the list header, in the list's own font and alignment.  The band is
	 * backfilled first: cl_ptext repaints only its own glyph cells, so the
	 * space past the last column (and around the rule) kept stale pixels
	 * after an expose.  Same for the slivers right of and below the list
	 * grid, which the diff renderer never touches. */
	cl_fillrect(0, MEMH, contw, ly0, 1);
	cl_ptext(SHM_FTERM, xpix, hdry, hdr);
	cl_fillrect(0, hdry + cellh + 1, contw, hdry + cellh + 2, 0);
	cl_fillrect(xpix + cols * cellw, ly0, contw, conth, 1);
	cl_fillrect(0, ly0 + lrows * cellh, contw, conth, 1);
	return 0;
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

	y = ly0 + r * cellh;
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
clamptop()
{
	if ( ptop > nproc - lrows )
		ptop = nproc - lrows;
	if ( ptop < 0 )
		ptop = 0;
	return 0;
}

static
flush()
{
	register int r, c;
	register char *vp;
	int c0;

	clamptop();
	cl_begin();
	if ( chromedirty )
	{
		drawmem();
		chromedirty = 0;
		memdirty = 0;
	}
	else if ( memdirty )
	{
		drawvals();	/* padded lines + bar delta: no white flash */
		memdirty = 0;
	}
	for ( r = 0; r < lrows; r++ )
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
	sbl.sb_x = 0;
	sbl.sb_y = ly0;
	sbl.sb_h = lrows * cellh;
	sbl.sb_total = nproc;
	sbl.sb_page = lrows;
	sbl.sb_pos = ptop;
	hr_sbdraw(&sbl, sblforce);
	sblforce = 0;
	cl_end();
	return 0;
}

static
layout()
{
	hdry = MEMH + 2;
	ly0 = hdry + cellh + 4;
	lrows = (conth - ly0) / cellh;
	cols = (contw - xpix) / cellw;
	if ( lrows < 1 ) lrows = 1;
	if ( lrows > MAXROWS ) lrows = MAXROWS;
	if ( cols > MAXCOLS ) cols = MAXCOLS;
	if ( cols < 1 ) cols = 1;
	return 0;
}

/* ------------------------------------------------------------------ */
/* keyboard                                                           */
/* ------------------------------------------------------------------ */

static
dokey(c)
{
	c &= 0xff;
	switch ( c )
	{
	case 'P'-0x40:	ptop--;			break;
	case 'N'-0x40:	ptop++;			break;
	case 'Z'-0x40:
	case 'B'-0x40:	ptop -= lrows - 1;	break;
	case 'V'-0x40:
	case 'F'-0x40:
	case ' ':	ptop += lrows - 1;	break;
	}
	clamptop();
	return 0;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

tick()
{
	pollflag = 1;
	signal(SIGALRM, tick);
	alarm(3);
}

main(argc, argv)
char **argv;
{
	WMSG e;
	int need;

	cellw = hr_font(SHM_FTERM)->cellw;
	cellh = hr_font(SHM_FTERM)->cellh;
	if ( cellw <= 0 ) cellw = 8;
	if ( cellh <= 0 ) cellh = 15;
	xpix = ((HRSB_W + cellw - 1) / cellw) * cellw;
	me.ha_w = xpix + 76 * cellw;
	me.ha_h = MEMH + 2 + cellh + 4 + 16 * cellh;
	if ( (mywid = hr_open(&me, &argc, argv)) < 0 )
		exit(1);		/* not running under zview */
	contw = me.ha_w;		/* the size we were GRANTED */
	conth = me.ha_h;
	layout();

	initdata();
	snap();

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
	alarm(3);
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
				clamptop();
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
					if ( e.wm_arg[0] < xpix &&
					     e.wm_arg[1] >= ly0 )
					{
						if ( hr_sbpress(&sbl, e.wm_arg[1]) )
						{
							ptop = sbl.sb_pos;
							need = 1;
						}
					}
				}
				else				/* release */
				{
					if ( sbl.sb_drag )
						hr_sbrelease(&sbl);
				}
				break;

			case E_MOTION:
				if ( sbl.sb_drag )
				{
					if ( hr_sbmotion(&sbl, e.wm_arg[1]) )
					{
						ptop = sbl.sb_pos;
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
		if ( pollflag )			/* resample the kernel */
		{
			pollflag = 0;
			if ( snap() )
				memdirty = 1;	/* values only: no pane backfill */
			need = 1;
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
