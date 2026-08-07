/*
 * hrapp.c - hrgui client start-up (see inc/hrapp.h for the contract).
 *
 * One function, hr_open(), replaces the old "the server hands the client its
 * window id, size and cell metrics on the command line" arrangement: the client
 * declares what it wants, the server answers with what it got.  That moves every
 * per-application property (title, size, icon, resizability) OUT of the server's
 * catalog file and into the application itself, which is what lets an app be
 * started with a bare argv.
 *
 * hr_open() also parses the options every GUI application shares (-T -I -S -P
 * -H, see hrapp.h) and finds the client's event channel whether it was forked by
 * the server or started from a shell (openev below).  Between them, a desktop
 * layout is a shell script rather than server code.
 *
 * Linked into every GUI client alongside clgfx.o (Makefile CLGFX).
 */
#include <signal.h>
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrapp.h"

#define ACKWAIT		2	/* seconds to wait for one ack attempt */
#define ACKTOTAL	120	/* wall-clock seconds before giving up entirely */

static int	mywid = -1;

hr_wid()
{
	return mywid;
}

/* Copy at most n-1 bytes of s and NUL-terminate (the record is fixed size and
 * goes on the wire, so clear the tail too rather than ship stack garbage). */
static
setstr(d, s, n)
register char *d, *s;
register int n;
{
	while ( --n > 0 && s && *s )
		*d++ = *s++;
	while ( n-- > 0 )
		*d++ = 0;
}

/* Parse "<A><sep><B>", two non-negative decimals ("640x400", "48,40"); returns
 * 1 and stores the pair, or 0 if the string is not exactly that. */
static
pair(s, sep, pa, pb)
register char *s;
int *pa, *pb;
{
	register int a, b;

	if ( *s < '0' || *s > '9' )
		return 0;
	a = 0;
	while ( *s >= '0' && *s <= '9' )
		a = a * 10 + (*s++ - '0');
	if ( *s != sep )
		return 0;
	s++;
	if ( *s < '0' || *s > '9' )
		return 0;
	b = 0;
	while ( *s >= '0' && *s <= '9' )
		b = b * 10 + (*s++ - '0');
	if ( *s )
		return 0;
	*pa = a;
	*pb = b;
	return 1;
}

/* Pull the globally recognised GUI options (hrapp.h) out of argv, applying them
 * over the application's own defaults, and compact the rest down so the caller's
 * argv holds only its own arguments (argc updated, argv[argc] left NULL).  An
 * option whose argument is malformed is left in argv rather than swallowed, so
 * the application sees it and can complain about it. */
static
guiargs(ap, pargc, argv)
HRAPP *ap;
int *pargc;
char **argv;
{
	register int i, n;
	int a, b;

	n = 1;
	for ( i = 1; i < *pargc; i++ )
	{
		if ( !strcmp(argv[i], "-T") && i + 1 < *pargc )
			ap->ha_title = argv[++i];
		else if ( !strcmp(argv[i], "-I") && i + 1 < *pargc )
			ap->ha_icon = argv[++i];
		else if ( !strcmp(argv[i], "-S") && i + 1 < *pargc &&
			  pair(argv[i+1], 'x', &a, &b) )
		{
			/* The size belongs to the application unless it declared
			 * itself resizable: a window whose layout is derived from
			 * its content (zterm's 80x25 grid) cannot honour a size
			 * imposed from outside, so drop it -- quietly, since a
			 * start-up script may well pass -S to a whole row of apps. */
			if ( ap->ha_flags & HRF_STRETCH )
			{
				ap->ha_w = a;
				ap->ha_h = b;
			}
			i++;
		}
		else if ( !strcmp(argv[i], "-P") && i + 1 < *pargc &&
			  pair(argv[i+1], ',', &a, &b) )
		{
			ap->ha_x = a;
			ap->ha_y = b;
			ap->ha_flags |= HRF_POS;
			i++;
		}
		else if ( !strcmp(argv[i], "-H") )
			ap->ha_flags |= HRF_MIN;
		else
			argv[n++] = argv[i];
	}
	argv[n] = (char *)0;
	*pargc = n;
}

/* The connect acknowledgement must not be a bet on one blocking read.  The
 * server writes E_CONNECTED into our event pipe and moves on; if that wakeup is
 * ever missed we sleep in read() forever, holding a window that exists but is
 * never drawn -- exactly how a second client used to be lost, leaving a blank
 * window on the desktop.  So arm a timer around the read: SIGALRM breaks the
 * sleep, and since the bytes are already in the pipe the retry just picks them
 * up.  Costs nothing whenever the wakeup arrives normally. */
static int	ackalrm;

static
onackalrm()
{
	ackalrm = 1;
	signal(SIGALRM, onackalrm);
}

/*
 * Declare our window to the server and wait for it.  Returns the window id, or
 * -1 if there is no server (the inherited pipes are absent or broken) or it
 * refused us (no free window slot).
 */
hr_open(ap, pargc, argv)
HRAPP *ap;
int *pargc;
char **argv;
{
	HRCONN c;
	WMSG e;
	int pid, awid, aw, ah;
	long t0;
	extern long time();

	if ( pargc && argv )
		guiargs(ap, pargc, argv);

	pid = getpid();

	c.hc_type = C_CONNECT;
	c.hc_pid = pid;
	c.hc_w = ap->ha_w;
	c.hc_h = ap->ha_h;
	c.hc_flags = ap->ha_flags;
	c.hc_x = ap->ha_x;
	c.hc_y = ap->ha_y;
	c.hc_menu = ap->ha_menu;	/* our own window-menu entries, HRM_* */
	setstr(c.hc_title, ap->ha_title, HRC_TITLE);
	setstr(c.hc_icon, ap->ha_icon, HRC_ICON);
	if ( write(HR_CMDFD, &c, sizeof(c)) != sizeof(c) )
		return -1;		/* no command pipe: not under zview */

	/* Wait to be acknowledged.  The answer arrives in the shared tail (the ack
	 * table, shmem.h SHM_ACK) because until we are told our window id we have no
	 * ring of our own to listen on.  The server pokes EVQ_CONNECT after every
	 * ack, so this normally wakes the instant we are answered; the alarm is only
	 * a backstop so a missed poke costs a second rather than the whole start-up.
	 *
	 * The give-up limit is WALL-CLOCK, not a loop count: EVQ_CONNECT is one
	 * ring drained by EVERY connecting client at once, so under a burst of
	 * simultaneous starts an evwait can return immediately (another client's
	 * poke, or the shared head/tail torn by a concurrent drain) -- a counted
	 * loop then burns all its tries in an instant and the client dies while
	 * the server is still working through the queue in front of it. */
	signal(SIGALRM, onackalrm);
	t0 = time((long *)0);
	for (;;)
	{
		if ( hr_ackget(pid, &awid, &aw, &ah) )
			break;
		if ( time((long *)0) - t0 >= (long)ACKTOTAL )
			break;
		ackalrm = 0;
		alarm(ACKWAIT);
		hr_evwait(EVQ_CONNECT);
		alarm(0);
		{			/* drain the poke; it carries no information */
			WMSG k;
			while ( hr_evget(EVQ_CONNECT, (short *)&k) )
				;
		}
	}
	signal(SIGALRM, SIG_DFL);
	if ( !hr_ackget(pid, &awid, &aw, &ah) )
		return -1;		/* no server, or it refused us */
	e.wm_arg[0] = awid;
	e.wm_arg[1] = aw;
	e.wm_arg[2] = ah;
	mywid = e.wm_arg[0];
	ap->ha_w = e.wm_arg[1];		/* granted size: may be less than asked */
	ap->ha_h = e.wm_arg[2];
	cl_init(mywid);			/* direct-render: map VRAM + clip descriptor */
	return mywid;
}

/* Tell the server we are done so it reaps the window (the client normally exits
 * right after).  A client that just exits is reaped anyway when its event pipe
 * breaks, but saying so is instant and keeps the desktop tidy. */
hr_bye()
{
	return hr_cmd(C_BYE);
}

/* Send a bare (no-argument) command record for this window.  Factored out of
 * hr_bye so a client can also claim the selection (C_SELOWN) without
 * open-coding the record. */
hr_cmd(type)
{
	WMSG c;
	int i;

	if ( mywid < 0 )
		return -1;
	c.wm_type = type;
	c.wm_wid = mywid;
	for ( i = 0; i < WM_NARG; i++ )
		c.wm_arg[i] = 0;
	return write(HR_CMDFD, &c, sizeof(c));
}
