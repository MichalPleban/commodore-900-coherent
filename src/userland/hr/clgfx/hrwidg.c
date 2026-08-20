/*
 * hrwidg.c - dock-widget client start-up (see inc/hrwidg.h for the contract).
 *
 * The widget half of zdock's widget cells.  zdock forks the widget with
 * "-W wid,x0,y0,x1,y1,dockpid"; hr_wopen() parses that, drops the inherited
 * server command pipe (a widget never writes commands -- and the 20-slot fd
 * table is tight), and enters clgfx sub-surface mode (cl_subinit), after
 * which every cl_* primitive draws into the cell with (0,0) = cell corner,
 * clipped by the dock window's published visible rects.
 *
 * hr_wlive() is the liveness gate a widget must test every tick: the dock
 * pid travels in argv because this libc has no getppid(), and the PID check
 * (not just ww_used) closes the wid-reuse hole -- after an unclean dock
 * death the server reaps the window and may hand the same wid to a stranger,
 * whose pixels a stale widget must not touch.
 *
 * Shipped in libhrgfx.sl next to hrapp.o/hrwl.o.
 */
#include "wire.h"
#include "shmem.h"
#include "clgfx.h"
#include "hrwidg.h"

static int	dockwid = -1;
static int	dockpid;

hr_wdockwid()
{
	return dockwid;
}

/* Parse n comma-separated non-negative decimals; 1 = exactly that, 0 = not. */
static
nums(s, v, n)
register char *s;
register int *v;
int n;
{
	register int a;

	while ( n-- > 0 )
	{
		if ( *s < '0' || *s > '9' )
			return 0;
		a = 0;
		while ( *s >= '0' && *s <= '9' )
			a = a * 10 + (*s++ - '0');
		*v++ = a;
		if ( n > 0 && *s++ != ',' )
			return 0;
	}
	return *s == 0;
}

/*
 * Find and consume "-W wid,x0,y0,x1,y1,dockpid" (argv compacted like hrapp.c
 * guiargs, argc updated) and set up the cell sub-surface.  Returns 0, or -1
 * when the option is absent or malformed -- the widget was not started by a
 * dock and should exit rather than guess at a cell.
 */
hr_wopen(pargc, argv)
int *pargc;
char **argv;
{
	register int i, n;
	int v[6], got;

	got = 0;
	n = 1;
	for ( i = 1; i < *pargc; i++ )
	{
		if ( !got && !strcmp(argv[i], "-W") && i + 1 < *pargc &&
		     nums(argv[i+1], v, 6) )
		{
			got = 1;
			i++;
		}
		else
			argv[n++] = argv[i];
	}
	argv[n] = (char *)0;
	*pargc = n;
	if ( !got )
		return -1;
	dockwid = v[0];
	dockpid = v[5];
	close(HR_CMDFD);
	cl_subinit(dockwid, v[1], v[2], v[3], v[4]);
	return 0;
}

/* 1 while the dock still owns the host window (see the header comment). */
hr_wlive()
{
	HRWIN wl[HRWL_N];

	if ( dockwid < 0 || hr_winlist(wl) < 0 )
		return 0;
	return wl[dockwid].ww_used && wl[dockwid].ww_pid == dockpid;
}
